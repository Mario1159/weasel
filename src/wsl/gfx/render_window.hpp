#pragma once

#include "wsl/comp/camera.hpp"
#include "wsl/gfx/mesh.hpp"
#include "wsl/gfx/render_context.hpp"
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

  SDL_GPUTexture *depth_texture = nullptr;
  SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

  SDL_GPUTexture *msaa_hdr_scene = nullptr;
  SDL_GPUTexture *msaa_hdr_bloom = nullptr;

  SDL_GPUTexture *hdr_scene = nullptr; // resolved HDR scene (sampler)
  SDL_GPUTexture *hdr_bloom_src
      = nullptr; // resolved HDR bloom source (sampler)

  // half-res bloom ping-pong
  SDL_GPUTexture *bloom_a = nullptr;
  SDL_GPUTexture *bloom_b = nullptr;

  // final LDR (tonemapped + bloom) output that can be sampled by ImGui /
  // GameView
  wsl::gfx::texture present_tex;
  SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

  SDL_GPUGraphicsPipeline *pipe_downsample = nullptr;
  SDL_GPUGraphicsPipeline *pipe_blur = nullptr;
  SDL_GPUGraphicsPipeline *pipe_composite = nullptr;

  SDL_GPUSampler *linear_sampler = nullptr;

  int current_sample_count = SDL_GPU_SAMPLECOUNT_4;
  bool present_to_swapchain = true;
  SDL_FColor scene_clear_color{ 0.1F, 0.1F, 0.1F, 1.0F };
  float exposure = 1.0F;
  float bloom_intensity = 1.0F;

  void create_depth_texture ();
  void begin_3d_pass (bool clear_color = true, bool clear_depth = true) const;
  void end_3d_pass (bool run_postprocess = true);
  void begin_ui_pass () const;
  void end_ui_pass () const;
  void new_swapchain ();
  void on_resize ();
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
  void destroy_texture (SDL_GPUTexture *&texture) const;

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
  SDL_GPUGraphicsPipeline *create_composite_pipe ();
  SDL_GPUGraphicsPipeline *create_downsample_pipe ();
  SDL_GPUGraphicsPipeline *create_blur_pipe ();

private:
  wsl::rsc::resource_manager *m_res_mgr = nullptr;
  std::vector<gfx::viewport> m_viewport_stack;
};

} // namespace gfx

} // namespace wsl
