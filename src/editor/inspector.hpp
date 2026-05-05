#pragma once

#include "wsl/reg/singleton_registry.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "ecs_inspector_utils.hpp"

namespace wsl::sys { class ecs_system; }
namespace wsl::comp::singl { class runtime_context; class editor_context; }

namespace editor
{

class inspector
{
public:
  inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                             wsl::comp::singl::editor_context *editor_ctx,
                             ecs_selection &selected);

  void draw ();

private:
  void draw_system_inspector (wsl::sys::ecs_system *system);
  void draw_singleton_inspector (entt::id_type type);
  void draw_entity_inspector (entt::entity entity);
  void draw_hierarchy_component (entt::entity entity);
  void set_parent (entt::entity child, entt::entity new_parent);
  
  bool draw_meta_class (entt::meta_any &object,
                        const glm::vec3 &scale = { 1.0F, 1.0F, 1.0F },
                        entt::meta_any *prefab_object = nullptr);
  bool draw_meta_object (const char *label, entt::meta_any &object,
                         entt::meta_any *prefab_object = nullptr);
  static bool draw_meta_enum (const char *label, entt::meta_any &object);
  bool draw_meta_sequence (const char *label, entt::meta_any &object);
  bool draw_meta_value (const char *label, entt::meta_any &object,
                        entt::meta_any *prefab_object = nullptr);
  
  static bool is_hidden (const entt::meta_data& data) ;
  void draw_add_component_ui (entt::entity entity);
  void draw_singleton_header (const wsl::reg::singleton_registry::descriptor &desc, 
                              const entt::meta_type& meta,
                              bool can_remove, bool *remove_requested);


  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  entt::registry m_no_scene_registry;
  ecs_selection &m_selection;

  bool m_is_prefab_instance = false;
  entt::registry *m_prefab_registry = nullptr;
  entt::entity m_prefab_entity = entt::null;
};

} // namespace editor
