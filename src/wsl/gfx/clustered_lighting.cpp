#include "clustered_lighting.hpp"

#include "wsl/gfx/shader.hpp"
#include "wsl/log/log.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include "wsl/rsc/shader_loader.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <cstring>

#include <tracy/Tracy.hpp>

namespace wsl
{

namespace gfx
{

namespace
{

/**
 * CPU-side mirror of the Slang `ClusterParams` cbuffer (b0, space3).
 * Must stay in sync with `cluster_build.slang` and `light_cull.slang`.
 */
struct alignas (16) cluster_params
{
  glm::mat4 inverse_projection;
  glm::vec2 screen_size;
  float z_near;
  float z_far;
  glm::uvec3 grid_size;
  uint32_t num_clusters;
  glm::uvec3 cluster_padding;
  glm::mat4 view;
  uint32_t num_point_lights;
  glm::uvec3 light_padding;
};

static_assert (sizeof (cluster_params) % 16 == 0,
               "cluster_params must be a multiple of 16 bytes for std430");

constexpr Uint32 kBuildThreadsX = 4;
constexpr Uint32 kBuildThreadsY = 4;
constexpr Uint32 kBuildThreadsZ = 1;
constexpr Uint32 kCullThreadsX = 64;

/**
 * Six frustum planes (a, b, c, d) where a*x + b*y + c*z + d >= 0
 * for points inside the frustum. Extracted from the view-projection
 * matrix using Gribb-Hartmann. Assumes the standard OpenGL clip-space
 * convention (z in [-1, 1]) which matches glm::perspective.
 */
struct frustum_planes
{
  glm::vec4 planes[6];
};

/** Extracts the six frustum planes from a view-projection matrix. */
frustum_planes
extract_frustum_planes (const glm::mat4 &vp)
{
  // glm::mat4 is column-major: `vp[i]` is the i-th column. Gribb-Hartmann
  // needs rows, so we read them via the transposed indexing.
  const glm::vec4 r0 = glm::vec4 (vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
  const glm::vec4 r1 = glm::vec4 (vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
  const glm::vec4 r2 = glm::vec4 (vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
  const glm::vec4 r3 = glm::vec4 (vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

  frustum_planes out{};
  out.planes[0] = r3 + r0; // Left
  out.planes[1] = r3 - r0; // Right
  out.planes[2] = r3 + r1; // Bottom
  out.planes[3] = r3 - r1; // Top
  out.planes[4] = r3 + r2; // Near
  out.planes[5] = r3 - r2; // Far
  return out;
}

/**
 * Returns true if the sphere (center, radius) intersects the
 * frustum (or is fully inside it). Conservative: a sphere that is just
 * outside the far plane but within epsilon is still reported visible, so
 * the GPU-side cluster AABB test catches the real rejection.
 */
bool
sphere_intersects_frustum (const glm::vec3 &center, float radius,
                           const frustum_planes &frustum)
{
  return std::ranges::all_of (frustum.planes, [&] (const glm::vec4 &plane) {
    const float distance = glm::dot (plane, glm::vec4 (center, 1.0F));
    return distance >= -radius;
  });
}

} // namespace

clustered_lighting::clustered_lighting (gfx::render_context *ctx,
                                        rsc::resource_manager *res_mgr)
    : m_ctx (ctx), m_res_mgr (res_mgr)
{
  if (m_ctx == nullptr || m_ctx->gpu_device == nullptr) {
    return;
  }
  if (m_res_mgr == nullptr) {
    return;
  }

  create_buffers ();
  create_pipelines ();
}

clustered_lighting::~clustered_lighting () { destroy_resources (); }

void
clustered_lighting::destroy_resources ()
{
  if (m_ctx == nullptr || m_ctx->gpu_device == nullptr) {
    return;
  }

  if (m_build_pipe != nullptr) {
    SDL_ReleaseGPUComputePipeline (m_ctx->gpu_device, m_build_pipe);
    m_build_pipe = nullptr;
  }
  if (m_cull_pipe != nullptr) {
    SDL_ReleaseGPUComputePipeline (m_ctx->gpu_device, m_cull_pipe);
    m_cull_pipe = nullptr;
  }

  for (uint32_t i = 0; i < k_cluster_buf_slots; ++i) {
    if (m_cluster_bufs[i] != nullptr) {
      SDL_ReleaseGPUBuffer (m_ctx->gpu_device, m_cluster_bufs[i]);
      m_cluster_bufs[i] = nullptr;
    }
  }
  if (m_light_buf != nullptr) {
    SDL_ReleaseGPUBuffer (m_ctx->gpu_device, m_light_buf);
    m_light_buf = nullptr;
  }
  if (m_light_transfer != nullptr) {
    SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, m_light_transfer);
    m_light_transfer = nullptr;
  }
  m_light_transfer_capacity = 0;
}

void
clustered_lighting::create_buffers ()
{
  const uint32_t num_clusters = m_cfg.grid_x * m_cfg.grid_y * m_cfg.grid_z;

  // ----- cluster AABB + light list buffer (used as RW SSBO + graphics storage)
  // ----- Usage flags must cover every pipeline that touches this buffer:
  //   * cluster_build.slang  : compute RW storage
  //   * light_cull.slang     : compute RW storage
  //   * cube.frag.slang      : graphics R storage (fragment reads the
  //                             cluster AABB + light list to shade)
  // On the Vulkan backend, binding a storage buffer in a graphics pipeline
  // that was created without the matching `GRAPHICS_STORAGE_*` flag is
  // undefined behaviour. Mesa/AMDVK in particular crashes the whole
  // command buffer the next time compute is dispatched, which presents as
  // a segfault inside `SDL_DispatchGPUCompute` even though the real
  // problem is in the prior graphics draw.
  SDL_GPUBufferCreateInfo cb{};
  cb.size = static_cast<Uint32> (sizeof (gpu_cluster) * num_clusters);
  cb.usage = static_cast<SDL_GPUBufferUsageFlags> (
      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
      | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
      | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);

  // Per-slot pool. See the comment in the header for the rationale
  // (avoids a use-after-free on the underlying VkBuffer on AMDVK/Mesa
  // when the buffer is recorded against before SDL_GPU's internal
  // allocator has finished wiring it up).
  for (uint32_t i = 0; i < k_cluster_buf_slots; ++i) {
    m_cluster_bufs[i] = SDL_CreateGPUBuffer (m_ctx->gpu_device, &cb);
    if (m_cluster_bufs[i] == nullptr) {
      wsl::log::gfx ()->error (
          "clustered_lighting: cluster buffer create (slot {}): {}", i,
          SDL_GetError ());
      return;
    }
  }

  // ----- light buffer (read-only SSBO) -----
  SDL_GPUBufferCreateInfo lb{};
  lb.size = static_cast<Uint32> (sizeof (gpu_cluster_light)
                                 * m_cfg.max_point_lights);
  lb.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
  m_light_buf = SDL_CreateGPUBuffer (m_ctx->gpu_device, &lb);
  if (m_light_buf == nullptr) {
    wsl::log::gfx ()->error ("clustered_lighting: light buffer create: {}",
                             SDL_GetError ());
    return;
  }

  // ----- per-frame transfer buffer for light uploads -----
  SDL_GPUTransferBufferCreateInfo tb{};
  tb.size = static_cast<Uint32> (sizeof (gpu_cluster_light)
                                 * m_cfg.max_point_lights);
  tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  m_light_transfer = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tb);
  m_light_transfer_capacity = tb.size;
  if (m_light_transfer == nullptr) {
    wsl::log::gfx ()->error (
        "clustered_lighting: light transfer buffer create: {}",
        SDL_GetError ());
  }
}
namespace
{

} // namespace

void
clustered_lighting::create_pipelines ()
{
  // ----- cluster build compute pipeline -----
  {
    auto id = m_res_mgr->register_shader (
        "engine://compiled_shaders/cluster_build.slang.spv");
    auto handle = m_res_mgr->load (id);
    if (!handle || handle->bytecode.empty ()) {
      wsl::log::gfx ()->error (
          "clustered_lighting: cluster_build bytecode load failed");
      return;
    }

    SDL_GPUComputePipelineCreateInfo pi{};
    pi.code = handle->bytecode.data ();
    pi.code_size = handle->bytecode.size ();
    pi.format = gfx::shader::native_format ();
    pi.entrypoint = "csMain";
    pi.num_samplers = 0;
    pi.num_readonly_storage_textures = 0;
    pi.num_readonly_storage_buffers = 0;
    pi.num_readwrite_storage_textures = 0;
    pi.num_readwrite_storage_buffers = 1; // cluster buffer (RW)
    pi.num_uniform_buffers = 1;           // ClusterParams
    pi.threadcount_x = kBuildThreadsX;
    pi.threadcount_y = kBuildThreadsY;
    pi.threadcount_z = kBuildThreadsZ;
    pi.props = 0;

    m_build_pipe = SDL_CreateGPUComputePipeline (m_ctx->gpu_device, &pi);
    if (m_build_pipe == nullptr) {
      wsl::log::gfx ()->error (
          "clustered_lighting: cluster_build pipeline create: {}",
          SDL_GetError ());
    }
  }

  // ----- light cull compute pipeline -----
  {
    auto id = m_res_mgr->register_shader (
        "engine://compiled_shaders/light_cull.slang.spv");
    auto handle = m_res_mgr->load (id);
    if (!handle || handle->bytecode.empty ()) {
      wsl::log::gfx ()->error (
          "clustered_lighting: light_cull bytecode load failed");
      return;
    }

    SDL_GPUComputePipelineCreateInfo pi{};
    pi.code = handle->bytecode.data ();
    pi.code_size = handle->bytecode.size ();
    pi.format = gfx::shader::native_format ();
    pi.entrypoint = "csMain";
    pi.num_samplers = 0;
    pi.num_readonly_storage_textures = 0;
    pi.num_readonly_storage_buffers = 1; // light buffer (RO)
    pi.num_readwrite_storage_textures = 0;
    pi.num_readwrite_storage_buffers = 1; // cluster buffer (RW)
    pi.num_uniform_buffers = 1;           // ClusterParams
    pi.threadcount_x = kCullThreadsX;
    pi.threadcount_y = 1;
    pi.threadcount_z = 1;
    pi.props = 0;

    m_cull_pipe = SDL_CreateGPUComputePipeline (m_ctx->gpu_device, &pi);
    if (m_cull_pipe == nullptr) {
      wsl::log::gfx ()->error (
          "clustered_lighting: light_cull pipeline create: {}",
          SDL_GetError ());
    }
  }
}

void
clustered_lighting::on_resize (uint32_t width, uint32_t height)
{
  m_screen_w = (width == 0) ? 1 : width;
  m_screen_h = (height == 0) ? 1 : height;
  m_camera_dirty = true;
}

void
clustered_lighting::on_camera_changed (const glm::mat4 &view,
                                       const glm::mat4 &proj, float z_near,
                                       float z_far)
{
  m_cached_view = view;
  m_cached_proj = proj;
  m_cached_z_near = z_near;
  m_cached_z_far = z_far;
  m_camera_dirty = true;
}

void
clustered_lighting::update (SDL_GPUCommandBuffer *cmd,
                            std::span<const gpu_cluster_light> lights,
                            const glm::mat4 &view)
{
  ZoneScoped;
  if (!is_active () || cmd == nullptr) {
    return;
  }
  if (lights.size () > m_cfg.max_point_lights) {
    wsl::log::gfx ()->warn (
        "clustered_lighting: truncating {} point lights to {}", lights.size (),
        m_cfg.max_point_lights);
  }

  const uint32_t num_clusters = m_cfg.grid_x * m_cfg.grid_y * m_cfg.grid_z;

  // ----- (0) View-frustum pre-cull (Phase 4 of the plan) -----
  // Build the frustum once per frame, then filter the lights down to the
  // visible subset before uploading. The lights are in world space, so we
  // compose `view` (which the lighting_system already passes in) with the
  // cached projection. We treat each light as a sphere of `radius` at
  // `pos`; the GPU-side cluster AABB test is the authoritative reject
  // pass, so a small over-approximation here is fine.
  std::vector<gpu_cluster_light> visible_lights;
  const gpu_cluster_light *upload_src = lights.data ();
  uint32_t upload_count = static_cast<uint32_t> (
      std::min<size_t> (lights.size (), m_cfg.max_point_lights));

  if (m_cfg.frustum_pre_cull) {
    ZoneScopedN ("clustered_lighting::frustum_cull");
    const glm::mat4 vp = m_cached_proj * view;
    const frustum_planes frustum = extract_frustum_planes (vp);
    visible_lights.reserve (lights.size ());
    for (const gpu_cluster_light &light : lights) {
      if (visible_lights.size () >= m_cfg.max_point_lights) {
        break;
      }
      const glm::vec3 center = glm::vec3 (
          light.pos_radius.x, light.pos_radius.y, light.pos_radius.z);
      const float radius = light.pos_radius.w;
      if (sphere_intersects_frustum (center, radius, frustum)) {
        visible_lights.push_back (light);
      }
    }
    upload_src = visible_lights.data ();
    upload_count = static_cast<uint32_t> (visible_lights.size ());
  }

  const uint32_t num_lights = upload_count;

  // ----- (1) Upload lights via a copy pass -----
  {
    const uint32_t upload_size
        = num_lights * static_cast<uint32_t> (sizeof (gpu_cluster_light));
    if (upload_size > m_light_transfer_capacity) {
      wsl::log::gfx ()->error (
          "clustered_lighting: light upload exceeds transfer buffer capacity");
      return;
    }

    void *mapped
        = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, m_light_transfer, false);
    if (mapped == nullptr) {
      wsl::log::gfx ()->error (
          "clustered_lighting: SDL_MapGPUTransferBuffer failed: {}",
          SDL_GetError ());
      return;
    }
    std::memcpy (mapped, upload_src, upload_size);
    SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, m_light_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);
    if (copy == nullptr) {
      // Per the SDL docs, `SDL_BeginGPUCopyPass` returns NULL on failure
      // (e.g. the cmd buffer is in a bad state from a failed swapchain
      // acquire). Without this check, the subsequent calls below would
      // dereference a NULL handle and on AMDVK the segfault is reported
      // as happening inside `SDL_DispatchGPUCompute` because the
      // compute pass is the first SDL call after the failed copy pass.
      wsl::log::gfx ()->error (
          "clustered_lighting: SDL_BeginGPUCopyPass failed: {}",
          SDL_GetError ());
      return;
    }

    // Vulkan rejects `vkCmdCopyBuffer` with size 0
    // (VUID-VkBufferCopy-size-01988), so skip the upload entirely
    // when the scene has no point lights this frame. The light buffer
    // will simply retain whatever it held before; the cull pass reads
    // 0 entries via `u_NumPointLights` and so is unaffected.
    if (upload_size > 0) {
      SDL_GPUTransferBufferLocation src{};
      src.transfer_buffer = m_light_transfer;
      src.offset = 0;

      SDL_GPUBufferRegion dst{};
      dst.buffer = m_light_buf;
      dst.offset = 0;
      dst.size = upload_size;

      SDL_UploadToGPUBuffer (copy, &src, &dst, false);
    }
    SDL_EndGPUCopyPass (copy);
  }

