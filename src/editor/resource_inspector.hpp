#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <SDL3_mixer/SDL_mixer.h>

namespace wsl::comp::singl
{
class runtime_context;
class editor_context;
}
namespace wsl::rsc
{
class resource_manager;
}

namespace editor
{

class resource_inspector
{
public:
  resource_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                      wsl::comp::singl::editor_context *editor_ctx);
  ~resource_inspector ();

  void draw ();
  void new_scene_dialog ();

  /*! \brief Request to open a material in the Shader Graph (set by UI). */
  std::string m_request_open_shader_graph;

private:
  void draw_models ();
  void draw_images ();
  void draw_cubemaps ();
  void draw_scenes ();
  void draw_audio ();
  void draw_ui_layouts ();
  void draw_fonts ();
  void draw_materials ();
  void draw_shaders ();

  void import_model_dialog ();
  void import_image_dialog ();
  void import_scene_dialog ();
  void save_scene_dialog ();
  void import_cubemap_dialog ();
  void import_audio_dialog ();

  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;

  entt::id_type m_selected_model = entt::null;
  entt::id_type m_selected_image = entt::null;
  entt::id_type m_selected_cubemap = entt::null;
  entt::id_type m_selected_scene = entt::null;
  entt::id_type m_selected_audio = entt::null;
  entt::id_type m_selected_ui_layout = entt::null;
  entt::id_type m_selected_font = entt::null;
  entt::id_type m_selected_material = entt::null;
  entt::id_type m_selected_shader = entt::null;

  int m_selected_lod_group = -1;
  int m_selected_lod_level = -1;

  MIX_Track *m_preview_track = nullptr;
  bool m_visible = true;
  bool m_show_preview = true;
};

} // namespace editor
