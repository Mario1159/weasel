#pragma once

#include <entt/entt.hpp>
#include <string>

namespace wsl::comp::singl { class runtime_context; class editor_context; }
namespace editor { struct ecs_selection; }

namespace editor
{
class signal_inspector
{
public:
  signal_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                    wsl::comp::singl::editor_context *editor_ctx,
                    ecs_selection *selection);

  void draw ();

  void open_signal_connection_modal (entt::id_type signal_type,
                                     entt::entity source_entity = entt::null);
  void draw_signal_connection_modal (entt::registry &registry);

private:
  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  ecs_selection *m_selection;

  // Signal connection state
  entt::id_type m_connect_signal_type = 0;
  entt::id_type m_connect_handler_system_type = 0;
  entt::entity m_connect_source_entity = entt::null;
  entt::entity m_connect_target_entity = entt::null;
  bool m_request_open_connect_modal = false;
  char m_connect_handler_search[128]{};
  char m_connect_entity_search[128]{};
  std::string m_connect_handler_name;
  std::string m_connect_modal_error;
  entt::id_type m_selected_signal_type = 0;
};
} // namespace editor