  cluster_params params{};
  params.inverse_projection = glm::inverse (m_cached_proj);
  params.screen_size = glm::vec2 (static_cast<float> (m_screen_w),
                                  static_cast<float> (m_screen_h));
  params.z_near = m_cached_z_near;
  params.z_far = m_cached_z_far;
  params.grid_size = glm::uvec3 (m_cfg.grid_x, m_cfg.grid_y, m_cfg.grid_z);
  params.num_clusters = num_clusters;
  params.cluster_padding = glm::uvec3 (0);
  params.view = view;
  params.num_point_lights = num_lights;
  params.light_padding = glm::uvec3 (0);

  // ----- (2) Cluster build pass: writes cluster AABBs into u_ClusterBuffer.
  // Recorded in its own compute pass so that the dispatch is serialized
  // before the cull pass and the cull pass is guaranteed to read the
  // freshly-written AABBs. Doing both dispatches in a single compute
  // pass is undefined per the SDL_GPU docs:
  //   "If you dispatch multiple times in a compute pass, and the
  //    dispatches write to the same resource region as each other,
  //    there is no guarantee of which order the writes will occur."
  // The build pipeline has 0 RO + 1 RW storage buffer, so the cluster
  // buffer is bound at slot 0 of the unified storage buffer space.
  {
    // In SDL3 main, RW storage buffers MUST be passed to
    // SDL_BeginGPUComputePass (not bound later via
    // SDL_BindGPUComputeStorageBuffers, which only sets RO bindings).
    // Passing 0 RW bindings here with a pipeline that expects 1 RW
    // buffer leaves readWriteComputeStorageBufferBindings[0] stale,
    // causing the driver to read garbage on dispatch (this was the
    // source of the "0x36313259" / "Y216" use-after-free crashes).
    SDL_GPUBuffer *cluster_buf = m_cluster_bufs[m_ctx->current_frame_slot ()];
    SDL_GPUStorageBufferReadWriteBinding rw_bindings[1] = {};
    rw_bindings[0].buffer = cluster_buf;
    rw_bindings[0].cycle = true;
    SDL_GPUComputePass *compute
        = SDL_BeginGPUComputePass (cmd, nullptr, 0, rw_bindings, 1);
    if (compute == nullptr) {
      // The SDL docs say `SDL_BeginGPUComputePass` returns NULL on
      // failure. Without this check, the `SDL_DispatchGPUCompute` call
      // below would dereference a NULL pass handle and the segfault
      // would be reported as happening inside the dispatch (which is
      // exactly what we were seeing intermittently on AMDVK).
      wsl::log::gfx ()->error (
          "clustered_lighting: SDL_BeginGPUComputePass (build) failed: {}",
          SDL_GetError ());
      return;
    }
    ZoneScopedN ("clustered_lighting::build_pass");
    SDL_BindGPUComputePipeline (compute, m_build_pipe);
    SDL_PushGPUComputeUniformData (cmd, 0, &params, sizeof (params));

    const Uint32 groups_x
        = (m_cfg.grid_x + kBuildThreadsX - 1) / kBuildThreadsX;
    const Uint32 groups_y
        = (m_cfg.grid_y + kBuildThreadsY - 1) / kBuildThreadsY;
    const Uint32 groups_z = m_cfg.grid_z;
    SDL_DispatchGPUCompute (compute, groups_x, groups_y, groups_z);
    SDL_EndGPUComputePass (compute);
  }

