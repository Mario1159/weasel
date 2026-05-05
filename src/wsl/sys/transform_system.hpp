#pragma once

#include "system.hpp"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>


namespace wsl
{

namespace sys
{

class transform_system : public sys::ecs_system_t<transform_system>
{
public:
  explicit transform_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({}, {});
  }

  void register_signals (reg::sig::signal_hub &hub) override;
  void register_event_handlers (reg::sig::signal_hub &hub) override;
  void register_iterations (reg::sig::signal_hub &hub) override;

  void on_update (entt::registry &registry, double dt) override;

private:
  void update_world_recursive (entt::registry &reg, entt::entity entity,
                               const glm::mat4 &parent_world) const;
  void update_world_transforms (entt::registry &registry, double dt);
};

} // namespace sys

} // namespace wsl
