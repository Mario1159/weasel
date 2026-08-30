#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "system.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif

namespace wsl::sys
{

/** System for submitting 2D sprites to the batch renderer. */
class render_2d_system : public sys::ecs_system_t<render_2d_system>
{
public:
  explicit render_2d_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({}, {});
  }

  void on_render_build_draw_data (entt::registry &registry) override;
  void on_render_record_draw_cmd (entt::registry &registry) override;
};

} // namespace wsl::sys
