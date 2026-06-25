#include "engine_ui.hpp"

#include "wsl/log/log.hpp"
#include "wsl/reg/sig/signal_hub.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/comp/singl/runtime_context.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <entt/entity/fwd.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <tracy/Tracy.hpp>

namespace editor
{

engine_ui::engine_ui (wsl::comp::singl::runtime_context *runtime_ctx,
                      wsl::comp::singl::editor_context *editor_ctx)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_root (runtime_ctx, editor_ctx)
{
}

void
engine_ui::register_signals (wsl::reg::sig::signal_hub &hub)
{
  hub.declare_signal<game_focus_toggled, engine_ui> ();
}

void
engine_ui::initialize ()
{
  m_root.attach_console_to_spdlog ();
}

void
engine_ui::set_console_command_handler (
    std::function<std::string (const std::string &)> handler)
{
  m_root.set_console_command_handler (std::move (handler));
}

void
engine_ui::handle_event (const SDL_Event &event)
{
  ImGui_ImplSDL3_ProcessEvent (&event);

  if ((m_runtime_ctx == nullptr)
      || (m_runtime_ctx->get_current_input_map () == nullptr)
      || event.type != SDL_EVENT_KEY_UP) {
    return;
  }

  auto &input_map = m_runtime_ctx->get_current_input_map ()->bindings;
  auto toggle_it = input_map.find ("toggle_game_focus");
  if (toggle_it != input_map.end ()
      && toggle_it->second.matches_event (event.key)) {
    m_game_focus = !m_game_focus;

    SDL_SetWindowRelativeMouseMode (m_runtime_ctx->window.handler,
                                    !m_game_focus);
    SDL_ShowCursor ();

    wsl::reg::sig::emit<game_focus_toggled> (
        m_runtime_ctx->signal_hub, game_focus_toggled{ m_game_focus });
    return;
  }

  // Editor toolbar shortcuts
  if (event.key.scancode == SDL_SCANCODE_F5) {
    if (!m_runtime_ctx->is_running) {
      m_runtime_ctx->set_running (true);
    }
    return;
  }

  if (event.key.scancode == SDL_SCANCODE_F7) {
    if (m_runtime_ctx->is_running) {
      m_runtime_ctx->set_running (false);
    }
    return;
  }

  if (event.key.scancode == SDL_SCANCODE_F8) {
    if (m_runtime_ctx->in_play_session) {
      m_runtime_ctx->stop ();
    }
    return;
  }
}

static int bd_count = 0;
void
engine_ui::build_draw_data (entt::registry &registry)
{
  if (bd_count < 5) {
    wsl::log::editor ()->trace ("Building draw data, iteration {}", bd_count);
  }
  ++bd_count;

  if ((m_runtime_ctx == nullptr) || (m_editor_ctx == nullptr)) {
    wsl::log::editor ()->warn ("Build draw data called with null context(s)");
    return;
  }

  if (m_editor_ctx->get_imgui_renderer () == nullptr) {
    wsl::log::editor ()->warn (
        "Build draw data called with null imgui_renderer");
    return;
  }

  m_editor_ctx->get_imgui_renderer ()->begin_frame ();
  m_root.draw (registry, m_runtime_ctx->window);

  TracyPlot ("FPS", ImGui::GetIO ().Framerate);
  m_editor_ctx->get_imgui_renderer ()->end_frame ();

  m_draw_data = ImGui::GetDrawData ();

  // TEMP DEBUG: dump the texture list so we can see if the second
  // frame adds a new texture (and what its dimensions are) before
  // PrepareDrawData creates it.
  if (m_draw_data != nullptr && m_draw_data->Textures != nullptr) {
    for (int ti = 0; ti < m_draw_data->Textures->Size; ++ti) {
      ImTextureData *tex = (*m_draw_data->Textures)[ti];
      wsl::log::editor ()->debug (
          "build_draw_data: texture[{}] id={} status={} size={}x{} "
          "format={} TexID={}",
          ti, (int)tex->UniqueID, (int)tex->Status, tex->Width, tex->Height,
          (int)tex->Format, (void *)(intptr_t)tex->GetTexID ());
    }
  }
}

void
engine_ui::prepare_gpu_resources ()
{
  if ((m_editor_ctx == nullptr) || (m_draw_data == nullptr)) {
    return;
  }

  m_editor_ctx->get_imgui_renderer ()->render_requested_previews ();
  m_editor_ctx->get_imgui_renderer ()->prepare (m_draw_data);
}

void
engine_ui::record_draw_commands ()
{
  if ((m_editor_ctx == nullptr) || (m_draw_data == nullptr)) {
    return;
  }

  m_editor_ctx->get_imgui_renderer ()->render (m_draw_data);
}

} // namespace editor
