#include "physics_debug_renderer.hpp"
#include "gfx/render_context.hpp"
#include "gfx/render_window.hpp"
#include "gfx/tracy_gpu_mem.hpp"
#include "wsl/gfx/shader.hpp"
#include "wsl/phys/jolt_runtime.hpp"
#include "wsl/rsc/resource_manager.hpp"

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <memory>

static inline glm::vec3
jph_to_glm (JPH::RVec3Arg v)
{
  return { v.GetX (), v.GetY (), v.GetZ () };
}

static inline glm::vec3
jph_to_glm (const JPH::Float3 &v)
{
  return { v.x, v.y, v.z };
}

static inline glm::mat4
jph_to_glm (JPH::RMat44Arg m)
{
  glm::mat4 out (1.0F);
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      out[c][r] = m (r, c);
    }
  }
  return out;
}

static inline glm::vec4
jph_color_to_glm (JPH::ColorArg c)
{
  return { static_cast<float> (c.r) / 255.0F, static_cast<float> (c.g) / 255.0F,
           static_cast<float> (c.b) / 255.0F,
           static_cast<float> (c.a) / 255.0F };
}

static inline JPH::Vec3
glm_to_jph (const glm::vec3 &v)
{
  return JPH::Vec3 (v.x, v.y, v.z);
}

