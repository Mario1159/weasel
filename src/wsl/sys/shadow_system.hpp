#pragma once

#include "system.hpp"
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif


namespace wsl
{

namespace sys
{

class shadow_system : public sys::ecs_system_t<shadow_system>
{
public:
  explicit shadow_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({ "Transform System" });
  }
  void on_render_record_draw_cmd (entt::registry &registry) override;
};

} // namespace sys

} // namespace wsl
