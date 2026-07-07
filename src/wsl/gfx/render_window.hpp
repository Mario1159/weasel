#pragma once

#include "wsl/comp/camera.hpp"
#include "wsl/gfx/gpu_resources.hpp"
#include "wsl/gfx/mesh.hpp"
#include "wsl/gfx/render_context.hpp"
#include "wsl/gfx/subviewport_target.hpp"
#include "wsl/gfx/viewport.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <entt/entt.hpp>
#include <vector>

namespace wsl
{

namespace rsc
{
class resource_manager;
}

namespace gfx
{

class render_window
{
public:
  //! Construct a render window.
  //!
  //! @param try_disable_vsync_on_startup When true (default), the
  //!   constructor requests `IMMEDIATE` present mode so the GPU's
  //!   present doesn't block on vblank. If the call fails, a spdlog
  //!   error is logged and the swapchain stays in vsync-paced mode.
  //!   Pass false to skip the attempt entirely (e.g. on drivers where
  //!   changing the present mode corrupts the compute queue).
  render_window (const char *name, int width, int height,
                 wsl::gfx::render_context *ctx,
                 wsl::rsc::resource_manager *res_mgr, bool headless = false,
                 bool try_disable_vsync_on_startup = true);
  ~render_window ();

  void get_size (uint32_t &width, uint32_t &height) const;
  void get_size (int &width, int &height) const;

  //! Toggle the swapchain present mode. When @p enabled is true, requests
  //! `MAILBOX` if the backend supports it (lower-latency vsync), otherwise
  //! `VSYNC`. When false, requests `IMMEDIATE` (no vblank sync).
  //!
  //! Returns true if the GPU swapchain was actually reconfigured.
  //! Returns false (and logs a spdlog error) if the backend refuses the
  //! request — the previous mode remains active in that case.
  //!
  //! @note On the Vulkan backend with AMDVK, both `IMMEDIATE` and
  //!   `MAILBOX` are reported as supported and accepted by
  //!   `SDL_SetGPUSwapchainParameters`, but the next compute dispatch
  //!   can segfault on those drivers. If you hit that crash, call
  //!   `set_vsync(true)` early in startup (or pass
  //!   `try_disable_vsync_on_startup = false` to the ctor).
  bool set_vsync (bool enabled);

  //! Current vsync state: true means the swapchain is in VSYNC or
  //! MAILBOX mode, false means IMMEDIATE.
  [[nodiscard]] bool
  vsync () const
  {
    return m_vsync;
  }

  SDL_Window *handler = nullptr;
  wsl::gfx::texture swapchain;

  // entt::registry *registry;
  wsl::gfx::render_context *ctx;

  gpu_texture depth_texture;
  SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

  gpu_texture msaa_hdr_scene;
  gpu_texture msaa_hdr_bloom;

  gpu_texture hdr_scene;     // resolved HDR scene (sampler)
  gpu_texture hdr_bloom_src; // resolved HDR bloom source (sampler)

  // half-res bloom ping-pong
  gpu_texture bloom_a;
  gpu_texture bloom_b;

  // final LDR (tonemapped + bloom) output that can be sampled by ImGui /
  // GameView
  wsl::gfx::texture present_tex;
  SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

  gpu_graphics_pipeline pipe_downsample;
  gpu_graphics_pipeline pipe_blur;
  gpu_graphics_pipeline pipe_composite;

  gpu_sampler linear_sampler;

  int current_sample_count = SDL_GPU_SAMPLECOUNT_4;
  bool present_to_swapchain = true;
  SDL_FColor scene_clear_color{ 0.1F, 0.1F, 0.1F, 1.0F };
  float exposure = 1.0F;
  float bloom_intensity = 1.0F;

  void create_depth_texture ();
  void begin_3d_pass (bool clear_color = true, bool clear_depth = true,
                      const char *label = "Main 3D Pass") const;
  void end_3d_pass (bool run_postprocess = true);

  //! Begin a 3D render pass targeting a subviewport offscreen target.
  void begin_subviewport_pass (const subviewport_target &target,
                               bool clear_color = true, bool clear_depth = true,
                               const char *label = "Subviewport Pass") const;
  //! End the subviewport render pass (resolves MSAA, no postprocess).
  void end_subviewport_pass ();
  void begin_ui_pass () const;
  void end_ui_pass () const;
  void new_swapchain ();
  void on_resize ();

