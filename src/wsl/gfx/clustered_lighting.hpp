#pragma once

#include "wsl/gfx/lighting.hpp"
#include "wsl/gfx/render_context.hpp"

#include <SDL3/SDL_gpu.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace wsl
{

namespace rsc
{
class resource_manager;
}

namespace gfx
{

/*!
 * \brief Configuration for the clustered lighting grid.
 */
struct clustered_lighting_config
{
  //! Number of tiles along the X axis (screen width).
  uint32_t grid_x = 16;
  //! Number of tiles along the Y axis (screen height).
  uint32_t grid_y = 9;
  //! Number of depth slices.
  uint32_t grid_z = 24;
  //! Maximum lights that can be assigned to a single cluster.
  uint32_t max_lights_per_cluster = 100;
  //! Upper bound on the number of point lights uploaded to the SSBO.
  uint32_t max_point_lights = 4096;
  //! When true, point lights outside the view frustum are filtered out
  //! on the CPU before the GPU cull pass. Cuts O(L*C) to O(V*C) where
  //! V is the number of visible lights.
  bool frustum_pre_cull = true;
  //! Master switch: when false, the compute passes are skipped and the
  //! fragment shader must use the fallback path.
  bool enabled = true;
};

/*!
 * \brief GPU-side mirror of `gpu_point_light`. Layout matches the
 * StructuredBuffer<GpuPointLight> declaration in `light_cull.slang`.
 */
struct alignas (16) gpu_cluster_light
{
  glm::vec4 pos_radius;      // xyz = pos, w = radius
  glm::vec4 color_intensity; // rgb = color, w = intensity
  glm::ivec4 shadow_info;    // x = shadow index, y = cast_shadows
};

/*!
 * \brief Cluster AABB + light list. Mirrors the Slang `Cluster` struct.
 *
 * Padded to 16 bytes by `alignas(16)` so the SPIR-V std430 layout matches.
 */
struct alignas (16) gpu_cluster
{
  glm::vec4 min_point;
  glm::vec4 max_point;
  uint32_t count;
  uint32_t light_indices[100];
};

/*!
 * \brief Owns the compute pipelines, storage buffers, and per-frame
 *        dispatch logic for clustered forward lighting.
 *
 * Lifecycle:
 *   1. Construct once per `scene_renderer` instance.
 *   2. Call `on_resize()` when the window size changes.
 *   3. Call `on_camera_changed()` whenever the active view changes.
 *   4. Call `update()` each frame after the camera is resolved and
 *      before any opaque draw. The point lights are uploaded into a
 *      storage buffer, then two compute passes build the cluster AABBs
 *      and assign lights to clusters.
 *   5. Call `bind_for_graphics()` at the start of the 3D render pass
 *      to bind the SSBOs to the fragment stage.
 */
class clustered_lighting
{
public:
  explicit clustered_lighting (gfx::render_context *ctx,
                               rsc::resource_manager *res_mgr);
  ~clustered_lighting ();

  clustered_lighting (const clustered_lighting &) = delete;
  clustered_lighting &operator= (const clustered_lighting &) = delete;

  //! Updates the screen size used by the cluster build pass.
  void on_resize (uint32_t width, uint32_t height);

  //! Records the active view so the next `update()` knows the projection
  //! and view matrices to feed the compute shaders.
  void on_camera_changed (const glm::mat4 &view, const glm::mat4 &proj,
                          float z_near, float z_far);

  //! Uploads the point lights and dispatches both compute passes.
  //! `lights.size()` must be `<= config.max_point_lights`.
  void update (SDL_GPUCommandBuffer *cmd,
               std::span<const gpu_cluster_light> lights,
               const glm::mat4 &view);

  //! Binds the cluster and light storage buffers for the fragment stage
  //! of the main 3D render pass.
  void bind_for_graphics (SDL_GPURenderPass *pass) const;

  //! Pushes the per-frame cluster uniform data (cbuffer b0, space3 in
  //! Slang) to the fragment stage. Called from the 3D render loop.
  void push_graphics_uniforms (SDL_GPUCommandBuffer *cmd,
                               const glm::mat4 &inv_proj,
                               const glm::vec2 &screen_size) const;

  //! Returns the current configuration.
  [[nodiscard]] const clustered_lighting_config &
  config () const
  {
    return m_cfg;
  }

  //! True when the master switch is on and GPU resources are valid.
  [[nodiscard]] bool
  is_active () const
  {
    if (!m_cfg.enabled || m_build_pipe == nullptr || m_cull_pipe == nullptr
        || m_light_buf == nullptr) {
      return false;
    }
    for (uint32_t i = 0; i < k_cluster_buf_slots; ++i) {
      if (m_cluster_bufs[i] == nullptr) {
        return false;
      }
    }
    return true;
  }

private:
  void create_pipelines ();
  void create_buffers ();
  void destroy_resources ();

  gfx::render_context *m_ctx = nullptr;
  rsc::resource_manager *m_res_mgr = nullptr;

  SDL_GPUComputePipeline *m_build_pipe = nullptr;
  SDL_GPUComputePipeline *m_cull_pipe = nullptr;

  // The cluster buffer is per-frame-in-flight rather than a single
  // shared buffer. With `k_max_frames_in_flight = 3` in `render_context`
  // there are three slots; the slot index is obtained from
  // `m_ctx->current_frame_slot()` at record time. This avoids a
  // use-after-free on AMDVK/Mesa where, on the first dispatch after
  // creation, the underlying `VkBuffer` handle is briefly NULL until
  // SDL_GPU's internal allocator has finished wiring the buffer up.
  // The Vulkan validation layer reports the handle as
  // "VkBuffer 0x0 ... invalid or has been destroyed" and on AMDVK the
  // driver segfaults the dispatch. With a per-slot pool each frame's
  // compute passes always operate on a buffer that has been quiesced
  // for at least 2 frames, so the first-use race is gone. The light
  // buffer stays single — it's read-only and has the same shape every
  // frame, so it doesn't need to be per-slot.
  static constexpr uint32_t k_cluster_buf_slots = 3;
  SDL_GPUBuffer *m_cluster_bufs[k_cluster_buf_slots]{};
  SDL_GPUBuffer *m_light_buf = nullptr;

  //! Transfer buffer used to upload point lights each frame.
  SDL_GPUTransferBuffer *m_light_transfer = nullptr;
  uint32_t m_light_transfer_capacity = 0;

  clustered_lighting_config m_cfg{};

  //! Cached for the next `update()` call.
  glm::mat4 m_cached_view{ 1.0F };
  glm::mat4 m_cached_proj{ 1.0F };
  float m_cached_z_near = 0.1F;
  float m_cached_z_far = 100.0F;
  uint32_t m_screen_w = 1;
  uint32_t m_screen_h = 1;
  bool m_camera_dirty = true;
};

} // namespace gfx

} // namespace wsl
