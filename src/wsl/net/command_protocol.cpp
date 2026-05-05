#include "command_protocol.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>

namespace wsl::net {

// Simple hash function for project path
std::string command_protocol::hash_project_path(const std::string& path) {
    std::hash<std::string> hasher;
    size_t hash = hasher(path);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

std::string command_protocol::socket_path(const std::string& project_path) {
    std::string hash = hash_project_path(project_path);
    return std::string(SOCKET_DIR) + "/" + SOCKET_PREFIX + hash + SOCKET_SUFFIX;
}

} // namespace wsl::net
