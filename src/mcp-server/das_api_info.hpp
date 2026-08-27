#pragma once

#include <mcp_tool.h>

namespace wsl::mcp_server
{

// MCP tool: describe the Daslang API exposed by Module_WeaselApi.
// No `name` -> grouped function listing; `name` -> full entry.
mcp::json handle_describe_das_api (const mcp::json &params);

} // namespace wsl::mcp_server
