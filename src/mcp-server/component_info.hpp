#pragma once

#include <mcp_server.h>
#include <string>

namespace wsl::mcp_server {

mcp::json handle_list_components(const mcp::json& params);
mcp::json handle_describe_component(const mcp::json& params);

} // namespace wsl::mcp_server
