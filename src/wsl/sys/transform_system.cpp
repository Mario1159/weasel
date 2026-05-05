#include "transform_system.hpp"

#include "../comp/hierarchy.hpp"
#include "reg/sig/signal_hub.hpp"
#include "../comp/transform.hpp"
#include "../comp/world_transform.hpp"
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/ext/matrix_float4x4.hpp>


namespace wsl
{

namespace sys
{

void
transform_system::update_world_recursive (entt::registry &reg, entt::entity e,
                                          const glm::mat4 &parent_world) const
{
  comp::transform  const&tr = reg.get<comp::transform> (e);
  comp::world_transform &wt = reg.get<comp::world_transform> (e);

  glm::mat4 const local = tr.model ();
  wt.value = parent_world * local;

  if (auto *h = reg.try_get<comp::hierarchy> (e)) {
    for (entt::entity child = h->first; child != entt::null;
         child = reg.get<comp::hierarchy> (child).next) {
      update_world_recursive (reg, child, wt.value);
    }
  }
}

void
transform_system::register_signals (reg::sig::signal_hub &hub)
{
  (void)hub;
}

void
transform_system::register_event_handlers (reg::sig::signal_hub &hub)
{
  (void)hub;
}

void
transform_system::register_iterations (reg::sig::signal_hub &hub)
{
  clear_registered_iterations ();

  register_iteration<comp::transform, comp::world_transform> (
      hub, "update_world_transforms",
      [this] (entt::registry &registry, double dt) {
        update_world_transforms (registry, dt);
      });
}

void
transform_system::on_update (entt::registry &registry, double dt)
{
  run_registered_iterations (registry, dt);
}

void
transform_system::update_world_transforms (entt::registry &reg, double /*unused*/)
{
  // 1. Process roots (no hierarchy or parent == null)
  auto view = reg.view<comp::transform, comp::world_transform> ();

  for (entt::entity const e : view) {
    auto *h = reg.try_get<comp::hierarchy> (e);
    if ((h == nullptr) || h->parent == entt::null) {
      update_world_recursive (reg, e, glm::mat4{ 1.0F });
    }
  }
}

} // namespace sys

} // namespace wsl
