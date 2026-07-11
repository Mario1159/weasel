#include "skybox_system.hpp"

#include "../comp/singl/runtime_context.hpp"
#include "../comp/singl/rendering_manager.hpp"
#include "rsc/resource_manager.hpp"
#include <entt/entity/fwd.hpp>

namespace wsl
{

namespace sys
{

void
skybox_system::on_render_record_draw_cmd (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();

  auto *rendering = runtime_ctx.get_active_rendering_manager ();
  if (rendering == nullptr) {
    return;
  }

  auto *renderer = rendering->try_renderer ();
  if (renderer != nullptr) {
    // If procedural skybox is active, ensure it is baked with current sun
    // direction
    if (rendering->skybox.value == rsc::builtin_skybox_procedural) {
      auto handle = runtime_ctx.resource_manager ().get (rendering->skybox);
      if (handle) {
        renderer->bake_procedural_skybox (*handle,
                                          rendering->get_sun_direction ());
      }
    }

    renderer->draw_active_environment (rendering->skybox_rotation);
  }
}

} // namespace sys

} // namespace wsl
