#pragma once

#include "wsl/debug/debug_renderer.hpp"
#include "wsl/gfx/render_context.hpp"
#include "wsl/gfx/render_window.hpp"

#include <memory>

namespace wsl::phys
{
class engine;
}

namespace editor
{

void draw_physics_debug (wsl::phys::engine &engine,
                         wsl::debug::debug_renderer_interface &renderer);

/**
 * No-op debug renderer used until physics debug draw is wired up.
 *
 * Box3D ships ``b3World_Draw`` callbacks, but
 * ``debug_renderer_interface`` has no primitive methods yet (see
 * ``physics_debug_drawer.cpp`` for the wiring steps), so this renderer
 * absorbs the frame lifecycle calls without emitting geometry.
 */
class noop_debug_renderer final
    : public wsl::debug::debug_renderer_interface
{
public:
  noop_debug_renderer (wsl::gfx::render_window &, wsl::gfx::render_context *)
  {
  }
  void begin_frame () override
  {
  }
  void end_frame (const glm::mat4 &) override
  {
  }
  void upload_buffers () override
  {
  }
  void set_camera_pos (const glm::vec3 &) override
  {
  }
};

inline std::unique_ptr<wsl::debug::debug_renderer_interface>
make_physics_debug_renderer (wsl::gfx::render_window &window,
                             wsl::gfx::render_context *ctx)
{
  return std::make_unique<noop_debug_renderer> (window, ctx);
}

} // namespace editor
