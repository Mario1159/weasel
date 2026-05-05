#pragma once

#include <imgui.h>
#include <textselect.hpp>
#include <string>
#include <vector>
#include <deque>
#include <functional>

namespace wsl::comp::singl { class runtime_context; class editor_context; }

namespace editor
{

class console
{
public:
  explicit console (wsl::comp::singl::runtime_context *runtime_ctx,
                    wsl::comp::singl::editor_context *editor_ctx);

  void draw (const char *title, bool *open = nullptr);

  void add_line (const std::string &line);
  void clear ();

  // Set the handler that executes commands in the editor's runtime context.
  // If unset, the console falls back to spawning weasel-cli.
  using command_handler_t = std::function<std::string(const std::string&)>;
  void set_command_handler (command_handler_t handler) { m_command_handler = std::move(handler); }

private:
  void execute_command (const std::string &command);
  static int input_callback (ImGuiInputTextCallbackData *data);

  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;

  std::deque<std::string> m_lines;
  char m_input_buffer[512] = "";
  bool m_scroll_to_bottom = false;

  TextSelect m_text_select;
  
  std::vector<std::string> m_history;
  int m_history_pos = -1;

  command_handler_t m_command_handler;
};

} // namespace editor
