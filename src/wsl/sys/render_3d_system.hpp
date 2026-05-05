#pragma once

#include "entt/entity/fwd.hpp"
#include "system.hpp"

#include <entt/entt.hpp>


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
