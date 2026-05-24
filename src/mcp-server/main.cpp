#include "mcp_server_app.hpp"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <iostream>

int
main (int argc, char **argv)
{
  CLI::App app{"Weasel Engine MCP Server"};

  std::string host = "localhost";
  int port = 8080;
  bool stdio_mode = false;
  app.add_option ("--host", host, "Host to bind to");
  app.add_option ("--port", port, "Port to listen on");
  app.add_flag ("--stdio", stdio_mode, "Use stdio transport instead of HTTP");

  try {
    app.parse (argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit (e);
  }

  wsl::mcp_server::mcp_server_app server (host, port);
  server.set_stdio_mode (stdio_mode);
  server.run ();

  return 0;
}
