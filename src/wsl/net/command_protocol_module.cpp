module;
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

module wsl.core;

namespace wsl::net
{

std::string
command_protocol::hash_project_path (const std::string &path)
{
  std::hash<std::string> hasher;
  size_t hash = hasher (path);
  std::ostringstream oss;
  oss << std::hex << std::setfill ('0') << std::setw (16) << hash;
  return oss.str ();
}

std::string
command_protocol::socket_path (const std::string &project_path)
{
  std::string hash = hash_project_path (project_path);
  return std::string (SOCKET_DIR) + "/" + SOCKET_PREFIX + hash + SOCKET_SUFFIX;
}

std::vector<std::string>
command_protocol::discover_editor_sockets ()
{
  namespace fs = std::filesystem;

  const size_t prefix_len = std::strlen (SOCKET_PREFIX);
  const size_t suffix_len = std::strlen (SOCKET_SUFFIX);

  std::vector<std::string> sockets;
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator (SOCKET_DIR, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_socket (ec) || ec) {
      continue;
    }

    const std::string name = entry.path ().filename ().string ();
    if (name.size () <= prefix_len + suffix_len) {
      continue;
    }
    if (name.compare (0, prefix_len, SOCKET_PREFIX) != 0) {
      continue;
    }
    if (name.compare (name.size () - suffix_len, suffix_len, SOCKET_SUFFIX)
        != 0) {
      continue;
    }

    sockets.push_back ((fs::path (SOCKET_DIR) / name).string ());
  }

  std::sort (sockets.begin (), sockets.end ());
  return sockets;
}

} // namespace wsl::net
