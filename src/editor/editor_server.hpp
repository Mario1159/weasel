#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace editor {

class editor_app;

class editor_server {
public:
    editor_server();
    ~editor_server();

    // Start server with the given project path (used for client validation)
    bool start(const std::string& project_path);

    // Stop the server and clean up
    void stop();

    // Poll for incoming client connections and commands (non-blocking)
    // Should be called regularly from the editor's main loop
    bool is_running() const;
    void poll();

    // Set the editor app reference (for executing commands)
    void set_editor_app(editor_app* app) { m_editor_app = app; }

private:
    class impl;
    std::unique_ptr<impl> m_impl;
    editor_app* m_editor_app = nullptr;
    std::string m_project_path;
    std::atomic<bool> m_running{false};
};

} // namespace editor
