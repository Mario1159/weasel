#pragma once

#include "gpu_resources.hpp"
#include <cstdint>

namespace wsl::gfx
{

class render_window;
class render_context;

/**
 * GPU render-target set for a single subviewport.
 *
 * Contains MSAA + resolve colour targets (HDR), an MSAA depth target,
 * and an optional LDR output target.  The formats match the main window
 * so the existing scene_renderer pipelines can render into them
 * without modification.
 */
struct subviewport_target
{
  gpu_texture color_msaa;
  gpu_texture color_resolve;
  gpu_texture bloom_msaa;
  gpu_texture bloom_resolve;
  gpu_texture depth;

  uint32_t width = 0;
  uint32_t height = 0;

  static subviewport_target create (render_window *window, render_context *ctx,
                                    uint32_t width, uint32_t height);
  static void destroy (subviewport_target &target);
};

} // namespace wsl::gfx
