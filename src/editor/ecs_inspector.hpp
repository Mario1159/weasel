#pragma once

#include "entities_and_singletons_panel.hpp"
#include "inspector.hpp"
#include "ecs_inspector_utils.hpp"

#include <entt/entt.hpp>
#include "wsl/events.hpp"

namespace wsl::comp::singl { class runtime_context; class editor_context; }

namespace editor
{

class ecs_inspector
{
public:
  ecs_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                 wsl::comp::singl::editor_context *editor_ctx,
                 ecs_selection &selection);

  ~ecs_inspector ();

  void draw ();
  void on_scene_changed (const wsl::event::scene_changed &e);

private:
  ecs_selection &m_selection;
  wsl::comp::singl::runtime_context *m_runtime_ctx;

  entities_and_singletons_panel m_entities_and_singletons;
  inspector m_inspector;
};

} // namespace editor
