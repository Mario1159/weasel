#pragma once

#include "editor/ui_system_interface.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_SDL_GPU.h>

#include <cereal/cereal.hpp>
#include <entt/entt.hpp>

#include <string>
#include <unordered_set>

#include "wsl/comp/component_meta.hpp"
#include "wsl/gfx/render_context.hpp"
#include "wsl/gfx/render_window.hpp"
#include "wsl/rsc/resource_manager.hpp"

namespace wsl
{

namespace comp
{

namespace singl
{

class runtime_context;

class ui_manager : public comp::singleton_component
{
public:
  ui_manager (gfx::render_context &ctx, wsl::gfx::render_window &window,
              wsl::rsc::resource_manager *res_mgr);
  ~ui_manager ();

  static void register_meta ();
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);

  void prepare_scene (entt::registry &registry);
  Rml::DataModelConstructor ensure_data_model (entt::registry &registry,
                                               const std::string &name);
  void clear_scene_bindings ();
  void load_font (const std::string &path);
  std::size_t
  binding_generation () const
  {
    return m_binding_generation;
  }
  entt::registry *
  prepared_registry () const
  {
    return m_prepared_registry;
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    rsc::ui_layout_id def_id{};
    serialize_field_if_diff (archive, "active_document_id",
                             m_active_document_id.value, def_id.value);
  }

  // -- getters / setters
  // -------------------------------------------------------

  const editor::ui_system_interface &
  system_interface () const
  {
    return m_system_interface;
  }
  editor::ui_system_interface &
  system_interface ()
  {
    return m_system_interface;
  }

  RenderInterface_SDL_GPU *
  render_interface () const
  {
    return m_render_interface.get ();
  }
  std::unique_ptr<RenderInterface_SDL_GPU> &
  render_interface ()
  {
    return m_render_interface;
  }

  Rml::Context *
  context () const
  {
    return m_context;
  }
  Rml::Context *&
  context ()
  {
    return m_context;
  }

  Rml::ElementDocument *
  active_document_instance () const
  {
    return m_active_document_instance;
  }
  Rml::ElementDocument *&
  active_document_instance ()
  {
    return m_active_document_instance;
  }

  const rsc::ui_layout_id &
  active_document_id () const
  {
    return m_active_document_id;
  }
  rsc::ui_layout_id &
  active_document_id ()
  {
    return m_active_document_id;
  }

  const rsc::ui_layout_id &
  loaded_document_id () const
  {
    return m_loaded_document_id;
  }
  rsc::ui_layout_id &
  loaded_document_id ()
  {
    return m_loaded_document_id;
  }

  bool
  needs_reload () const
  {
    return m_needs_reload;
  }
  bool &
  needs_reload ()
  {
    return m_needs_reload;
  }

private:
  editor::ui_system_interface m_system_interface;
  std::unique_ptr<RenderInterface_SDL_GPU> m_render_interface;
  Rml::Context *m_context = nullptr;
  Rml::ElementDocument *m_active_document_instance = nullptr;
  rsc::ui_layout_id m_active_document_id{ entt::null };
  rsc::ui_layout_id m_loaded_document_id{ entt::null };
  bool m_needs_reload = false;
  entt::registry *m_prepared_registry = nullptr;
  std::unordered_set<std::string> m_loaded_fonts;
  std::unordered_set<std::string> m_active_model_names;
  std::size_t m_binding_generation = 0;
};

} // namespace singl

} // namespace comp

} // namespace wsl
