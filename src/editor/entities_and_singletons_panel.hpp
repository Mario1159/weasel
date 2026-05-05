#pragma once

#include "ecs_inspector_utils.hpp"
#include <entt/entt.hpp>
#include <string>

namespace wsl::comp::singl { class runtime_context; class editor_context; }
namespace wsl::rsc { class scene; }

namespace editor
{

class entities_and_singletons_panel
{
public:
  entities_and_singletons_panel (wsl::comp::singl::runtime_context *runtime_ctx,
                     wsl::comp::singl::editor_context *editor_ctx,
                     ecs_selection &selection);

  void draw ();

private:
  void draw_entity_list ();
  void draw_entity_flat ();
  void draw_entity_tree ();
  void draw_entity_subtree (entt::entity entity, int depth);
  void draw_entity_row (entt::entity entity, int depth);
  void draw_core_singleton_list ();
  void draw_scene_singleton_list ();
  void draw_add_singleton_ui ();
  
  entt::entity duplicate_entity (entt::entity original, entt::entity parent = entt::null);
  void delete_entity (entt::entity entity);
  void make_prefab (entt::entity entity);


  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  ecs_selection &m_selection;

  entt::entity m_rename_entity = entt::null;
  char m_rename_buffer[128]{};
  bool m_request_rename_popup = false;
  bool m_show_hierarchy = true;

  float m_entities_height = -1.0F;
  float m_core_singletons_height = -1.0F;
};

} // namespace editor
