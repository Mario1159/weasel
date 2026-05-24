#include "console.hpp"
#include "imgui_internal.h"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"

#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>

namespace editor
{

console::console (wsl::comp::singl::runtime_context *runtime_ctx,
                 wsl::comp::singl::editor_context *editor_ctx)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_text_select (
          [this] (std::size_t idx) -> std::string_view {
            return m_lines[idx];
          },
          [this] () -> std::size_t { return m_lines.size (); })
{
}

void
console::add_line (const std::string &line)
{
  m_lines.push_back (line);
  m_scroll_to_bottom = true;
}

void
console::clear ()
{
  m_lines.clear ();
  m_text_select.clearSelection ();
}

void
console::execute_command (const std::string &command)
{
  add_line ("> " + command);
  
  // History management
  m_history.push_back (command);
  m_history_pos = -1;

  if (m_command_handler) {
    // Execute directly in the editor's runtime context
    std::string output = m_command_handler (command);
    if (!output.empty ()) {
      // Split output by newlines and add each line
      std::istringstream iss (output);
      std::string line;
      while (std::getline (iss, line)) {
        // Strip possible trailing \r
        if (!line.empty () && line.back () == '\r') line.pop_back ();
        add_line (line);
      }
    }
    return;
  }

  // Fallback: spawn weasel-cli binary
  // Note: --project is no longer passed here because it now requires
  // --interactive, and the console fallback runs one-shot commands.
  std::string full_cmd = "./weasel-cli " + command + " 2>&1";
  
  FILE* pipe = popen(full_cmd.c_str(), "r");
  if (!pipe) {
    add_line("Error: Failed to execute weasel-cli");
    return;
  }

  char buffer[128];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    std::string line(buffer);
    if (!line.empty() && line.back() == '\n') line.pop_back();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    add_line(line);
  }

  pclose(pipe);
}

int
console::input_callback (ImGuiInputTextCallbackData *data)
{
  auto *console_ptr = static_cast<console *> (data->UserData);
  switch (data->EventFlag) {
  case ImGuiInputTextFlags_CallbackHistory: {
    const int prev_pos = console_ptr->m_history_pos;
    if (data->EventKey == ImGuiKey_UpArrow) {
      if (console_ptr->m_history_pos == -1)
        console_ptr->m_history_pos = (int)console_ptr->m_history.size () - 1;
      else if (console_ptr->m_history_pos > 0)
        --console_ptr->m_history_pos;
    } else if (data->EventKey == ImGuiKey_DownArrow) {
      if (console_ptr->m_history_pos != -1)
        if (++console_ptr->m_history_pos >= (int)console_ptr->m_history.size ())
          console_ptr->m_history_pos = -1;
    }

    if (prev_pos != console_ptr->m_history_pos) {
      const char *str
          = (console_ptr->m_history_pos >= 0)
                ? console_ptr->m_history[console_ptr->m_history_pos].c_str ()
                : "";
      data->DeleteChars (0, data->BufTextLen);
      data->InsertChars (0, str);
    }
    break;
  }
  case ImGuiInputTextFlags_CallbackCompletion:
    // TODO: implement tab completion if desired
    break;
  }
  return 0;
}

void
console::draw (const char *title, bool *open)
{
  if (!ImGui::Begin (title, open)) {
    ImGui::End ();
    return;
  }

  // Reserve enough left-over height for 1 separator + 1 input text
  const float footer_height_to_reserve
      = ImGui::GetStyle ().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing ();
  
  ImGui::BeginChild ("scrolling_region", ImVec2 (0, -footer_height_to_reserve), 
                    false, ImGuiWindowFlags_HorizontalScrollbar);

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts().mono);

  for (const auto &line : m_lines) {
    ImGui::TextUnformatted (line.c_str ());
  }

  m_text_select.update ();

  if (m_scroll_to_bottom) {
    ImGui::SetScrollHereY (1.0F);
    m_scroll_to_bottom = false;
  }

  ImGui::PopFont ();
  ImGui::EndChild ();

  ImGui::Separator ();

  // Command-line input
  bool reclaim_focus = false;
  ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
  
  ImGui::PushItemWidth (-1);
  if (ImGui::InputText ("##Input", m_input_buffer, IM_ARRAYSIZE (m_input_buffer), input_text_flags, &input_callback, (void*)this)) {
    std::string cmd (m_input_buffer);
    if (!cmd.empty ()) {
      execute_command (cmd);
    }
    m_input_buffer[0] = '\0';
    reclaim_focus = true;
  }
  ImGui::PopItemWidth ();

  // Auto-focus on window apparition
  ImGui::SetItemDefaultFocus ();
  if (reclaim_focus) {
    ImGui::SetKeyboardFocusHere (-1); // Auto focus previous widget
  }

  ImGui::End ();
}

} // namespace editor
