#pragma once

#include <entt/entt.hpp>
#include "system.hpp"


namespace wsl
{

namespace sys
{

class lighting_system : public sys::ecs_system_t<lighting_system>
{
public:
  explicit lighting_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({ "Shadow System", "Transform System" });
  }

  void on_render_record_draw_cmd (entt::registry &registry) override;
};

} // namespace sys

} // namespace wsl
