#include "chat_panel.hpp"

#include "renderer_imgui.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/ai/a2a/json_util.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <simdjson.h>

#include <filesystem>

namespace editor
{

chat_panel::chat_panel (wsl::comp::singl::runtime_context * /*runtime_ctx*/,
                        wsl::comp::singl::editor_context * /*editor_ctx*/)
    : m_session (m_client)
{
  m_available_agents = m_agent_manager.discover_agents ();
  if (!m_available_agents.empty ()) {
    m_selected_agent = m_available_agents[0].name;
  }

  m_session.set_update_handler (
      [this] (const std::string &method, const std::string &params) {
        on_session_update (method, params);
      });

  m_session.set_agent_request_handler (
      [this] (const std::string &method,
              const std::string &params) -> std::string {
        return on_agent_request (method, params);
      });
}

chat_panel::~chat_panel ()
{
  if (m_connect_thread.joinable ()) {
    m_connect_thread.join ();
  }
  if (m_prompt_thread.joinable ()) {
    m_prompt_thread.join ();
  }
  if (m_connected) {
    m_session.close_session ();
    m_client.terminate ();
  }
}

void
chat_panel::draw (const char *title, bool *open)
{
  // Poll for background connect completion
  if (m_initializing && m_connect_done.load ()) {
    if (m_connect_thread.joinable ()) {
      m_connect_thread.join ();
    }

    // Collect messages from background thread
    {
      std::lock_guard<std::mutex> lock (m_msg_mutex);
      for (auto &msg : m_pending_messages) {
        m_messages.push_back (std::move (msg));
      }
      m_pending_messages.clear ();
    }
    m_scroll_to_bottom = true;

    if (m_connect_result.load ()) {
      m_connected = true;
      m_initializing = false;

      display_message info_msg;
      info_msg.m_role = display_message::role::system;
      info_msg.m_text = "Connected to " + m_selected_agent
                        + " (session: " + m_connect_session_id + ")";
      m_messages.push_back (info_msg);
      m_scroll_to_bottom = true;
    } else {
      m_initializing = false;
      display_message err_msg;
      err_msg.m_role = display_message::role::system;
      err_msg.m_text = m_connect_error;
      m_messages.push_back (err_msg);
      m_scroll_to_bottom = true;
    }
    m_connect_done.store (false);
  }

  // Poll for background prompt completion
  if (m_prompt_done.load ()) {
    if (m_prompt_thread.joinable ()) {
      m_prompt_thread.join ();
    }
    // Mark streaming as done on the last assistant message
    if (!m_messages.empty ()
        && m_messages.back ().m_role == display_message::role::assistant
        && m_messages.back ().m_streaming) {
      m_messages.back ().m_streaming = false;
    }
    m_prompt_done.store (false);
  }

  if (!ImGui::Begin (title, open)) {
    ImGui::End ();
    return;
  }

  draw_toolbar ();

  if (m_connected) {
    draw_config_options ();
  }

  if (m_show_plan && !m_plan.empty ()) {
    draw_plan ();
    ImGui::Separator ();
  }

  draw_messages ();
  ImGui::Separator ();
  draw_input ();

  if (m_pending_permission) {
    render_permission_dialog ();
  }

  ImGui::End ();
}

void
chat_panel::draw_toolbar ()
{
  ImGui::PushStyleVar (ImGuiStyleVar_FramePadding, ImVec2 (4, 2));

  // Agent selector
  ImGui::TextUnformatted ("Agent:");
  ImGui::SameLine ();
  ImGui::SetNextItemWidth (120);

  int selected_idx = 0;
  std::string combo_items;
  for (size_t i = 0; i < m_available_agents.size (); ++i) {
    combo_items += m_available_agents[i].display_name;
    combo_items += '\0';
    if (m_available_agents[i].name == m_selected_agent) {
      selected_idx = static_cast<int> (i);
    }
  }
  if (!combo_items.empty ()) {
    combo_items.pop_back ();
  }

  if (!m_connected && !combo_items.empty ()) {
    if (ImGui::Combo ("##agent", &selected_idx, combo_items.c_str ())) {
      m_selected_agent = m_available_agents[selected_idx].name;
    }
  } else if (m_connected) {
    ImGui::TextDisabled ("%s", m_selected_agent.c_str ());
  }

  // Status
  ImGui::SameLine ();
  if (m_connected) {
    ImGui::TextColored (ImVec4 (0.2F, 0.8F, 0.2F, 1.0F), "● Connected");
  } else if (m_initializing) {
    ImGui::TextColored (ImVec4 (0.8F, 0.8F, 0.2F, 1.0F), "● Connecting...");
  } else {
    ImGui::TextColored (ImVec4 (0.5F, 0.5F, 0.5F, 1.0F), "● Disconnected");
  }

  // Actions
  ImGui::SameLine ();
  float const avail = ImGui::GetContentRegionAvail ().x;
  ImGui::SetCursorPosX (ImGui::GetCursorPosX () + avail - 120);

  ImGui::PushID ("toolbar");

  if (!m_connected) {
    if (ImGui::Button ("Connect", ImVec2 (70, 0))) {
      handle_connect ();
    }
  } else {
    if (m_session.is_processing ()) {
      if (ImGui::Button ("Stop", ImVec2 (50, 0))) {
        handle_cancel ();
      }
    } else {
      ImGui::BeginDisabled ();
      ImGui::Button ("Stop", ImVec2 (50, 0));
      ImGui::EndDisabled ();
    }
    ImGui::SameLine ();
    if (ImGui::Button ("Disconnect", ImVec2 (70, 0))) {
      handle_disconnect ();
    }
  }

  ImGui::PopID ();

  ImGui::PopStyleVar ();
}

void
chat_panel::draw_config_options ()
{
  auto const &opts = m_session.config_options ();
  if (opts.empty ()) {
    return;
  }

  ImGui::Separator ();
  ImGui::PushStyleVar (ImGuiStyleVar_FramePadding, ImVec2 (4, 2));

  for (auto const &opt : opts) {
    if (opt.type != wsl::ai::acp::config_option_type::select) {
      continue;
    }

    ImGui::TextDisabled ("%s:", opt.name.c_str ());
    ImGui::SameLine ();
    ImGui::SetNextItemWidth (150);

    // Build combo items
    int selected_idx = 0;
    std::string combo_items;
    for (size_t i = 0; i < opt.options.size (); ++i) {
      combo_items += opt.options[i].name;
      combo_items += '\0';
      if (opt.options[i].value == opt.current_value) {
        selected_idx = static_cast<int> (i);
      }
    }
    if (!combo_items.empty ()) {
      combo_items.pop_back ();
    }

    std::string combo_id = "##" + opt.id;
    if (!combo_items.empty ()
        && ImGui::Combo (combo_id.c_str (), &selected_idx,
                         combo_items.c_str ())) {
      std::string new_value = opt.options[selected_idx].value;
      m_session.set_config_option (opt.id, new_value);
    }
  }

  ImGui::PopStyleVar ();
}

void
chat_panel::draw_plan ()
{
  ImGui::TextDisabled ("Plan");
  ImGui::Indent ();

  for (const auto &entry : m_plan) {
    bool checked = entry.status == "completed";
    ImGui::PushID (entry.content.c_str ());

    if (ImGui::Checkbox ("##plan", &checked)) {
    }

    ImGui::SameLine ();
    if (entry.status == "completed") {
      ImGui::TextColored (ImVec4 (0.5F, 0.5F, 0.5F, 1.0F), "%s",
                          entry.content.c_str ());
    } else if (entry.status == "in_progress") {
      ImGui::TextColored (ImVec4 (0.8F, 0.8F, 0.2F, 1.0F), "%s",
                          entry.content.c_str ());
    } else {
      ImGui::TextUnformatted (entry.content.c_str ());
    }

    ImGui::PopID ();
  }

  ImGui::Unindent ();
}

void
chat_panel::draw_messages ()
{
  float const footer_height_to_reserve
      = ImGui::GetStyle ().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing ();

  ImGui::BeginChild ("scrolling_region", ImVec2 (0, -footer_height_to_reserve),
                     false, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto &msg : m_messages) {
    ImGui::PushID (&msg);

    // Role label
    const char *role_label = "";
    ImVec4 role_color;
    switch (msg.m_role) {
    case display_message::role::user:
      role_label = "You";
      role_color = ImVec4 (0.4F, 0.7F, 1.0F, 1.0F);
      break;
    case display_message::role::assistant:
      role_label = "Agent";
      role_color = ImVec4 (0.6F, 0.9F, 0.6F, 1.0F);
      break;
    case display_message::role::system:
      role_label = "System";
      role_color = ImVec4 (0.8F, 0.8F, 0.4F, 1.0F);
      break;
    }

    ImGui::TextColored (role_color, "%s", role_label);

    // Message content
    ImGui::Indent ();
    ImGui::PushTextWrapPos (ImGui::GetCursorPosX ()
                            + ImGui::GetContentRegionAvail ().x);

    if (!msg.m_text.empty ()) {
      ImGui::TextUnformatted (msg.m_text.c_str ());
    }

    // Tool calls
    for (const auto &tc : msg.m_tool_calls) {
      render_tool_call (tc);
    }

    ImGui::PopTextWrapPos ();
    ImGui::Unindent ();
    ImGui::Separator ();
    ImGui::Spacing ();

    ImGui::PopID ();
  }

  if (m_scroll_to_bottom) {
    ImGui::SetScrollHereY (1.0F);
    m_scroll_to_bottom = false;
  }

  ImGui::EndChild ();
}

void
chat_panel::draw_input ()
{
  bool reclaim_focus = false;
  ImGuiInputTextFlags const input_flags = ImGuiInputTextFlags_EnterReturnsTrue;

  ImGui::PushItemWidth (-80);
  if (ImGui::InputText ("##input", m_input_buffer,
                        IM_ARRAYSIZE (m_input_buffer), input_flags)) {
    handle_send ();
    reclaim_focus = true;
  }
  ImGui::PopItemWidth ();

  ImGui::SameLine ();

  ImGui::PushID ("input");

  if (m_session.is_processing ()) {
    if (ImGui::Button ("Stop", ImVec2 (70, 0))) {
      handle_cancel ();
    }
  } else {
    bool const has_text = m_input_buffer[0] != '\0';
    bool const can_send = m_connected && !m_initializing && has_text;

    if (!can_send) {
      ImGui::BeginDisabled ();
    }
    if (ImGui::Button ("Send", ImVec2 (70, 0))) {
      handle_send ();
      reclaim_focus = true;
    }
    if (!can_send) {
      ImGui::EndDisabled ();
    }
  }

  ImGui::PopID ();

  ImGui::SetItemDefaultFocus ();
  if (reclaim_focus) {
    ImGui::SetKeyboardFocusHere (-1);
  }
}

void
chat_panel::handle_send ()
{
  if (m_input_buffer[0] == '\0' || !m_connected) {
    return;
  }

  std::string text (m_input_buffer);

  // Add user message to display
  display_message user_msg;
  user_msg.m_role = display_message::role::user;
  user_msg.m_text = text;
  m_messages.push_back (user_msg);
  m_scroll_to_bottom = true;

  // Clear input
  m_input_buffer[0] = '\0';

  // Prepare assistant message placeholder BEFORE sending prompt
  // so on_session_update can append chunks immediately
  display_message assistant_msg;
  assistant_msg.m_role = display_message::role::assistant;
  assistant_msg.m_streaming = true;
  m_messages.push_back (assistant_msg);
  m_scroll_to_bottom = true;

  // Send to agent (non-blocking)
  prompt_async (text);
}

void
chat_panel::prompt_async (const std::string &text)
{
  // Join any previous prompt thread
  if (m_prompt_thread.joinable ()) {
    m_prompt_thread.join ();
  }

  m_prompt_done.store (false);

  m_prompt_thread = std::thread ([this, text] {
    if (!m_session.prompt (text)) {
      spdlog::error ("[chat] prompt_async: prompt failed");
    }

    m_prompt_done.store (true);
  });
}

void
chat_panel::handle_connect ()
{
  if (m_connected || m_initializing) {
    return;
  }

  // Find selected agent
  const wsl::ai::acp::agent_entry *agent = nullptr;
  for (const auto &a : m_available_agents) {
    if (a.name == m_selected_agent) {
      agent = &a;
      break;
    }
  }

  if (!agent) {
    spdlog::error ("[chat] No agent selected");
    return;
  }

  m_initializing = true;
  m_connect_result.store (false);
  m_connect_error.clear ();
  m_connect_session_id.clear ();

  display_message info_msg;
  info_msg.m_role = display_message::role::system;
  info_msg.m_text = "Connecting to " + agent->display_name + "...";
  m_messages.push_back (info_msg);
  m_scroll_to_bottom = true;

  // Launch agent subprocess (fast, non-blocking)
  if (!m_agent_manager.launch (m_client, *agent)) {
    spdlog::error ("[chat] Failed to launch agent: {}", agent->command);
    m_initializing = false;

    display_message err_msg;
    err_msg.m_role = display_message::role::system;
    err_msg.m_text
        = "Error: Failed to launch agent '" + agent->display_name + "'";
    m_messages.push_back (err_msg);
    m_scroll_to_bottom = true;
    return;
  }

  // Run initialize + new_session in background thread
  m_connect_thread = std::thread ([this] { connect_async (); });
}

void
chat_panel::connect_async ()
{
  wsl::ai::acp::client_capabilities caps;
  caps.fs.read_text_file = true;
  caps.fs.write_text_file = true;
  caps.terminal = true;

  wsl::ai::acp::implementation_info info;
  info.name = "weasel";
  info.title = "Weasel Engine";
  info.version = "1.0.0";

  if (!m_session.initialize (caps, info)) {
    m_connect_error = "Error: Agent initialization failed";
    m_client.terminate ();
    m_connect_done.store (true);
    return;
  }

  std::string cwd = std::filesystem::current_path ().string ();
  std::string session_id = m_session.new_session (cwd);

  if (session_id.empty ()) {
    m_connect_error = "Error: Failed to create session";
    m_client.terminate ();
    m_connect_done.store (true);
    return;
  }

  m_connect_session_id = session_id;
  m_connect_result.store (true);
  m_connect_done.store (true);
}

void
chat_panel::handle_disconnect ()
{
  if (!m_connected) {
    return;
  }

  // Wait for any in-flight prompt to finish
  if (m_prompt_thread.joinable ()) {
    m_prompt_thread.join ();
  }

  m_session.close_session ();
  m_client.terminate ();
  m_connected = false;
  m_plan.clear ();
  m_pending_permission.reset ();

  display_message info_msg;
  info_msg.m_role = display_message::role::system;
  info_msg.m_text = "Disconnected";
  m_messages.push_back (info_msg);
  m_scroll_to_bottom = true;
}

void
chat_panel::handle_cancel ()
{
  if (m_connected && m_session.is_processing ()) {
    m_session.cancel ();

    display_message info_msg;
    info_msg.m_role = display_message::role::system;
    info_msg.m_text = "Cancelled";
    m_messages.push_back (info_msg);
    m_scroll_to_bottom = true;
  }
}

void
chat_panel::render_content_block (const wsl::ai::acp::content_block &block)
{
  if (auto *text = std::get_if<wsl::ai::acp::text_content> (&block)) {
    ImGui::TextUnformatted (text->text.c_str ());
  } else if (auto *img = std::get_if<wsl::ai::acp::image_content> (&block)) {
    ImGui::TextDisabled ("[Image: %s]", img->mime_type.c_str ());
  } else if (auto *res
             = std::get_if<wsl::ai::acp::resource_link_content> (&block)) {
    ImGui::TextDisabled ("[Resource: %s]", res->name.c_str ());
  }
}

void
chat_panel::render_tool_call (const wsl::ai::acp::tool_call_update &tc)
{
  ImGui::PushStyleColor (ImGuiCol_ChildBg, ImVec4 (0.15F, 0.15F, 0.2F, 0.5F));
  ImGui::Indent ();

  float const avail = ImGui::GetContentRegionAvail ().x;
  ImGui::BeginChild ("tool_call", ImVec2 (avail, 0), ImGuiChildFlags_Borders);

  // Status icon
  const char *status_icon = "○";
  if (tc.status) {
    switch (*tc.status) {
    case wsl::ai::acp::tool_call_status::pending:
      status_icon = "○";
      break;
    case wsl::ai::acp::tool_call_status::in_progress:
      status_icon = "◐";
      break;
    case wsl::ai::acp::tool_call_status::completed:
      status_icon = "●";
      break;
    case wsl::ai::acp::tool_call_status::failed:
      status_icon = "✗";
      break;
    }
  }

  // Title
  std::string title = "Tool Call";
  if (tc.title) {
    title = *tc.title;
  } else if (tc.kind) {
    title = wsl::ai::acp::tool_kind_name (*tc.kind);
  }

  ImGui::Text ("%s %s", status_icon, title.c_str ());

  // Locations
  if (tc.locations) {
    for (const auto &loc : *tc.locations) {
      ImGui::TextDisabled ("  %s", loc.path.c_str ());
    }
  }

  ImGui::EndChild ();
  ImGui::Unindent ();
  ImGui::PopStyleColor ();
}

void
chat_panel::render_permission_dialog ()
{
  ImGui::SetNextWindowSize (ImVec2 (400, 0), ImGuiCond_Always);
  ImGui::SetNextWindowPos (ImGui::GetMainViewport ()->GetCenter (),
                           ImGuiCond_Always, ImVec2 (0.5F, 0.5F));

  ImGui::PushStyleVar (ImGuiStyleVar_WindowPadding, ImVec2 (16, 16));

  if (ImGui::Begin ("Permission Required", nullptr,
                    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoMove)) {

    ImGui::Text ("Agent requests permission:");

    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();

    if (m_pending_permission->title) {
      ImGui::TextWrapped ("%s", m_pending_permission->title->c_str ());
    }

    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();

    float const btn_width = 90.0F;
    float const total = btn_width * 4 + ImGui::GetStyle ().ItemSpacing.x * 3;
    ImGui::SetCursorPosX ((ImGui::GetWindowSize ().x - total) * 0.5F);

    if (ImGui::Button ("Allow Once", ImVec2 (btn_width, 0))) {
      m_pending_permission.reset ();
    }
    ImGui::SameLine ();
    if (ImGui::Button ("Allow Always", ImVec2 (btn_width, 0))) {
      m_pending_permission.reset ();
    }
    ImGui::SameLine ();
    if (ImGui::Button ("Reject Once", ImVec2 (btn_width, 0))) {
      m_pending_permission.reset ();
    }
    ImGui::SameLine ();
    if (ImGui::Button ("Reject", ImVec2 (btn_width, 0))) {
      m_pending_permission.reset ();
    }
  }

  ImGui::End ();
  ImGui::PopStyleVar ();
}

void
chat_panel::on_session_update (const std::string & /*method*/,
                               const std::string &params)
{
  simdjson::dom::parser parser;
  auto doc = parser.parse (params);

  auto update_el = doc["update"];
  if (update_el.error ()) {
    return;
  }

  auto update = update_el.value ();
  auto session_update = update["sessionUpdate"];
  if (session_update.error ()) {
    return;
  }

  std::string_view update_type;
  if (session_update.get_string ().get (update_type) != 0) {
    return;
  }

  if (update_type == "agent_message_chunk") {
    // Text chunk from assistant
    auto content_el = update["content"];
    if (!content_el.error ()) {
      auto type_el = content_el["type"];
      if (!type_el.error ()) {
        std::string_view type;
        if (type_el.get_string ().get (type) == 0 && type == "text") {
          auto text_el = content_el["text"];
          if (!text_el.error ()) {
            std::string_view text;
            if (text_el.get_string ().get (text) == 0) {
              // Append to last assistant message
              if (!m_messages.empty ()
                  && m_messages.back ().m_role
                         == display_message::role::assistant) {
                m_messages.back ().m_text += std::string (text);
                m_scroll_to_bottom = true;
              }
            }
          }
        }
      }
    }
  } else if (update_type == "tool_call") {
    // Tool call update
    wsl::ai::acp::tool_call_update tc;

    auto id_el = update["toolCallId"];
    if (!id_el.error ()) {
      std::string_view id;
      if (id_el.get_string ().get (id) == 0) {
        tc.tool_call_id = std::string (id);
      }
    }

    auto title_el = update["title"];
    if (!title_el.error ()) {
      std::string_view title;
      if (title_el.get_string ().get (title) == 0) {
        tc.title = std::string (title);
      }
    }

    auto kind_el = update["kind"];
    if (!kind_el.error ()) {
      std::string_view kind;
      if (kind_el.get_string ().get (kind) == 0) {
        auto parsed = wsl::ai::acp::parse_tool_kind (std::string (kind));
        if (parsed) {
          tc.kind = *parsed;
        }
      }
    }

    auto status_el = update["status"];
    if (!status_el.error ()) {
      std::string_view status;
      if (status_el.get_string ().get (status) == 0) {
        auto parsed
            = wsl::ai::acp::parse_tool_call_status (std::string (status));
        if (parsed) {
          tc.status = *parsed;
        }
      }
    }

    // Add to last assistant message
    if (!m_messages.empty ()
        && m_messages.back ().m_role == display_message::role::assistant) {
      m_messages.back ().m_tool_calls.push_back (tc);
      m_scroll_to_bottom = true;
    }
  } else if (update_type == "plan") {
    // Plan update
    auto entries_el = update["entries"];
    if (!entries_el.error () && entries_el.value ().is_array ()) {
      m_plan.clear ();
      for (auto entry : entries_el.value ().get_array ()) {
        wsl::ai::acp::plan_entry pe;

        auto content_el2 = entry["content"];
        if (!content_el2.error ()) {
          std::string_view content;
          if (content_el2.get_string ().get (content) == 0) {
            pe.content = std::string (content);
          }
        }

        auto priority_el = entry["priority"];
        if (!priority_el.error ()) {
          std::string_view priority;
          if (priority_el.get_string ().get (priority) == 0) {
            pe.priority = std::string (priority);
          }
        }

        auto status_el2 = entry["status"];
        if (!status_el2.error ()) {
          std::string_view status;
          if (status_el2.get_string ().get (status) == 0) {
            pe.status = std::string (status);
          }
        }

        m_plan.push_back (pe);
      }
      m_show_plan = !m_plan.empty ();
    }
  } else if (update_type == "permission") {
    // Permission request
    wsl::ai::acp::tool_call_update tc;
    tc.tool_call_id = "permission";

    auto title_el = update["title"];
    if (!title_el.error ()) {
      std::string_view title;
      if (title_el.get_string ().get (title) == 0) {
        tc.title = std::string (title);
      }
    }

    m_pending_permission = tc;

    // Parse permission options
    auto options_el = update["options"];
    if (!options_el.error () && options_el.value ().is_array ()) {
      m_permission_options.clear ();
      for (auto opt : options_el.value ().get_array ()) {
        wsl::ai::acp::permission_option po;

        auto id_el = opt["id"];
        if (!id_el.error ()) {
          std::string_view id;
          if (id_el.get_string ().get (id) == 0) {
            po.option_id = std::string (id);
          }
        }

        auto name_el = opt["name"];
        if (!name_el.error ()) {
          std::string_view name;
          if (name_el.get_string ().get (name) == 0) {
            po.name = std::string (name);
          }
        }

        m_permission_options.push_back (po);
      }
    }
  } else if (update_type == "completed") {
    // Turn completed
    if (!m_messages.empty ()
        && m_messages.back ().m_role == display_message::role::assistant) {
      m_messages.back ().m_streaming = false;
    }
  }
}

std::string
chat_panel::on_agent_request (const std::string &method,
                              const std::string & /*params*/)
{
  // Handle fs, terminal, and other agent requests
  // For now, return an error for unhandled requests
  return R"({"error":{"code":-32601,"message":"Method not implemented"}})";
}

} // namespace editor
