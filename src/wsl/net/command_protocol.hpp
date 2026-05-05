#pragma once

#include <string>
#include <functional>

namespace wsl::net {

// Protocol constants for editor-cli communication
struct command_protocol {
    // Socket path template: /tmp/weasel-editor-{hash}.sock
    static constexpr const char* SOCKET_DIR = "/tmp";
    static constexpr const char* SOCKET_PREFIX = "weasel-editor-";
    static constexpr const char* SOCKET_SUFFIX = ".sock";

    // Handshake messages
    static constexpr const char* HANDSHAKE_OK = "OK";
    static constexpr const char* HANDSHAKE_PROJECT_MISMATCH = "ERROR: Project mismatch";
    static constexpr const char* HANDSHAKE_NO_PROJECT = "ERROR: No project loaded";

    // Response terminator (sent after command output)
    static constexpr const char* RESPONSE_TERMINATOR = "<<<END>>>";

    // Generate socket path from project path (hashed for uniqueness)
    static std::string socket_path(const std::string& project_path);

    // Hash function for project path
    static std::string hash_project_path(const std::string& path);
};

} // namespace wsl::net
