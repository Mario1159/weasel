#include "ui_manager.hpp"
#include "wsl/log/log.hpp"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/factory.hpp>

#include "comp/component_meta.hpp"
#include "gfx/render_context.hpp"
#include "gfx/render_window.hpp"
#include "rsc/resource_manager.hpp"
#include "runtime_context.hpp"
#include <filesystem>
#include <imgui.h>
#include <string>

namespace wsl
{

namespace comp
{
namespace singl
{

ui_manager::ui_manager (gfx::render_context &ctx,
                        wsl::gfx::render_window &window,
                        wsl::rsc::resource_manager *res_mgr)
{
  if (ctx.gpu_device == nullptr || window.handler() == nullptr) {
    wsl::log::editor ()->trace ("Headless mode, skipping RmlUi initialization");
    return;
  }

  m_render_interface = std::make_unique<RenderInterface_SDL_GPU> (
      ctx.gpu_device, window.handler());
  wsl::log::editor ()->trace ("Using GPU device {}", (void *)ctx.gpu_device);
  Rml::SetRenderInterface (m_render_interface.get ());
  Rml::SetSystemInterface (&m_system_interface);
  Rml::Initialise ();

  int width = 0;
  int height = 0;
  window.get_size (width, height);
  m_context = Rml::CreateContext ("UI Context", Rml::Vector2i{ width, height });

  if (res_mgr != nullptr) {
    for (const auto &font : res_mgr->list_fonts ()) {
      std::string const resolved = res_mgr->resolve_path (font.path);
      load_font (resolved);
    }
  }
}

ui_manager::~ui_manager ()
{
  if (m_context == nullptr)
    return;

  if (m_active_document_instance != nullptr) {
    m_active_document_instance->Close ();
    m_active_document_instance = nullptr;
  }

  clear_scene_bindings ();
  Rml::Shutdown ();
}

void
ui_manager::register_meta ()
{
  using namespace entt::literals;
  auto &&factory_ui
      = entt::meta_factory<comp::singl::ui_manager> ()
            .type (entt::type_hash<comp::singl::ui_manager>::value ())
            .custom<comp::meta_info> (comp::meta_info{
                "UI Manager",
                "Runtime-owned RmlUi context and renderer used "
                "by the active scene.",
                "" });
  (factory_ui
       .func<&comp::singl::ui_manager::custom_inspect>)("custom_inspect"_hs);
}

bool
ui_manager::custom_inspect (const char *label,
                            comp::singl::runtime_context *runtime)
{
  (void)label;
  if (runtime == nullptr) {
    return false;
  }

  auto &res_mgr = runtime->resource_manager ();
  bool changed = false;

  const char *preview = "None";
  if (m_active_document_id.value != entt::null) {
    if (auto info = res_mgr.info (m_active_document_id)) {
      preview = info->name.c_str ();
    }
  }

  if (ImGui::BeginCombo ("Active Document", preview)) {
    if (ImGui::Selectable ("None",
                           m_active_document_id.value == entt::null)) {
      if (m_active_document_id.value != entt::null) {
        m_active_document_id.value = entt::null;
        m_needs_reload = true;
        changed = true;
      }
    }

    for (const auto &info : res_mgr.list_ui_layouts ()) {
      std::filesystem::path const p (info.path);
      if (p.extension () != ".rml") {
        continue;
      }

      const bool selected = (info.id == m_active_document_id.value);
      if (ImGui::Selectable (info.name.c_str (), selected)) {
        m_active_document_id.value = info.id;
        m_needs_reload = true;
        changed = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }

  if (m_active_document_id.value != entt::null) {
    if (ImGui::Button ("Reload Document")) {
      m_needs_reload = true;
      changed = true;
    }
  }

  return changed;
}

void
ui_manager::clear_scene_bindings ()
{
  if (m_context != nullptr) {
    for (const std::string &name : m_active_model_names) {
      m_context->RemoveDataModel (name);
    }
  }

  m_active_model_names.clear ();
  m_prepared_registry = nullptr;
  ++m_binding_generation;
}

void
ui_manager::prepare_scene (entt::registry &registry)
{
  if (m_prepared_registry == &registry) {
    return;
  }

  if (m_active_document_instance != nullptr) {
    m_active_document_instance->Close ();
    m_active_document_instance = nullptr;
  }

  m_loaded_document_id.value = entt::null;
  clear_scene_bindings ();
  m_prepared_registry = &registry;
}

void
ui_manager::load_font (const std::string &path)
{
  if (m_loaded_fonts.contains (path)) {
    return;
  }

  if (Rml::LoadFontFace (path)) {
    m_loaded_fonts.insert (path);
  } else {
    wsl::log::editor ()->error ("Failed to load font face from {}", path);
  }
}

Rml::DataModelConstructor
ui_manager::ensure_data_model (entt::registry &registry,
                               const std::string &name)
{
  if ((m_context == nullptr) || name.empty ()) {
    return {};
  }

  prepare_scene (registry);

  Rml::DataModelConstructor constructor;
  const auto data_models = m_context->GetDataModels ();
  if (const auto it = data_models.find (name); it != data_models.end ()) {
    constructor = it->second;
  } else {
    constructor = m_context->CreateDataModel (name);
  }

  if (constructor) {
    m_active_model_names.insert (name);
  }

  return constructor;
}

} // namespace singl
} // namespace comp

} // namespace wsl
