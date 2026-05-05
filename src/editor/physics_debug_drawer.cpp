#include "physics_debug_drawer.hpp"

#include "debug/debug_renderer.hpp"
#include "wsl/phys/physics_engine.hpp"
#include "physics_debug_renderer.hpp"

#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace editor {

void draw_physics_debug(wsl::phys::engine& engine, wsl::debug::debug_renderer_interface& renderer) {
  // The editor's physics_debug_renderer implements both the engine-specific
  // JPH::DebugRenderer and the wsl::debug::debug_renderer_interface. Try to
  // downcast to the editor implementation so we can pass a Jolt DebugRenderer
  // to the Jolt debug draw calls.
  auto *p = dynamic_cast<editor::physics_debug_renderer*>(&renderer);
  if (p == nullptr) {
    return;
}

  engine.get_system().DrawBodies(
      JPH::BodyManager::DrawSettings{ .mDrawShape = true,
                                      .mDrawShapeWireframe = true,
                                      .mDrawBoundingBox = true,
                                      .mDrawWorldTransform = false,
                                      .mDrawVelocity = false },
      p);

  engine.get_system().DrawConstraints(p);
  engine.get_system().DrawConstraintLimits(p);
  engine.get_system().DrawConstraintReferenceFrame(p);
}

} // namespace editor
