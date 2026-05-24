#pragma once

#include "mcp_stdio_server.hpp"

#include <mcp_server.h>
#include <memory>
#include <string>

namespace wsl::mcp_server {

/*!
 * \brief Encapsulates the MCP server that exposes weasel-cli documentation.
 */
class mcp_server_app {
public:
  /*!\brief Constructs the server with the given bind address and port. */
  explicit mcp_server_app (const std::string &host, int port);

  /*!\brief Enables stdio transport instead of HTTP. */
  void set_stdio_mode (bool enable);

  /*!\brief Starts the server and blocks until stopped. */
  void run ();

private:
  void register_tools ();

  bool stdio_mode_ = false;
  mcp::server m_server;
  mcp_stdio_server m_stdio_server;
};

} // namespace wsl::mcp_server
