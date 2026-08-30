#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "system.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif


namespace wsl
{

namespace sys
{

class skybox_system : public sys::ecs_system_t<skybox_system>
{
public:
  explicit skybox_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({}, {});
  }

  void on_render_record_draw_cmd (entt::registry &registry) override;
};

} // namespace sys

} // namespace wsl
