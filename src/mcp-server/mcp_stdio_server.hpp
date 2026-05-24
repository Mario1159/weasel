#pragma once

#include <mcp_message.h>
#include <mcp_tool.h>
#include <functional>
#include <map>
#include <string>

namespace wsl::mcp_server {

/*!
 * \brief Simple MCP server using stdio transport (line-delimited JSON-RPC).
 *
 * This is used by OpenCode and other MCP clients that launch a local process
 * and communicate over stdin/stdout.
 */
class mcp_stdio_server {
public:
  using method_handler = std::function<mcp::json(const mcp::json &, const std::string &)>;
  using tool_handler = method_handler;

  mcp_stdio_server ();

  void set_server_info (const std::string &name, const std::string &version);
  void set_capabilities (const mcp::json &capabilities);
  void set_instructions (const std::string &instructions);

  void register_tool (const mcp::tool &tool, tool_handler handler);

  /*!\brief Reads JSON-RPC requests from stdin and writes responses to stdout. */
  void run ();

private:
  mcp::json handle_initialize (const mcp::request &req);
  mcp::json process_request (const mcp::request &req);

  void send_response (const mcp::json &response);

  std::string name_;
  std::string version_;
  mcp::json capabilities_;
  std::string instructions_;

  std::map<std::string, std::pair<mcp::tool, tool_handler>> tools_;
  std::map<std::string, method_handler> method_handlers_;

  bool initialized_ = false;
};

} // namespace wsl::mcp_server
