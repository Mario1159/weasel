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

int
editor_client::open_socket (const std::string &socket_path)
{
  int fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    wsl::log::net ()->error ("Failed to create client socket: {}",
                             strerror (errno));
    return -1;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, socket_path.c_str (), sizeof (addr.sun_path) - 1);

  if (::connect (fd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
    wsl::log::net ()->debug ("Failed to connect to {}: {}", socket_path,
                             strerror (errno));
    close (fd);
    return -1;
  }

  return fd;
}

bool
editor_client::handshake (int fd, const std::string &normalized_project_path)
{
  // Send project path for handshake (use normalized path)
  if (!write_line (fd, normalized_project_path)) {
    return false;
  }

  const std::string response = read_line (fd);
  if (response.find (wsl::net::command_protocol::HANDSHAKE_OK) != 0) {
    wsl::log::net ()->warn ("Handshake failed: {}", response);
    return false;
  }

  return true;
}

bool
editor_client::connect (const std::optional<std::string> &project_path)
{
  disconnect ();

  if (project_path.has_value ()) {
    // Normalize the path for consistent hashing
    const std::string normalized_path
        = std::filesystem::path (*project_path).lexically_normal ().string ();
    m_socket_path = wsl::net::command_protocol::socket_path (normalized_path);

    m_socket_fd = open_socket (m_socket_path);
    if (m_socket_fd < 0) {
      return false;
    }

    if (!handshake (m_socket_fd, normalized_path)) {
      disconnect ();
      return false;
    }

    m_project_path = normalized_path;
    m_connected = true;
    wsl::log::net ()->info ("Connected to editor server for project: {}",
                            normalized_path);
    return true;
  }

  // Path-less attach: discover running editors and attach to the first one
  // that has a project loaded.
  for (const std::string &socket_path :
       wsl::net::command_protocol::discover_editor_sockets ()) {
    int fd = open_socket (socket_path);
    if (fd < 0) {
      continue;
    }

    // Ask which project this instance has loaded.
    if (!write_line (fd, wsl::net::command_protocol::HANDSHAKE_QUERY)) {
      close (fd);
      continue;
    }

    const std::string response = read_line (fd);
    const std::string prefix
        = wsl::net::command_protocol::HANDSHAKE_PROJECT_PREFIX;
    if (response.compare (0, prefix.size (), prefix) != 0) {
      wsl::log::net ()->debug ("Skipping {}: {}", socket_path, response);
      close (fd);
      continue;
    }

    const std::string discovered = response.substr (prefix.size ());
    if (!handshake (fd, discovered)) {
      close (fd);
      continue;
    }

    m_socket_fd = fd;
    m_socket_path = socket_path;
    m_project_path = discovered;
    m_connected = true;
    wsl::log::net ()->info ("Connected to editor server on {} for project: {}",
                            socket_path, discovered);
    return true;
  }

  wsl::log::net ()->debug ("No running editor instance found");
  return false;
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

  if (!write_line (m_socket_fd, command)) {
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

std::string
editor_client::read_line (int fd)
{
  std::string line;
  char ch;
  while (read (fd, &ch, 1) > 0) {
    if (ch == '\n') {
      break;
    }
    line += ch;
  }
  return line;
}

bool
editor_client::write_line (int fd, const std::string &line)
{
  std::string data = line + "\n";
  size_t total = 0;
  while (total < data.size ()) {
    ssize_t written = write (fd, data.c_str () + total, data.size () - total);
    if (written < 0)
      return false;
    total += written;
  }
  return true;
}

} // namespace wsl::cli
