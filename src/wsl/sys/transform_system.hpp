#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "system.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <glm/mat4x4.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <unordered_set>
#endif

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

  void register_event_sources (event::event_hub &hub) override;
  void register_event_sinks (event::event_hub &hub) override;
  void register_iterations (event::event_hub &hub) override;

  void on_update (entt::registry &registry, double dt) override;
  void on_editor_update (entt::registry &registry, double dt) override;

private:
  void update_world_recursive (entt::registry &reg, entt::entity entity,
                               const glm::mat4 &parent_world,
                               std::unordered_set<entt::entity> &path,
                               int depth) const;
  void update_world_transforms (entt::registry &registry, double dt);
};

} // namespace sys

} // namespace wsl
