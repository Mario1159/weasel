#include "editor_server.hpp"
#include "editor_app.hpp"
#include "wsl/net/command_protocol.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <spdlog/spdlog.h>
#include <iostream>
#include <sstream>
#include <thread>

namespace editor {

class editor_server::impl {
public:
    impl() = default;
    ~impl() { cleanup(); }

    bool start(const std::string& project_path) {
        m_project_path = project_path;
        m_socket_path = wsl::net::command_protocol::socket_path(project_path);

        // Remove existing socket file if present
        unlink(m_socket_path.c_str());

        m_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_server_fd < 0) {
            spdlog::error("editor_server: failed to create socket: {}", strerror(errno));
            return false;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            spdlog::error("editor_server: failed to bind socket {}: {}", m_socket_path, strerror(errno));
            cleanup();
            return false;
        }

        if (listen(m_server_fd, 5) < 0) {
            spdlog::error("editor_server: failed to listen: {}", strerror(errno));
            cleanup();
            return false;
        }

        spdlog::info("editor_server: started on {}", m_socket_path);
        return true;
    }

    void stop() {
        cleanup();
    }

    bool is_running() const {
        return m_server_fd >= 0;
    }

    void poll(editor_app* editor_app) {
        if (m_server_fd < 0) {
            return;
        }

        // Non-blocking accept
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_server_fd, &fds);

        timeval timeout{};
        timeout.tv_usec = 100; // 100 microsecond timeout for non-blocking

        if (select(m_server_fd + 1, &fds, nullptr, nullptr, &timeout) > 0) {
            int client_fd = accept(m_server_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                std::thread(&impl::handle_client, this, client_fd, editor_app).detach();
            }
        }
    }

private:
    void handle_client(int client_fd, editor_app* editor_app) {
        spdlog::info("editor_server: handle_client() called, client_fd={}", client_fd);
        // Read project path from client (handshake)
        std::string client_project = read_line(client_fd);
        spdlog::info("editor_server: received project: {}", client_project);
        
        // Validate project match
        if (client_project != m_project_path) {
            std::string response = wsl::net::command_protocol::HANDSHAKE_PROJECT_MISMATCH;
            write_all(client_fd, response + "\n");
            spdlog::warn("editor_server: project mismatch - client: {}, server: {}", client_project, m_project_path);
            close(client_fd);
            return;
        }

        // Send OK handshake
        write_all(client_fd, std::string(wsl::net::command_protocol::HANDSHAKE_OK) + "\n");

        // Process commands from client
        while (true) {
            std::string command = read_line(client_fd);
            if (command.empty()) break;

            spdlog::debug("editor_server: received command: {}", command);

            // Execute command via editor_app
            std::string output;
            if (editor_app) {
                output = editor_app->execute_command(command);
            } else {
                output = "ERROR: No editor app available\n";
            }

            // Send response with terminator
            write_all(client_fd, output);
            write_all(client_fd, wsl::net::command_protocol::RESPONSE_TERMINATOR);
            write_all(client_fd, "\n");
        }

        close(client_fd);
    }

    std::string read_line(int fd) {
        std::string result;
        char buffer[1];
        while (read(fd, buffer, 1) > 0) {
            if (buffer[0] == '\n') break;
            result += buffer[0];
        }
        return result;
    }

    bool write_all(int fd, const std::string& data) {
        size_t total = 0;
        while (total < data.size()) {
            ssize_t written = write(fd, data.c_str() + total, data.size() - total);
            if (written < 0) return false;
            total += written;
        }
        return true;
    }

    void cleanup() {
        if (m_server_fd >= 0) {
            close(m_server_fd);
            m_server_fd = -1;
        }
        if (!m_socket_path.empty()) {
            unlink(m_socket_path.c_str());
            m_socket_path.clear();
        }
    }

    int m_server_fd = -1;
    std::string m_project_path;
    std::string m_socket_path;
};

editor_server::editor_server() : m_impl(std::make_unique<impl>()) {}
editor_server::~editor_server() = default;

bool editor_server::start(const std::string& project_path) {
    return m_impl->start(project_path);
}

void editor_server::stop() {
    m_impl->stop();
}

bool editor_server::is_running() const {
    return m_impl->is_running();
}

void editor_server::poll() {
    m_impl->poll(m_editor_app);
}

} // namespace editor
