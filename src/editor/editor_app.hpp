#pragma once

#include "wsl/app.hpp"
#include "engine_ui.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "editor_server.hpp"

namespace editor
{

class editor_app : public wsl::app
{
public:
  editor_app (const std::string &name, int width, int height,
              const std::string &engine_res_path = ".");
  ~editor_app () override;

protected:
  void on_init () override;
  void on_event (SDL_Event &e) override;
  void on_update (double dt) override;
  void on_render () override;

public:
  wsl::comp::singl::runtime_context* get_runtime_context() const { return m_runtime_context.get(); }
  void set_project_path(const std::string& path) { 
    m_runtime_context->resource_manager.load_project(path); 
    // Server will be started in on_update once the project is fully loaded
  }

  // Execute a command in the editor's context (for editor_server)
  std::string execute_command(const std::string& command);

private:
  std::unique_ptr<wsl::comp::singl::editor_context> m_editor_context;
  std::unique_ptr<engine_ui> m_engine_ui_layer;
  std::unique_ptr<editor_server> m_server;
  std::string m_project_path;
};

} // namespace editor
