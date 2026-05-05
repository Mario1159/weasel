#include "editor_app.hpp"
#include "editor_server.hpp"
#include "app.hpp"
#include "editor/engine_ui.hpp"
#include "sys/core_systems.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/project.hpp"
#include "wsl/rsc/project_loader.hpp"
#include "cli/repl_handler.hpp"
#include "cli/command_executor.hpp"
#include <SDL3/SDL_events.h>
#include <entt/entity/fwd.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <sstream>

namespace editor
{

editor_app::editor_app (const std::string &name, int width, int height,
                         const std::string &engine_res_path)
    : ::wsl::app (name, width, height, engine_res_path)
{
  m_editor_context = std::make_unique<::wsl::comp::singl::editor_context> (*m_runtime_context);
  m_runtime_context->set_editor_ctx (m_editor_context.get());

  // Register editor context for the inspector
  ::wsl::comp::singl::editor_context::register_meta ();
  m_runtime_context->singleton_registry
      .register_bound_singleton_component<::wsl::comp::singl::editor_context> (
          { "Editor Context", true });

  m_engine_ui_layer
      = std::make_unique<engine_ui> (m_runtime_context.get(), m_editor_context.get());

  // Initialize editor server
  m_server = std::make_unique<editor_server>();
  m_server->set_editor_app(this);
}

editor_app::~editor_app ()
{
  m_server->stop();
}

std::string editor_app::execute_command(const std::string& command) {
    // Create a command_executor with the editor's runtime_context
    wsl::cli::command_executor executor(*m_runtime_context);
    return executor.execute(command);
}

void
editor_app::on_init ()
{
  m_engine_ui_layer->initialize ();

  // Wire the interactive console directly into the editor's runtime context
  m_engine_ui_layer->set_console_command_handler (
      [this] (const std::string &cmd) { return this->execute_command (cmd); });
}

void
editor_app::on_event (SDL_Event &e)
{
  m_engine_ui_layer->handle_event (e);
}

void
editor_app::on_update (double dt)
{
  m_editor_context->tick_editor_camera_anim ((float)dt);

  if (m_editor_context->pending_project_load)
    {
      m_runtime_context->resource_manager.load_project (
          *m_editor_context->pending_project_load);
      m_editor_context->pending_project_load.reset ();
    }

  m_runtime_context->resource_manager.update_async_uploads ();
  m_editor_context->editor_resources.update_async_uploads ();

  // Start or restart the editor server when a project becomes loaded
  if (auto current_proj = m_runtime_context->resource_manager.current_project ()) {
    std::string proj_root = std::filesystem::weakly_canonical (current_proj->root_path).string ();
    if (m_project_path != proj_root) {
      m_project_path = proj_root;
      if (m_server) {
        if (m_server->is_running ()) {
          m_server->stop ();
        }
        spdlog::info ("editor_app: starting server for project: {}", m_project_path);
        if (!m_server->start (m_project_path)) {
          spdlog::error ("editor_app: failed to start editor server");
        }
      }
    }
  }

  // Poll editor server for incoming commands
  m_server->poll();
}

void
editor_app::on_render ()
{
  ::wsl::sys::core_systems::render_callbacks callbacks;
  callbacks.build_draw_data = [this] (entt::registry &reg) {
    m_engine_ui_layer->build_draw_data (reg);
  };
  callbacks.prepare_gpu_rsc = [this] (entt::registry &) {
    m_engine_ui_layer->prepare_gpu_resources ();
  };
  callbacks.record_ui_draw_cmd = [this] (entt::registry &) {
    m_engine_ui_layer->record_draw_commands ();
  };

  m_runtime_context->core_systems->render (m_runtime_context->window, callbacks);
}

} // namespace editor
