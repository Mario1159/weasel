#pragma once

#include <string>
#include <optional>

namespace wsl::cli {

class editor_client {
public:
    editor_client() = default;
    ~editor_client();

    // Connect to editor server for the given project
    // Returns true if connection successful and project matches
    bool connect(const std::string& project_path);

    // Disconnect from server
    void disconnect();

    // Send command and receive response
    // Returns the response from the editor, or nullopt if connection failed
    std::optional<std::string> execute_command(const std::string& command);

    // Check if connected
    bool is_connected() const { return m_connected; }

private:
    int m_socket_fd = -1;
    bool m_connected = false;
    std::string m_socket_path;

    std::string read_response();
    bool write_line(const std::string& line);
};

} // namespace wsl::cli
