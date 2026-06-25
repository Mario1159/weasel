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

  // --- Frame-in-flight (triple buffering) ---
  // The number of command buffers that may be in flight on the GPU at
  // once. Each slot owns a command buffer and (after submit) a fence that
  // signals when the GPU has finished executing the work it recorded.
  // `begin_frame` waits on the slot's fence before recycling it, which
  // replaces the old max-1 frame-in-flight stall on the swapchain acquire.
  static constexpr uint32_t kMaxFramesInFlight = 3;

  //! Index of the slot chosen for the current frame
  //! (0 .. kMaxFramesInFlight-1).
  [[nodiscard]] uint32_t
  current_frame_slot () const
  {
    return m_current_slot;
  }

  //! Monotonically increasing frame counter, useful for per-frame
  //! data tagging.
  [[nodiscard]] uint64_t
  frame_index () const
  {
    return m_frame_index;
  }

  // The fence for the most recently submitted command buffer, or
  // nullptr if no frame is in flight. Owned by the per-slot
  // FrameSlot and released after the slot is recycled (i.e. the
  // caller must wait + release with SDL_ReleaseGPUFence before the
  // next frame reuses the slot).
  SDL_GPUFence *current_fence () const;

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

private:
  //! Per-slot state. `cmd_buffer` is the live command buffer for this
  //! slot while the frame is being recorded; `fence` is set by
  //! `submit_frame` and is non-null from submit until the next
  //! `begin_frame` on this slot completes.
  struct FrameSlot
  {
    SDL_GPUFence *fence = nullptr;
  };

  FrameSlot m_slots[kMaxFramesInFlight]{};
  uint32_t m_current_slot = 0;
  uint64_t m_frame_index = 0;
};

} // namespace gfx

} // namespace wsl
