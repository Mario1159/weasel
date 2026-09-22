#pragma once

#include "wsl/comp/singl/runtime_context_fwd.hpp"
#include <entt/entt.hpp>
#include <string>

namespace editor
{
struct ecs_selection;
}

namespace editor
{
class event_inspector
{
public:
  event_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                   wsl::comp::singl::editor_context *editor_ctx,
                   ecs_selection *selection);

  void draw ();

  void open_signal_connection_modal (entt::id_type signal_type);
  void draw_signal_connection_modal (entt::registry &registry);

private:
  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  ecs_selection *m_selection;

  // Event connection state
  entt::id_type m_connect_signal_type = 0;
  entt::id_type m_connect_handler_system_type = 0;
  bool m_request_open_connect_modal = false;
  char m_connect_handler_search[128]{};
  std::string m_connect_handler_name;
  std::string m_connect_modal_error;
  entt::id_type m_selected_signal_type = 0;
};
} // namespace editor
