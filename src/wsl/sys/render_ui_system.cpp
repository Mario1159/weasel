#include "render_ui_system.hpp"
#include "wsl/log/log.hpp"

#include <RmlUi/Core/Math.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL_events.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <string>

#include "wsl/reg/sig/signal_hub.hpp"
#include "wsl/comp/singl/ui_manager.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/engine_resources.hpp"
#include <imgui.h>

namespace wsl
{

namespace sys
{

void
render_ui_system::register_signals (reg::sig::signal_hub &hub)
{
  (void)hub;
}

void
render_ui_system::register_event_handlers (reg::sig::signal_hub &hub)
{
  wsl::reg::sig::declare_handler<render_ui_system> (hub, "on_event");
}

void
render_ui_system::register_iterations (reg::sig::signal_hub &hub)
{
  (void)hub;
}

void
render_ui_system::on_init (entt::registry & /*unused*/)
{
}

void
render_ui_system::on_render_build_draw_data (entt::registry &registry)
{
  if (!registry.ctx ().template contains<comp::singl::runtime_context *> ()
      || !registry.ctx ().template contains<comp::singl::ui_manager *> ()) {
    return;
  }

  auto &ui = *registry.ctx ().template get<comp::singl::ui_manager *> ();

  ui.needs_reload = true;
}

void
render_ui_system::on_update (entt::registry &registry, double /*unused*/)
{
  if (!registry.ctx ().template contains<comp::singl::runtime_context *> ()
      || !registry.ctx ().template contains<comp::singl::ui_manager *> ()) {
    return;
  }

  auto &runtime_ctx
      = *registry.ctx ().template get<comp::singl::runtime_context *> ();
  auto &ui = *registry.ctx ().template get<comp::singl::ui_manager *> ();

  ui.prepare_scene (registry);

  const bool needs_reload
      = ui.needs_reload
        || ui.loaded_document_id.value != ui.active_document_id.value;
  if (needs_reload) {
    // Ensure all registered fonts are loaded into RmlUi
    for (const auto &font : runtime_ctx.resource_manager.list_fonts ()) {
      std::string const resolved
          = runtime_ctx.resource_manager.resolve_path (font.path);
      ui.load_font (resolved);
    }

    // Load engine fonts from editor context if available
    if (runtime_ctx.editor_ctx != nullptr) {
      for (const auto &font :
           runtime_ctx.editor_ctx->editor_resources.list_fonts ()) {
        std::string const resolved
            = runtime_ctx.editor_ctx->editor_resources.resolve_path (font.path);
        ui.load_font (resolved);
      }
    }

    if (ui.active_document_instance != nullptr) {
      ui.active_document_instance->Close ();
      ui.active_document_instance = nullptr;
    }
    ui.loaded_document_id.value = entt::null;

    if (ui.active_document_id.value != entt::null) {
      if (auto info
          = runtime_ctx.resource_manager.info (ui.active_document_id)) {
        std::string const resolved
            = runtime_ctx.resource_manager.resolve_path (info->path);
        ui.active_document_instance = ui.context->LoadDocument (resolved);
        if (ui.active_document_instance != nullptr) {
          ui.active_document_instance->Show ();
          ui.loaded_document_id = ui.active_document_id;
        } else {
          wsl::log::sys ()->error ("Failed to load RML document: {}", resolved);
        }
      }
    }
    ui.needs_reload = false;
  }

  if (ui.context != nullptr) {
    ui.context->Update ();
  }
}

void
render_ui_system::on_render_record_draw_cmd (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.template contains<comp::singl::runtime_context *> ()
      || !ctx.template contains<comp::singl::ui_manager *> ()) {
    return;
  }

  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();
  auto &ui = *ctx.get<comp::singl::ui_manager *> ();

  if (ui.context == nullptr) {
    return;
  }

  int width, height;
  if ((runtime_ctx.editor_ctx != nullptr)
      && !runtime_ctx.editor_ctx->game_fullscreen) {
    // In editor mode, RmlUi should match the present_tex size
    width = (int)runtime_ctx.window.present_tex.width;
    height = (int)runtime_ctx.window.present_tex.height;
  } else {
    runtime_ctx.window.get_size (width, height);
  }

  // Update context size if it changed
  Rml::Vector2i const current_size = ui.context->GetDimensions ();
  if (current_size.x != width || current_size.y != height) {
    ui.context->SetDimensions (Rml::Vector2i{ width, height });
  }

  if (ui.render_interface) {
    ui.render_interface->BeginFrame (
        runtime_ctx.window.ctx->main_cmd,
        runtime_ctx.window.present_tex.texture_data, width, height);
  }

  ui.context->Render ();

  if (ui.render_interface) {
    ui.render_interface->EndFrame ();
  }
}

void
render_ui_system::on_event (entt::registry &registry, const SDL_Event &ev)
{

  auto &ctx = registry.ctx ();
  if (!ctx.template contains<comp::singl::runtime_context *> ()
      || !ctx.template contains<comp::singl::ui_manager *> ()) {
    return;
  }

  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();
  auto &ui = *ctx.get<comp::singl::ui_manager *> ();

  if ((runtime_ctx.editor_ctx != nullptr) && !runtime_ctx.is_running) {
    return;
  }

  if ((runtime_ctx.editor_ctx != nullptr)
      && !runtime_ctx.editor_ctx->game_fullscreen) {
    SDL_Event adjusted_ev = ev;
    bool process = true;

    auto &ed = *runtime_ctx.editor_ctx;
    float const img_x = ed.last_img_min.x;
    float const img_y = ed.last_img_min.y;
    float const img_w = ed.last_img_size.x;
    float const img_h = ed.last_img_size.y;

    if (img_w > 0 && img_h > 0) {
      float const scale_x = (float)runtime_ctx.window.present_tex.width / img_w;
      float const scale_y
          = (float)runtime_ctx.window.present_tex.height / img_h;

      ImVec2 const mouse_pos = ImGui::GetMousePos ();

      if (ev.type == SDL_EVENT_MOUSE_MOTION) {
        adjusted_ev.motion.x = (mouse_pos.x - img_x) * scale_x;
        adjusted_ev.motion.y = (mouse_pos.y - img_y) * scale_y;
      } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                 || ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        adjusted_ev.button.x = (mouse_pos.x - img_x) * scale_x;
        adjusted_ev.button.y = (mouse_pos.y - img_y) * scale_y;

        // Optional: ignore clicks outside the game view
        if (mouse_pos.x < img_x || mouse_pos.x > img_x + img_w
            || mouse_pos.y < img_y || mouse_pos.y > img_y + img_h) {
          process = false;
        }
      }
    }

    if (process) {
      RmlSDL::InputEventHandler (ui.context, runtime_ctx.window.handler,
                                 adjusted_ev);
    }
  } else {
    RmlSDL::InputEventHandler (ui.context, runtime_ctx.window.handler, ev);
  }
}

} // namespace sys

} // namespace wsl
