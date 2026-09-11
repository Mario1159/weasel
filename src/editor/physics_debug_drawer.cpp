#include "physics_debug_drawer.hpp"

#include "debug/debug_renderer.hpp"
#include "wsl/phys/physics_engine.hpp"

namespace editor {

void draw_physics_debug(wsl::phys::engine& engine, wsl::debug::debug_renderer_interface& renderer) {
  // Physics debug drawing is deferred until Box3D provides a debug renderer.
  (void)engine;
  (void)renderer;
}

} // namespace editor
