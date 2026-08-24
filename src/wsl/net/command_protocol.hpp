#pragma once

#include <string>
#include <vector>

namespace wsl::net
{

// Protocol constants for editor-cli communication
struct command_protocol
{
  // Socket path template: /tmp/weasel-editor-{hash}.sock
  static constexpr const char *SOCKET_DIR = "/tmp";
  static constexpr const char *SOCKET_PREFIX = "weasel-editor-";
  static constexpr const char *SOCKET_SUFFIX = ".sock";

  // Handshake messages
  static constexpr const char *HANDSHAKE_OK = "OK";
  static constexpr const char *HANDSHAKE_PROJECT_MISMATCH
      = "ERROR: Project mismatch";
  static constexpr const char *HANDSHAKE_NO_PROJECT
      = "ERROR: No project loaded";

  // Query sent by clients that want to discover the project a running
  // editor instance has open; answered with "PROJECT <path>".
  static constexpr const char *HANDSHAKE_QUERY = "?";
  static constexpr const char *HANDSHAKE_PROJECT_PREFIX = "PROJECT ";

  // Response terminator (sent after command output)
  static constexpr const char *RESPONSE_TERMINATOR = "<<<END>>>";

  // Generate socket path from project path (hashed for uniqueness)
  static std::string socket_path (const std::string &project_path);

  // Hash function for project path
  static std::string hash_project_path (const std::string &path);

  // Lists sockets of running editor instances (SOCKET_DIR entries matching
  // the editor socket naming pattern), sorted by name for deterministic
  // ordering. Stale files are tolerated; connection attempts fail naturally.
  static std::vector<std::string> discover_editor_sockets ();
};

} // namespace wsl::net