  // ----- (3) Light cull pass: assigns lights to clusters.
  // Recorded in a separate compute pass. The cull pipeline has 1 RO
  // (light) + 1 RW (cluster) storage buffer. RW must be passed to
  // SDL_BeginGPUComputePass; RO is still bound via
  // SDL_BindGPUComputeStorageBuffers.
  {
    SDL_GPUBuffer *cluster_buf = m_cluster_bufs[m_ctx->current_frame_slot ()];
    SDL_GPUStorageBufferReadWriteBinding rw_bindings[1] = {};
    rw_bindings[0].buffer = cluster_buf;
    rw_bindings[0].cycle = true;
    SDL_GPUComputePass *compute
        = SDL_BeginGPUComputePass (cmd, nullptr, 0, rw_bindings, 1);
    if (compute == nullptr) {
      wsl::log::gfx ()->error (
          "clustered_lighting: SDL_BeginGPUComputePass (cull) failed: {}",
          SDL_GetError ());
      return;
    }
    ZoneScopedN ("clustered_lighting::cull_pass");
    SDL_BindGPUComputePipeline (compute, m_cull_pipe);
    SDL_PushGPUComputeUniformData (cmd, 0, &params, sizeof (params));

    SDL_GPUBuffer *ro_bufs[1] = { m_light_buf };
    SDL_BindGPUComputeStorageBuffers (compute, 0, ro_bufs, 1);

    const Uint32 groups = (num_clusters + kCullThreadsX - 1) / kCullThreadsX;
    SDL_DispatchGPUCompute (compute, groups, 1, 1);
    SDL_EndGPUComputePass (compute);
  }

