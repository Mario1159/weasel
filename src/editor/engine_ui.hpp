#pragma once

#include "root.hpp"
#include "wsl/reg/sig/signal_hub.hpp"

#include <SDL3/SDL.h>
#include <entt/entity/fwd.hpp>
#include <imgui.h>


namespace wsl::comp::singl
{
class runtime_context;
class editor_context;
}

namespace editor
{

class engine_ui
{
public:
  struct game_focus_toggled
  {
    bool focused = true;
  };

  engine_ui (wsl::comp::singl::runtime_context *runtime_ctx,
             wsl::comp::singl::editor_context *editor_ctx);

  static void register_signals (wsl::reg::sig::signal_hub &hub);
  void initialize ();
  void set_console_command_handler (console::command_handler_t handler);
  void handle_event (const SDL_Event &event);
  void build_draw_data (entt::registry &registry);
  void prepare_gpu_resources ();
  void record_draw_commands ();

private:
  wsl::comp::singl::runtime_context *m_runtime_ctx = nullptr;
  wsl::comp::singl::editor_context *m_editor_ctx = nullptr;
  root m_root;
  ImDrawData *m_draw_data = nullptr;
  bool m_game_focus = true;
};

} // namespace editor
