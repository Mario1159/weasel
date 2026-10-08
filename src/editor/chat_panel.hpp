#pragma once

#include "wsl/comp/singl/runtime_context_fwd.hpp"
#include <agentsdk/acp/acp_agent_manager.hpp>
#include <agentsdk/acp/acp_client.hpp>
#include <agentsdk/acp/acp_session.hpp>
#include <agentsdk/acp/acp_types.hpp>

#include <imgui.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace editor
{

class chat_panel
{
public:
  chat_panel (wsl::comp::singl::runtime_context *runtime_ctx,
              wsl::comp::singl::editor_context *editor_ctx);
  ~chat_panel ();

  void draw (const char *title, bool *open = nullptr);

private:
  void draw_toolbar ();
  void draw_config_options ();
  void draw_plan ();
  void draw_messages ();
  void draw_input ();
  void handle_send ();
  void handle_connect ();
  void handle_disconnect ();
  void handle_cancel ();

  void connect_async ();
  void prompt_async (const std::string &text);

  void render_content_block (const agentsdk::acp::content_block &block);
  void render_tool_call (const agentsdk::acp::tool_call_update &tc);
  void render_permission_dialog ();

  void on_session_update (const std::string &method, const std::string &params);
  void apply_tool_call_update (agentsdk::acp::tool_call_update &&update);
  agentsdk::acp::agent_response on_agent_request (const std::string &method,
                                                 const std::string &params);

  agentsdk::acp::acp_client m_client;
  agentsdk::acp::acp_session m_session;
  agentsdk::acp::acp_agent_manager m_agent_manager;

  // Not owned.
  wsl::comp::singl::runtime_context *m_runtime_ctx = nullptr;

  // Agent state
  bool m_connected = false;
  bool m_initializing = false;
  std::string m_selected_agent;
  std::vector<agentsdk::acp::agent_entry> m_available_agents;

  // Conversation
  struct display_message
  {
    enum class role
    {
      user,
      assistant,
      system
    };
    role m_role;
    std::string m_text;
    std::string m_message_id;
    bool m_streaming = false;
    std::vector<agentsdk::acp::tool_call_update> m_tool_calls;
  };

  // Background connect thread
  std::thread m_connect_thread;
  std::atomic<bool> m_connect_result{ false };
  std::atomic<bool> m_connect_done{ false };
  std::string m_connect_error;
  std::string m_connect_session_id;

  // Background prompt thread
  std::thread m_prompt_thread;
  std::atomic<bool> m_prompt_done{ false };

  // Thread-safe message queue for messages from background threads
  std::mutex m_msg_mutex;
  std::vector<display_message> m_pending_messages;

  std::vector<display_message> m_messages;

  // Plan
  std::vector<agentsdk::acp::plan_entry> m_plan;
  bool m_show_plan = false;

  // Permission request
  std::optional<agentsdk::acp::tool_call_update> m_pending_permission;
  std::vector<agentsdk::acp::permission_option> m_permission_options;

  // Config options
  std::string m_selected_model;

  // Input
  char m_input_buffer[4096] = "";
  bool m_scroll_to_bottom = false;
};

} // namespace editor