namespace editor
{

physics_debug_renderer::physics_debug_renderer (wsl::gfx::render_window &w,
                                                wsl::gfx::render_context *ctx)
    : m_window (&w), m_ctx (ctx)
{
  Initialize ();

  auto *res_mgr = m_window->resource_manager ();
  auto vs_id = res_mgr->register_shader (
      "engine://compiled_shaders/flat.vert.slang.spv");
  auto fs_id = res_mgr->register_shader (
      "engine://compiled_shaders/flat.frag.slang.spv");

  SDL_GPUShader *vert = wsl::gfx::shader::load_from_manager (
      m_ctx->gpu_device, res_mgr, vs_id, SDL_GPU_SHADERSTAGE_VERTEX,
      /*num_uniform_buffers=*/1,
      /*num_samplers=*/0);

  SDL_GPUShader *frag = wsl::gfx::shader::load_from_manager (
      m_ctx->gpu_device, res_mgr, fs_id, SDL_GPU_SHADERSTAGE_FRAGMENT,
      /*num_uniform_buffers=*/0,
      /*num_samplers=*/0);

  // ------------------------------------------------------------
  // Common pipeline state
  // ------------------------------------------------------------
  SDL_GPUGraphicsPipelineCreateInfo pi{};
  SDL_zero (pi);

  pi.vertex_shader = vert;
  pi.fragment_shader = frag;

  // --- Vertex layout ---
  static SDL_GPUVertexBufferDescription vb{};
  vb.slot = 0;
  vb.pitch = sizeof (debug_vertex);
  vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

  pi.vertex_input_state.num_vertex_buffers = 1;
  pi.vertex_input_state.vertex_buffer_descriptions = &vb;

  static SDL_GPUVertexAttribute attrs[2];
  SDL_zero (attrs);

  attrs[0].location = 0;
  attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attrs[0].offset = offsetof (debug_vertex, pos);

  attrs[1].location = 1;
  attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  attrs[1].offset = offsetof (debug_vertex, color);

  pi.vertex_input_state.num_vertex_attributes = 2;
  pi.vertex_input_state.vertex_attributes = attrs;

  // --- Color target (swapchain) ---

  SDL_GPUColorTargetDescription ctd[2]{};
  ctd[0].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // scene
  ctd[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // bloom

  pi.target_info.num_color_targets = 2;
  pi.target_info.color_target_descriptions = ctd;

  // --- Depth (shared with main renderer) ---
  pi.target_info.has_depth_stencil_target = true;
  pi.target_info.depth_stencil_format = m_window->depth_format();

  pi.depth_stencil_state.enable_depth_test = true;
  pi.depth_stencil_state.enable_depth_write = true; // <-- IMPORTANT
  pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

  pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_4;

  // ------------------------------------------------------------
  // Line pipeline
  // ------------------------------------------------------------
  pi.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
  m_pipeline_lines = wsl::gfx::gpu_graphics_pipeline (m_ctx->gpu_device, pi);

  // ------------------------------------------------------------
  // Triangle pipeline
  // ------------------------------------------------------------
  pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  m_pipeline_tris = wsl::gfx::gpu_graphics_pipeline (m_ctx->gpu_device, pi);

  SDL_ReleaseGPUShader (m_ctx->gpu_device, vert);
  SDL_ReleaseGPUShader (m_ctx->gpu_device, frag);

  create_default_texture ();
}

physics_debug_renderer::~physics_debug_renderer ()
{
  destroy_default_resources ();
  m_pipeline_lines.reset ();
  m_pipeline_tris.reset ();
  m_vertex_buffer.reset ();
  m_upload_buffer.reset ();
  wsl::phys::release_jolt_runtime ();
}

std::unique_ptr<physics_debug_renderer>
make_physics_debug_renderer (wsl::gfx::render_window &window,
                             wsl::gfx::render_context *ctx)
{
  wsl::phys::retain_jolt_runtime ();

  try {
    return std::make_unique<physics_debug_renderer> (window, ctx);
  } catch (...) {
    wsl::phys::release_jolt_runtime ();
    throw;
  }
}

// ------------------------------------------------------------
// Frame control
// ------------------------------------------------------------

void
physics_debug_renderer::begin_frame ()
{
  m_line_vertices.clear ();
  m_tri_vertices.clear ();
}

void
physics_debug_renderer::end_frame (const glm::mat4 &vp)
{
  flush (vp);
}

void
physics_debug_renderer::set_camera_pos (const glm::vec3 &pos)
{
  camera_pos = pos;
}

// ------------------------------------------------------------
// Jolt DebugRenderer
// ------------------------------------------------------------

void
physics_debug_renderer::DrawLine (JPH::RVec3Arg a, JPH::RVec3Arg b,
                                  JPH::ColorArg c)
{
  glm::vec4 const col = jph_color_to_glm (c);
  m_line_vertices.push_back ({ jph_to_glm (a), col });
  m_line_vertices.push_back ({ jph_to_glm (b), col });
}

void
physics_debug_renderer::DrawTriangle (JPH::RVec3Arg a, JPH::RVec3Arg b,
                                      JPH::RVec3Arg c, JPH::ColorArg col,
                                      ECastShadow /*inCastShadow*/)
{
  glm::vec4 const color = jph_color_to_glm (col);
  m_tri_vertices.push_back ({ jph_to_glm (a), color });
  m_tri_vertices.push_back ({ jph_to_glm (b), color });
  m_tri_vertices.push_back ({ jph_to_glm (c), color });
}

physics_debug_renderer::Batch
physics_debug_renderer::CreateTriangleBatch (const Triangle *tris, int count)
{
  auto *batch = new debug_triangle_batch ();

  for (int i = 0; i < count; ++i) {
    debug_triangle_batch::tri tri;
    tri.v0 = jph_to_glm (tris[i].mV[0].mPosition);
    tri.v1 = jph_to_glm (tris[i].mV[1].mPosition);
    tri.v2 = jph_to_glm (tris[i].mV[2].mPosition);
    tri.color = jph_color_to_glm (tris[i].mV[0].mColor);
    batch->tris.push_back (tri);
  }

  return Batch (batch);
}

void
physics_debug_renderer::upload_buffers ()
{

  const Uint32 line_count = (Uint32)m_line_vertices.size ();
  const Uint32 tri_count = (Uint32)m_tri_vertices.size ();

  if (line_count == 0 && tri_count == 0) {
    return;
  }

  const Uint32 line_bytes = line_count * sizeof (debug_vertex);
  const Uint32 tri_bytes = tri_count * sizeof (debug_vertex);
  const Uint32 total_bytes = line_bytes + tri_bytes;

  // ------------------------------------------------------------
  // Ensure buffers are large enough
  // ------------------------------------------------------------
  if (!m_vertex_buffer || total_bytes > m_vertex_buffer_size) {
    m_vertex_buffer.reset ();
    m_upload_buffer.reset ();

    m_vertex_buffer_size = total_bytes;

    SDL_GPUBufferCreateInfo bi{};
    bi.size = total_bytes;
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_vertex_buffer = wsl::gfx::gpu_buffer (m_ctx->gpu_device, bi);

    SDL_GPUTransferBufferCreateInfo ti{};
    ti.size = total_bytes;
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    m_upload_buffer = wsl::gfx::gpu_transfer_buffer (m_ctx->gpu_device, ti);
  }

  // ------------------------------------------------------------
  // Upload data (lines first, triangles second)
  // ------------------------------------------------------------
  uint8_t *dst = (uint8_t *)SDL_MapGPUTransferBuffer (
      m_ctx->gpu_device, m_upload_buffer.get (), false);

  if (line_count != 0U) {
    memcpy (dst, m_line_vertices.data (), line_bytes);
  }

  if (tri_count != 0U) {
    memcpy (dst + line_bytes, m_tri_vertices.data (), tri_bytes);
  }

  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, m_upload_buffer.get ());

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTransferBufferLocation const src{ m_upload_buffer.get (), 0 };
  SDL_GPUBufferRegion const dstreg{ m_vertex_buffer.get (), 0, total_bytes };

  SDL_UploadToGPUBuffer (cp, &src, &dstreg, true);
  SDL_EndGPUCopyPass (cp);
  SDL_SubmitGPUCommandBuffer (cmd);
  cmd = nullptr;
}

// ------------------------------------------------------------
// Flush
// ------------------------------------------------------------
void
physics_debug_renderer::flush (const glm::mat4 &vp)
{
  const Uint32 line_count = (Uint32)m_line_vertices.size ();
  const Uint32 tri_count = (Uint32)m_tri_vertices.size ();

  const Uint32 line_bytes = line_count * sizeof (debug_vertex);

  if (line_count == 0 && tri_count == 0) {
    return;
  }

  // ------------------------------------------------------------
  // Ensure required fragment samplers are bound (from main pass)
  // ------------------------------------------------------------
  /*
  SDL_GPUTextureSamplerBinding dummy[5]{};

  for (int i = 0; i < 5; ++i) {
    dummy[i].texture = m_default_texture.get ();
    dummy[i].sampler = m_default_sampler.get ();
  }

  SDL_BindGPUFragmentSamplers (m_ctx->main_pass, 0, dummy, 5);
  */

  SDL_PushGPUVertexUniformData (m_ctx->main_cmd, 0, &vp, sizeof (glm::mat4));

  // ------------------------------------------------------------
  // Draw lines
  // ------------------------------------------------------------
  SDL_GPUBufferBinding vb{};
  vb.buffer = m_vertex_buffer.get ();
  vb.offset = 0;

  if (m_ctx->main_pass != nullptr) {
    SDL_BindGPUVertexBuffers (m_ctx->main_pass, 0, &vb, 1);

    if ((line_count != 0U) && m_pipeline_lines) {
      SDL_BindGPUGraphicsPipeline (m_ctx->main_pass, m_pipeline_lines.get ());
      SDL_DrawGPUPrimitives (m_ctx->main_pass, line_count, 1, 0, 0);
    }

    // ------------------------------------------------------------
    // Draw triangles
    // ------------------------------------------------------------
    if ((tri_count != 0U) && (m_pipeline_tris)) {
      vb.offset = line_bytes;
      SDL_BindGPUVertexBuffers (m_ctx->main_pass, 0, &vb, 1);

      SDL_BindGPUGraphicsPipeline (m_ctx->main_pass, m_pipeline_tris.get ());
      SDL_DrawGPUPrimitives (m_ctx->main_pass, tri_count, 1, 0, 0);
    }
  }
}

void
physics_debug_renderer::create_default_texture ()
{
  // ---- texture ----
  SDL_GPUTextureCreateInfo tex{};
  tex.type = SDL_GPU_TEXTURETYPE_2D;
  tex.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex.width = 1;
  tex.height = 1;
  tex.layer_count_or_depth = 1;
  tex.num_levels = 1;
  tex.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  m_default_texture = wsl::gfx::gpu_texture (m_ctx->gpu_device, tex);

  // ---- sampler ----
  SDL_GPUSamplerCreateInfo sinfo{};
  sinfo.min_filter = SDL_GPU_FILTER_LINEAR;
  sinfo.mag_filter = SDL_GPU_FILTER_LINEAR;
  sinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sinfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sinfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sinfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

  m_default_sampler = wsl::gfx::gpu_sampler (m_ctx->gpu_device, sinfo);

  // ---- staging buffer ----
  uint32_t white = 0xFFFFFFFF;

  SDL_GPUTransferBufferCreateInfo tinfo{};
  tinfo.size = sizeof (uint32_t);
  tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

  SDL_GPUTransferBuffer *upload
      = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tinfo);
  wsl::gfx::tracy_alloc_transfer (upload, tinfo.size);

