#include "editor_client.hpp"
#include "wsl/log/log.hpp"
#include "wsl/net/command_protocol.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>

namespace wsl::cli
{

editor_client::~editor_client () { disconnect (); }

bool
editor_client::connect (const std::string &project_path)
{
  // Normalize the path for consistent hashing
  std::string normalized_path
      = std::filesystem::path (project_path).lexically_normal ().string ();
  // Normalize the path for consistent hashing
  m_socket_path = wsl::net::command_protocol::socket_path (normalized_path);

  m_socket_fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (m_socket_fd < 0) {
    wsl::log::net ()->error ("Failed to create client socket: {}",
                             strerror (errno));
    return false;
  }
  wsl::log::net ()->info ("Client socket created, fd={}", m_socket_fd);

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, m_socket_path.c_str (), sizeof (addr.sun_path) - 1);

  if (::connect (m_socket_fd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
    wsl::log::net ()->debug ("Failed to connect to {}: {}", m_socket_path,
                             strerror (errno));
    disconnect ();
    return false;
  }
  wsl::log::net ()->info ("Connected to editor socket");

  // Send project path for handshake (use normalized path)
  if (!write_line (normalized_path)) {
    disconnect ();
    return false;
  }

  // Read handshake response
  char buffer[256];
  ssize_t bytes_read = read (m_socket_fd, buffer, sizeof (buffer) - 1);
  if (bytes_read <= 0) {
    disconnect ();
    return false;
  }
  buffer[bytes_read] = '\0';
  std::string response (buffer);

  if (response.find (wsl::net::command_protocol::HANDSHAKE_OK) == 0) {
    wsl::log::net ()->info ("Connected to editor server for project: {}",
                            normalized_path);
    m_connected = true;
    return true;
  } else {
    wsl::log::net ()->warn ("Handshake failed: {}", response);
    disconnect ();
    return false;
  }
}

void
editor_client::disconnect ()
{
  if (m_socket_fd >= 0) {
    close (m_socket_fd);
    m_socket_fd = -1;
  }
  m_connected = false;
}

std::optional<std::string>
editor_client::execute_command (const std::string &command)
{
  if (!m_connected || m_socket_fd < 0) {
    return std::nullopt;
  }

  if (!write_line (command)) {
    disconnect ();
    return std::nullopt;
  }

  return read_response ();
}

std::string
editor_client::read_response ()
{
  std::string result;
  char buffer[1024];
  std::string terminator = wsl::net::command_protocol::RESPONSE_TERMINATOR;

  while (true) {
    ssize_t bytes_read = read (m_socket_fd, buffer, sizeof (buffer) - 1);
    if (bytes_read <= 0)
      break;

    buffer[bytes_read] = '\0';
    result += buffer;

    // Check for terminator
    if (result.find (terminator) != std::string::npos) {
      // Remove terminator from result
      size_t pos = result.find (terminator);
      result = result.substr (0, pos);
      break;
    }
  }

  return result;
}

bool
editor_client::write_line (const std::string &line)
{
  std::string data = line + "\n";
  size_t total = 0;
  while (total < data.size ()) {
    ssize_t written
        = write (m_socket_fd, data.c_str () + total, data.size () - total);
    if (written < 0)
      return false;
    total += written;
  }
  return true;
}

} // namespace wsl::cli
