#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "system.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
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
