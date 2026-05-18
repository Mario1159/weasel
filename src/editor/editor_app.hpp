#pragma once

#include "wsl/editor_app.hpp"
#include "engine_ui.hpp"
#include "editor_server.hpp"

namespace editor
{

class editor_app : public wsl::editor_app
{
public:
  editor_app (const std::string &name, int width, int height,
              const std::string &engine_res_path = ".");
  ~editor_app () override;

  wsl::comp::singl::runtime_context *get_runtime_context () const
  {
    return m_runtime_context.get ();
  }
  void set_project_path (const std::string &path);
  std::string execute_command (const std::string &command);

protected:
  std::unique_ptr<wsl::gfx::imgui_renderer_interface>
  create_imgui_renderer (wsl::gfx::render_window &window,
                       wsl::gfx::render_context *ctx) override;

  std::unique_ptr<wsl::debug::debug_renderer_interface>
  create_debug_renderer (wsl::gfx::render_window &window,
                         wsl::gfx::render_context *ctx) override;

  std::unique_ptr<wsl::editor::editor_ui_layer_interface>
  create_ui_layer (wsl::comp::singl::runtime_context *runtime_ctx,
                   wsl::comp::singl::editor_context *editor_ctx) override;

  void on_init () override;
  void on_update (double dt) override;

private:
  std::unique_ptr<editor_server> m_server;
  std::string m_project_path;
};

} // namespace editor
