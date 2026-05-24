#include "mcp_server_app.hpp"
#include "cli_reference.hpp"
#include "mcp_stdio_server.hpp"

#include <mcp_server.h>
#include <sstream>

namespace wsl::mcp_server {

namespace {

mcp::json
build_text_response (const std::string &text)
{
  return {
    {
      {"type", "text"},
      {"text", text}
    }
  };
}

mcp::json
handle_list_commands (const mcp::json &params)
{
  (void)params;
  const auto &ref = get_cli_reference ();

  std::ostringstream oss;
  oss << "Available weasel-cli commands:\n\n";

  oss << "== CLI Subcommands ==\n";
  for (const auto &cmd : ref) {
    if (cmd.category == "cli") {
      oss << cmd.name << " - " << cmd.description << "\n";
    }
  }

  oss << "\n== REPL Commands ==\n";
  for (const auto &cmd : ref) {
    if (cmd.category == "repl") {
      oss << cmd.name << " - " << cmd.description << "\n";
    }
  }

  return build_text_response (oss.str ());
}

mcp::json
handle_describe_command (const mcp::json &params)
{
  if (!params.contains ("name")) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                "Missing required parameter: name");
  }

  const std::string name = params["name"].get<std::string> ();
  const cli_command_info *info = find_command (name);

  if (!info) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Unknown command: " + name);
  }

  std::ostringstream oss;
  oss << "Command: " << info->name << "\n";
  oss << "Category: " << info->category << "\n";
  oss << "Description: " << info->description << "\n";
  oss << "Syntax: " << info->syntax << "\n";

  if (!info->parameters.empty ()) {
    oss << "Parameters:\n";
    for (const auto &p : info->parameters) {
      oss << "  - " << p << "\n";
    }
  }

  if (!info->examples.empty ()) {
    oss << "Examples:\n";
    for (const auto &ex : info->examples) {
      oss << "  " << ex << "\n";
    }
  }

  return build_text_response (oss.str ());
}

mcp::json
handle_get_quick_start (const mcp::json &params)
{
  (void)params;
  std::ostringstream oss;
  oss << "Weasel Engine CLI Quick Start\n\n";
  oss << "1. Create a project:\n";
  oss << "   weasel-cli create-project ./mygame MyGame\n\n";
  oss << "2. Create a scene:\n";
  oss << "   weasel-cli create-scene ./mygame/rsc/scenes/level1.wscn.json Level1\n\n";
  oss << "3. Validate a project:\n";
  oss << "   weasel-cli validate-project ./mygame/wslpro.json\n\n";
  oss << "4. Start the interactive REPL:\n";
  oss << "   weasel-cli --project ./mygame/wslpro.json --interactive\n\n";
  oss << "5. Attach to a running editor:\n";
  oss << "   weasel-cli --project ./mygame/wslpro.json --attach\n\n";
  oss << "For full command reference, use the list_commands and describe_command tools.";

  return build_text_response (oss.str ());
}

} // namespace

mcp_server_app::mcp_server_app (const std::string &host, int port)
  : m_server (mcp::server::configuration{host, port})
{
  mcp::json capabilities = {
    {"tools", mcp::json::object ()}
  };

  m_server.set_server_info ("Weasel MCP Server", "0.1.0");
  m_server.set_capabilities (capabilities);
  m_server.set_instructions (
    "This MCP server provides reference documentation for the Weasel Engine "
    "CLI and REPL commands.");

  m_stdio_server.set_server_info ("Weasel MCP Server", "0.1.0");
  m_stdio_server.set_capabilities (capabilities);
  m_stdio_server.set_instructions (
    "This MCP server provides reference documentation for the Weasel Engine "
    "CLI and REPL commands.");

  register_tools ();
}

void
mcp_server_app::set_stdio_mode (bool enable)
{
  stdio_mode_ = enable;
}

void
mcp_server_app::run ()
{
  if (stdio_mode_) {
    m_stdio_server.run ();
  } else {
    m_server.start (true);
  }
}

void
mcp_server_app::register_tools ()
{
  mcp::tool list_tool = mcp::tool_builder ("list_commands")
                          .with_description (
                            "List all available weasel-cli and REPL commands.")
                          .build ();
  m_server.register_tool (list_tool, [] (const mcp::json &params,
                                          const std::string & /*session_id*/) {
    return handle_list_commands (params);
  });
  m_stdio_server.register_tool (list_tool, [] (const mcp::json &params,
                                                const std::string & /*session_id*/) {
    return handle_list_commands (params);
  });

  mcp::tool desc_tool = mcp::tool_builder ("describe_command")
                          .with_description (
                            "Get detailed usage information for a specific command.")
                          .with_string_param ("name",
                                               "Name of the command to describe.")
                          .build ();
  m_server.register_tool (desc_tool, [] (const mcp::json &params,
                                          const std::string & /*session_id*/) {
    return handle_describe_command (params);
  });
  m_stdio_server.register_tool (desc_tool, [] (const mcp::json &params,
                                                const std::string & /*session_id*/) {
    return handle_describe_command (params);
  });

  mcp::tool quick_tool = mcp::tool_builder ("get_quick_start")
                            .with_description (
                              "Return a quick-start guide for the Weasel CLI.")
                            .build ();
  m_server.register_tool (quick_tool, [] (const mcp::json &params,
                                            const std::string & /*session_id*/) {
    return handle_get_quick_start (params);
  });
  m_stdio_server.register_tool (quick_tool, [] (const mcp::json &params,
                                                  const std::string & /*session_id*/) {
    return handle_get_quick_start (params);
  });
}

} // namespace wsl::mcp_server
