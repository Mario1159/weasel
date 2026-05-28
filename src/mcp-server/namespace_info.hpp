#pragma once

#include <mcp_server.h>
#include <string>

namespace wsl::mcp_server {

mcp::json handle_list_namespaces(const mcp::json& params);
mcp::json handle_describe_namespace(const mcp::json& params);

} // namespace wsl::mcp_server