  void *mapped = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, upload, false);
  memcpy (mapped, &white, sizeof (uint32_t));
  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, upload);

  // ---- begin command buffer ----
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);

  // ---- begin copy pass ----
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = upload;
  src.offset = 0;
  src.pixels_per_row = 1;
  src.rows_per_layer = 1;

  SDL_GPUTextureRegion dst{};
  dst.texture = m_default_texture.get ();
  dst.mip_level = 0;
  dst.layer = 0;
  dst.x = 0;
  dst.y = 0;
  dst.z = 0;
  dst.w = 1;
  dst.h = 1;
  dst.d = 1;

  SDL_UploadToGPUTexture (copy, &src, &dst, false);

  SDL_EndGPUCopyPass (copy);

  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, upload);
  wsl::gfx::tracy_free_transfer (upload);
}

void
physics_debug_renderer::destroy_default_resources ()
{
  m_default_sampler.reset ();
  m_default_texture.reset ();
}

void
physics_debug_renderer::DrawGeometry (
    JPH::RMat44Arg in_model_matrix, const JPH::AABox &in_world_space_bounds,
    float in_lod_scale_sq, JPH::ColorArg in_color,
    const JPH::DebugRenderer::GeometryRef &in_geometry,
    JPH::DebugRenderer::ECullMode /*inCullMode*/,
    JPH::DebugRenderer::ECastShadow /*inCastShadow*/,
    JPH::DebugRenderer::EDrawMode in_draw_mode)
{
  const LOD &lod = in_geometry->GetLOD (glm_to_jph (camera_pos),
                                        in_world_space_bounds, in_lod_scale_sq);

  auto *batch
      = static_cast<debug_triangle_batch *> (lod.mTriangleBatch.GetPtr ());

  glm::mat4 const m = jph_to_glm (in_model_matrix);
  glm::vec4 const color = jph_color_to_glm (in_color);

  for (const auto &t : batch->tris) {
    glm::vec3 const v0 = glm::vec3 (m * glm::vec4 (t.v0, 1.0F));
    glm::vec3 const v1 = glm::vec3 (m * glm::vec4 (t.v1, 1.0F));
    glm::vec3 const v2 = glm::vec3 (m * glm::vec4 (t.v2, 1.0F));

    if (in_draw_mode == JPH::DebugRenderer::EDrawMode::Solid) {
      m_tri_vertices.push_back ({ v0, color });
      m_tri_vertices.push_back ({ v1, color });
      m_tri_vertices.push_back ({ v2, color });
    } else {
      // Wireframe = edges
      m_line_vertices.push_back ({ v0, color });
      m_line_vertices.push_back ({ v1, color });

      m_line_vertices.push_back ({ v1, color });
      m_line_vertices.push_back ({ v2, color });

      m_line_vertices.push_back ({ v2, color });
      m_line_vertices.push_back ({ v0, color });
    }
  }
}