  TracyPlot ("uploaded_lights", (int64_t)num_lights);
  TracyPlot ("total_clusters", (int64_t)num_clusters);
}
void
clustered_lighting::bind_for_graphics (SDL_GPURenderPass *pass) const
{
  if (!is_active () || pass == nullptr) {
    return;
  }

  // The HLSL register numbers chosen in cube.frag.slang are:
  //   t15, space2  -> light buffer
  //   t16, space2  -> cluster buffer (declared as StructuredBuffer for RO)
  // SDL_GPU's "first_slot" matches the HLSL register number, which
  // maps 1:1 to the SPIR-V binding number after the -fvk-*-shift
  // flags. (t16 is used instead of u0 because both t and u registers
  // are merged into the same descriptor set by the shifts, so u0
  // collides with the t0 texture; t16 is the first free slot after
  // the textures.)
  SDL_GPUBuffer *light_buf = m_light_buf;
  SDL_BindGPUFragmentStorageBuffers (pass, 15, &light_buf, 1);

  SDL_GPUBuffer *cluster_buf = m_cluster_bufs[m_ctx->current_frame_slot ()];
  SDL_BindGPUFragmentStorageBuffers (pass, 16, &cluster_buf, 1);
}

void
clustered_lighting::push_graphics_uniforms (SDL_GPUCommandBuffer *cmd,
                                            const glm::mat4 &inv_proj,
                                            const glm::vec2 &screen_size) const
{
  if (!is_active () || cmd == nullptr) {
    return;
  }

  cluster_params params{};
  params.inverse_projection = inv_proj;
  params.screen_size = screen_size;
  params.z_near = m_cached_z_near;
  params.z_far = m_cached_z_far;
  params.grid_size = glm::uvec3 (m_cfg.grid_x, m_cfg.grid_y, m_cfg.grid_z);
  params.num_clusters = m_cfg.grid_x * m_cfg.grid_y * m_cfg.grid_z;
  params.cluster_padding = glm::uvec3 (0);
  params.view = m_cached_view;
  params.num_point_lights = 0;
  params.light_padding = glm::uvec3 (0);

  // ClusterParams lives at cbuffer b0, space3 in the Slang source.
  // HLSL b0 corresponds to the cbuffer descriptor in set 3.
  // ClusterParams is the 5th cbuffer in `cube.frag.slang` (b4, space3),
  // so the SPIR-V binding is 4. The slot in SDL_PushGPUFragmentUniformData
  // matches the binding number (see Material/Lighting/IBL/Post pushes in
  // scene_renderer.cpp).
  SDL_PushGPUFragmentUniformData (cmd, 4, &params, sizeof (params));
}

} // namespace gfx

} // namespace wsl
