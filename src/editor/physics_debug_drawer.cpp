#include "physics_debug_drawer.hpp"

#include "debug/debug_renderer.hpp"
#include "wsl/phys/physics_engine.hpp"

namespace editor
{

/**
 * Emits physics debug visualization into the editor debug renderer.
 *
 * Box3D v0.1.0 exposes debug draw via ``b3World_Draw`` + ``b3DebugDraw``
 * callbacks, but ``debug_renderer_interface`` currently has no line/shape
 * primitives (only begin/end/upload/set_camera), so there is nothing to
 * forward the callbacks to yet. Wiring this up requires:
 *
 * 1. Adding primitive methods (segment, box, transform, ...) to
 *    ``wsl::debug::debug_renderer_interface`` and its concrete renderer.
 * 2. Exposing a ``b3World_Draw`` pass through ``phys::engine`` /
 *    ``box3d_adapter`` (the adapter currently has no debug-draw entry).
 * 3. Filling in the callbacks below.
 */
void
draw_physics_debug (wsl::phys::engine &engine,
                    wsl::debug::debug_renderer_interface &renderer)
{
  (void)engine;
  (void)renderer;
}

} // namespace editor
