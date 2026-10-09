#pragma once

#include "wsl/net/socket_compat.hpp"
#include <string>
#include <optional>

namespace wsl::cli
{

class editor_client
{
public:
  editor_client () = default;
  ~editor_client ();

  // Connect to an editor server. When `project_path` is provided, only an
  // editor with that exact project loaded accepts the connection. When it
  // is nullopt, running editor instances are discovered automatically and
  // the first one with a project loaded wins; its project path is then
  // available through connected_project().
  // Returns true if connection successful and project matches.
  bool connect (const std::optional<std::string> &project_path);

  // Disconnect from server
  void disconnect ();

  // Send command and receive response
  // Returns the response from the editor, or nullopt if connection failed
  std::optional<std::string> execute_command (const std::string &command);

  // Check if connected
  bool
  is_connected () const
  {
    return m_connected;
  }

  // Project path of the connected editor (valid while connected)
  const std::string &
  connected_project () const
  {
    return m_project_path;
  }

private:
  wsl::net::socket_handle m_socket_fd = wsl::net::invalid_socket;
  bool m_connected = false;
  std::string m_socket_path;
  std::string m_project_path;

  wsl::net::socket_handle open_socket (const std::string &socket_path);
  bool handshake (wsl::net::socket_handle fd,
                  const std::string &normalized_project_path);

  std::string read_line (wsl::net::socket_handle fd);
  std::string read_response ();
  bool write_line (wsl::net::socket_handle fd, const std::string &line);
};

} // namespace wsl::cli
