#pragma once

#include "viewport.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

namespace wsl
{

/**
 * @namespace wsl::gfx
 * @brief Low-level graphics and rendering abstractions.
 */
namespace gfx
{

class render_context
{
public:
  render_context (bool headless = false);
  ~render_context ();

  SDL_GPUDevice *device () const;
  SDL_GPUCommandBuffer *command_buffer () const;
  SDL_GPURenderPass *main_render_pass () const;
  SDL_GPURenderPass *ui_render_pass () const;

  bool has_active_frame () const;
  bool has_main_render_pass () const;
  bool has_ui_render_pass () const;

  void begin_frame ();
  void submit_frame ();

  SDL_GPURenderPass *
  begin_render_pass (const SDL_GPUColorTargetInfo *color_targets,
                     Uint32 num_color_targets,
                     const SDL_GPUDepthStencilTargetInfo *depth_target) const;
  void
  begin_main_render_pass (const SDL_GPUColorTargetInfo *color_targets,
                          Uint32 num_color_targets,
                          const SDL_GPUDepthStencilTargetInfo *depth_target);
  void end_main_render_pass ();

  void begin_cmd ();
  void end_cmd ();

  void begin_clear_render_pass (SDL_GPUTexture *swapchain,
                                SDL_GPUTexture *depth);
  void begin_clear_render_pass (SDL_GPUTexture *color_msaa,
                                SDL_GPUTexture *resolve_target,
                                SDL_GPUTexture *depth);
  void end_clear_render_pass ();

  void begin_ui_render_pass (SDL_GPUTexture *swapchain);
  void end_ui_render_pass ();

  //! Sets the GPU viewport on the active main render pass.
  void set_viewport (const SDL_GPUViewport &vp);
  //! Sets the GPU scissor rect on the active main render pass.
  void set_scissor_rect (const SDL_Rect &rect);
  //! Resets the viewport to the full render target.
  void reset_viewport (uint32_t width, uint32_t height);
  //! Resets the scissor rect to the full render target.
  void reset_scissor_rect (uint32_t width, uint32_t height);

  SDL_GPUDevice *gpu_device = nullptr;
  SDL_GPUCommandBuffer *main_cmd = nullptr;
  SDL_GPURenderPass *main_pass = nullptr;
  SDL_GPURenderPass *ui_pass = nullptr;
};

} // namespace gfx

} // namespace wsl
