#include "editor_app.hpp"

#include "editor_server.hpp"
#include "cli/command_executor.hpp"
#include "renderer_imgui.hpp"
#include "physics_debug_renderer.hpp"
#include "engine_ui.hpp"
#include "wsl/log/log.hpp"
#include <filesystem>
namespace editor
{

editor_app::editor_app (const std::string &name, int width, int height,
                        const std::string &engine_res_path)
    : wsl::editor_app (name, width, height, engine_res_path)
{
  init_editor_subsystems ();

  m_server = std::make_unique<editor_server> ();
  m_server->set_editor_app (this);
}

editor_app::~editor_app () { m_server->stop (); }

std::string
editor_app::execute_command (const std::string &command)
{
  wsl::cli::command_executor executor (*m_runtime_context);
  return executor.execute (command);
}

void
editor_app::set_project_path (const std::string &path)
{
  m_runtime_context->resource_manager.load_project (path);
}

std::unique_ptr<wsl::gfx::imgui_renderer_interface>
editor_app::create_imgui_renderer (wsl::gfx::render_window &window,
                                   wsl::gfx::render_context *ctx)
{
  return std::make_unique<editor::renderer_imgui> (window, ctx);
}

std::unique_ptr<wsl::debug::debug_renderer_interface>
editor_app::create_debug_renderer (wsl::gfx::render_window &window,
                                   wsl::gfx::render_context *ctx)
{
  return editor::make_physics_debug_renderer (window, ctx);
}

std::unique_ptr<wsl::editor::editor_ui_layer_interface>
editor_app::create_ui_layer (wsl::comp::singl::runtime_context *runtime_ctx,
                             wsl::comp::singl::editor_context *editor_ctx)
{
  return std::make_unique<editor::engine_ui> (runtime_ctx, editor_ctx);
}

void
editor_app::on_init ()
{
  wsl::editor_app::on_init ();

  // Wire the interactive console directly into the editor's runtime context
  ui_layer ()->set_console_command_handler (
      [this] (const std::string &cmd) { return this->execute_command (cmd); });
}

void
editor_app::on_update (double dt)
{
  wsl::editor_app::on_update (dt);

  // Start or restart the editor server when a project becomes loaded
  if (auto current_proj
      = m_runtime_context->resource_manager.current_project ()) {
    std::string proj_root
        = std::filesystem::weakly_canonical (current_proj->root_path).string ();
    if (m_project_path != proj_root) {
      m_project_path = proj_root;
      if (m_server) {
        if (m_server->is_running ()) {
          m_server->stop ();
        }
        wsl::log::editor ()->info ("Starting server for project: {}",
                                   m_project_path);
        if (!m_server->start (m_project_path)) {
          wsl::log::editor ()->error ("Failed to start editor server");
        }
      }
    }
  }

  // Poll editor server for incoming commands
  m_server->poll ();
}

} // namespace editor