void
physics_debug_renderer::DrawText3D (JPH::Vec3Arg /*inPosition*/,
                                    const JPH::string_view & /*inString*/,
                                    JPH::ColorArg /*inColor*/,
                                    float /*inHeight*/)
{
  // No-op por ahora
}

physics_debug_renderer::Batch
physics_debug_renderer::CreateTriangleBatch (const Vertex *in_vertices,
                                             int /*in_vertex_count*/,
                                             const uint32_t *in_indices,
                                             int in_index_count)
{
  auto *batch = new debug_triangle_batch ();

  // Jolt garantiza que los índices vienen en múltiplos de 3
  const int tri_count = in_index_count / 3;
  batch->tris.reserve (tri_count);

  for (int i = 0; i < in_index_count; i += 3) {
    const Vertex &v0 = in_vertices[in_indices[i + 0]];
    const Vertex &v1 = in_vertices[in_indices[i + 1]];
    const Vertex &v2 = in_vertices[in_indices[i + 2]];

    debug_triangle_batch::tri tri;
    tri.v0 = jph_to_glm (v0.mPosition);
    tri.v1 = jph_to_glm (v1.mPosition);
    tri.v2 = jph_to_glm (v2.mPosition);

    // Color plano: toma el del primer vértice
    tri.color = jph_color_to_glm (v0.mColor);

    batch->tris.push_back (tri);
  }

  return Batch (batch);
}

} // namespace editor