  // -----------------------------------------------------------------
  // Tracy frame image capture
  // -----------------------------------------------------------------
  // The engine's present_tex (the tonemapped, bloom-augmented LDR
  // output) is the natural source for Tracy's per-frame screenshot
  // in the Frame view. The flow is:
  //
  //   1. frame_image_init() sets the downsample target size
  //      (e.g. 320x180, must be divisible by 4 for Tracy).
  //   2. frame_image_resize(src_w, src_h) (re)allocates the staging
  //      transfer buffer to be large enough to hold the *full*
  //      present_tex download. Called from on_resize().
  //   3. frame_image_issue_copy() records a copy pass from
  //      present_tex into that staging buffer.
  //   4. The main command buffer is submitted (existing path).
  //   5. frame_image_submit(fence) blocks on the submission fence,
  //      maps the staging buffer, downscales BGRA -> RGBA on the
  //      CPU and forwards the result to FrameImage().
  //
  // The downscaled image lives only for the duration of the
  // FrameImage() call; Tracy copies the pixels internally so the
  // staging buffer can be reused the next frame.
  void frame_image_init (uint32_t target_w, uint32_t target_h);
  void frame_image_shutdown ();
  void frame_image_resize (uint32_t src_w, uint32_t src_h);
  void frame_image_issue_copy ();
  void frame_image_submit (SDL_GPUFence *fence);

  void postprocess_hdr_bloom ();

  //! Push a viewport onto the stack. The active viewport is applied to the
  //! main render pass on the next begin_3d_pass().
  void push_viewport (const gfx::viewport &vp);
  //! Pop the top viewport. If no viewports remain, full-screen rendering
  //! resumes.
  void pop_viewport ();
  //! Remove all viewports and return to full-screen rendering.
  void reset_viewports ();
  //! Returns true when at least one viewport is active.
  [[nodiscard]] bool has_active_viewport () const;
  //! Returns the number of active viewports.
  [[nodiscard]] size_t viewport_count () const;
  //! Returns the currently active viewport (top of stack), or a full-screen
  //! default if none are pushed.
  [[nodiscard]] gfx::viewport current_viewport () const;

  //! Applies the given viewport directly to the active main render pass.
  //! Does not use the viewport stack.
  void apply_viewport (const gfx::viewport &vp) const;

  [[nodiscard]] wsl::rsc::resource_manager *
  resource_manager () const
  {
    return m_res_mgr;
  }

private:
  SDL_GPUSampler *ensure_linear_sampler ();
  void destroy_texture (gpu_texture &texture) const;

  //! Tracked vsync state. Defaults to true (VSYNC) so the engine is
  //! safe to use on any driver before the first `set_vsync` call.
  bool m_vsync = true;
  //! Tracked swapchain composition. SDL has no "get current
  //! composition" query, so we remember what we last set (or the
  //! default, SDR, if we have never set it).
  SDL_GPUSwapchainComposition m_swapchain_composition
      = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  SDL_GPUGraphicsPipeline *
  create_fullscreen_pipe (const char *frag_shader_path,
                          SDL_GPUTextureFormat out_format,
                          int num_uniform_buffers, int num_samplers);
  gpu_graphics_pipeline create_composite_pipe ();
  gpu_graphics_pipeline create_downsample_pipe ();
  gpu_graphics_pipeline create_blur_pipe ();

private:
  wsl::rsc::resource_manager *m_res_mgr = nullptr;
  std::vector<gfx::viewport> m_viewport_stack;

  // Tracy frame image capture state. Owned by the render window;
  // the staging transfer buffer is (re)allocated in
  // frame_image_resize() and torn down in frame_image_shutdown().
  gpu_transfer_buffer m_fi_transfer;
  // Size the staging buffer was last allocated for. Tracked so a
  // window resize that changes the present_tex dimensions re-allocates
  // exactly once instead of thrashing per-frame.
  uint32_t m_fi_alloc_w = 0;
  uint32_t m_fi_alloc_h = 0;
  // Source dimensions from the last frame_image_issue_copy().
  uint32_t m_fi_src_w = 0;
  uint32_t m_fi_src_h = 0;
  // Downsample target (set once in frame_image_init, immutable).
  uint32_t m_fi_dst_w = 0;
  uint32_t m_fi_dst_h = 0;
  uint32_t m_fi_dst_pitch = 0;
  bool m_fi_src_is_bgra = true;
};

} // namespace gfx

} // namespace wsl
