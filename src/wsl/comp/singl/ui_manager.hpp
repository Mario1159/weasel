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
    archive (cereal::make_nvp ("active_document_id", active_document_id.value));
  }

  editor::ui_system_interface system_interface;
  std::unique_ptr<RenderInterface_SDL_GPU> render_interface;
  Rml::Context *context = nullptr;
  Rml::ElementDocument *active_document_instance = nullptr;

  rsc::ui_layout_id active_document_id{ entt::null };
  rsc::ui_layout_id loaded_document_id{ entt::null };
  bool needs_reload = false;

private:
  entt::registry *m_prepared_registry = nullptr;
  std::unordered_set<std::string> m_loaded_fonts;
  std::unordered_set<std::string> m_active_model_names;
  std::size_t m_binding_generation = 0;
};

} // namespace singl

} // namespace comp

} // namespace wsl
