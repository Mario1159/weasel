#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "entt/entity/fwd.hpp"
#endif
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

class render_3d_system : public sys::ecs_system_t<render_3d_system>
{
public:
  explicit render_3d_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({ "Lighting System", "Transform System" });
  }

  void on_render_record_draw_cmd (entt::registry &registry) override;
};

} // namespace sys

} // namespace wsl
