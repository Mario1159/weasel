#pragma once

#ifndef IN_MODULE_INTERFACE
#include <RmlUi/Core.h>
#endif
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif

#include "entt/entity/fwd.hpp"
#include "system.hpp"
#include "wsl/event.hpp"

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
  void register_event_sources (event::event_hub &hub) override;
  void register_iterations (event::event_hub &hub) override;
  void on_init (entt::registry &registry) override;
  void on_update (entt::registry &registry, double dt) override;
  void on_render_build_draw_data (entt::registry &registry) override;
  void on_render_record_draw_cmd (entt::registry &registry) override;

  // Raw-SDL input path for the embedded game viewport. RmlUI still needs the
  // original SDL event (with viewport-adjusted coordinates), which the
  // pull-only message bus cannot supply, so this is fed directly from the
  // editor poll loop rather than the removed `ecs_system::on_event` virtual.
  void handle_sdl_event (entt::registry &registry, const engine_event &ev);
};

} // namespace sys

} // namespace wsl
