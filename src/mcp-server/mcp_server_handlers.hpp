#pragma once

#include <mcp_server.h>
#include <string>

namespace wsl::mcp_server
{

mcp::json handle_list_commands (const mcp::json &params);
mcp::json handle_describe_command (const mcp::json &params);
mcp::json handle_get_quick_start (const mcp::json &params);
mcp::json handle_cli_capabilities (const mcp::json &params);

} // namespace wsl::mcp_server
