#pragma once

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

#include "entt/entity/fwd.hpp"
#include "system.hpp"


namespace wsl
{

namespace sys
{

class render_ui_system : public sys::ecs_system_t<render_ui_system>
{
public:
  explicit render_ui_system (const std::string &name) : ecs_system_t (name)
  {
    set_relationships ({}, {});
  }
  void register_signals (reg::sig::signal_hub &hub) override;
  void register_event_handlers (reg::sig::signal_hub &hub) override;
  void register_iterations (reg::sig::signal_hub &hub) override;
  void on_init (entt::registry &registry) override;
  void on_update (entt::registry &registry, double dt) override;
  void on_render_build_draw_data (entt::registry &registry) override;
  void on_render_record_draw_cmd (entt::registry &registry) override;
  void on_event (entt::registry &registry, const SDL_Event &ev) override;
};

} // namespace sys

} // namespace wsl
