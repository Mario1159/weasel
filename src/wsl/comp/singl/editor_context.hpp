#pragma once

#include "wsl/comp/singl/engine_resources.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/input.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/camera_2d.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <memory>

#ifdef WEASEL_BUILD_EDITOR
#include <imgui.h>
#endif

namespace wsl::gfx
{
class imgui_renderer_interface;
}
namespace wsl::debug
{
class debug_renderer_interface;
}

namespace wsl
{
namespace rsc
{
class scene;
}

namespace comp::singl
{

class editor_context : public comp::singleton_component
{
public:
  explicit editor_context (wsl::comp::singl::runtime_context &runtime_ctx);
  ~editor_context (); // Defined in .cpp where renderer types are complete

  editor_context (const editor_context &) = delete;
  editor_context &operator= (const editor_context &) = delete;
  editor_context (editor_context &&) = delete;
  editor_context &operator= (editor_context &&) = delete;

  static void register_meta ();
  bool custom_inspect (const char *label,
                       wsl::comp::singl::runtime_context *runtime_ctx);

  wsl::comp::singl::runtime_context &runtime_ctx;

  // Editor-specific renderers (created by wsl::editor_app factory methods)
  std::unique_ptr<wsl::gfx::imgui_renderer_interface> imgui_renderer;
  std::unique_ptr<wsl::debug::debug_renderer_interface> debug_renderer;

  wsl::gfx::imgui_renderer_interface *get_imgui_renderer () const;
  wsl::debug::debug_renderer_interface *get_debug_renderer () const;

  enum class game_view_cam_mode
  {
    engine_default,
    entity
  };
  game_view_cam_mode game_view_camera_mode = game_view_cam_mode::engine_default;
  entt::entity game_view_camera_entity = entt::null;

  // Viewport combobox selection in the game view toolbar.
  // entt::null = root viewport (use rendering_manager::render_viewport).
  entt::entity game_view_selected_viewport = entt::null;

  // Game view mode determines editor interactions and camera.
  enum class game_view_mode
  {
    mode_2d_edit,
    mode_3d_edit,
    mode_2d_view,
    mode_3d_view,
    mode_3d_fly,
  };

  // Camera combobox selection in the game view toolbar.
  enum class game_view_camera_sel
  {
    default_editor,
    editor_3d,
    editor_2d,
    default_runtime,
    entity,
  };

  game_view_camera_sel game_view_camera_selection
      = game_view_camera_sel::default_editor;
  entt::entity game_view_selected_camera_entity = entt::null;

  // Editor state (engine default camera = NOT an entity)
  wsl::comp::camera editor_camera{};
  glm::vec3 editor_cam_pos{ 6.64463F, 3.4202F, 6.64463F };
  glm::quat editor_cam_rot{ glm::radians (glm::vec3 (-20.0F, 45.0F, 0.0F)) };

  // Editor 2D camera state
  wsl::comp::camera_2d editor_camera_2d{};
  glm::vec2 editor_cam_2d_pos{ 0.0F, 0.0F };

  // Editor selection used by rendering/editor overlays
  entt::entity selected_entity = entt::null;

  // Editor resources
  wsl::comp::singl::engine_resources editor_resources;

  wsl::input::action_map editor_input_map;
  bool game_fullscreen = false;
  std::optional<std::string> pending_project_load;

  // Editor icons
  rsc::image_id icon_signal;
  rsc::image_id icon_singleton;
  rsc::image_id icon_system;
  rsc::image_id icon_entity;
  rsc::image_id icon_inspector;

  rsc::image_id icon_show;
  rsc::image_id icon_hide;

  rsc::image_id icon_focus_cam;
  rsc::image_id icon_reset_cam;
  rsc::image_id icon_play;
  rsc::image_id icon_pause;
  rsc::image_id icon_stop;
  rsc::image_id icon_translate;
  rsc::image_id icon_rotate;
  rsc::image_id icon_scale;
  rsc::image_id icon_refresh;
  rsc::image_id icon_grid;
  rsc::image_id icon_welcome_bg;

  std::string wsl_library_path;
  bool is_loading_project = false;

  void re_register_editor_resources ();

  glm::vec2 last_img_min{ 0.0F, 0.0F };
  glm::vec2 last_img_size{ 0.0F, 0.0F };

  // Grid configuration for 3D rendering
  bool grid_visible = true;
  glm::vec3 grid_camera_pos{ 0.0F };
  glm::vec3 grid_fog_center{ 0.0F };
  float grid_fog_radius = 400.0F;

  // -------- Camera resolve helper --------
  class resolved_camera
  {
  public:
    bool valid = false;

    // what source are we using?
    bool using_engine_default = false; // engine/editor camera (non-entity)
    entt::entity entity = entt::null;  // if using an entity camera

    // camera outputs
    float aspect_ratio = 1.0F;
    glm::vec3 world_pos{ 0.0F };
    glm::mat4 view{ 1.0F };
    glm::mat4 proj{ 1.0F };
    glm::mat4 vp{ 1.0F };

    void reset ();
  };

  // Resolves the game view mode based on camera selection and viewport.
  game_view_mode resolve_game_view_mode (entt::registry &registry,
                                         wsl::rsc::scene *scene) const;

  // Resolves the camera used to render the Game View:
  // - Running: scene->camera
  // - Paused:  (entity override if selected) else engine default camera
  bool resolve_game_view_camera (entt::registry &registry,
                                 wsl::rsc::scene *scene,
                                 resolved_camera &out) const;

  // Camera helpers
  void reset_editor_camera (); // default: look at origin from a fixed distance

  // Smooth camera animation tick (call once per frame)
  void tick_editor_camera_anim (float dt);

  // Focus: moves editor camera near target and looks at it (smooth)
  void focus_editor_camera_to_point (const glm::vec3 &target_world_pos);

  void cancel_editor_camera_anim ();

private:
  // Internal: compute a camera rotation so that camera looks at target
  static glm::quat make_look_at_quat (const glm::vec3 &cam_pos,
                                      const glm::vec3 &target,
                                      const glm::vec3 &up);

  struct camera_anim
  {
    bool active = false;
    glm::vec3 target_pos{ 0.0F };
    glm::quat target_rot{ 1.0F, 0.0F, 0.0F, 0.0F };

    // Exponential smoothing speed (bigger = faster)
    float speed = 18.0F;

    // Stop thresholds
    float pos_eps = 0.0005F;
    float rot_eps = 0.0005F;

    void begin (const glm::vec3 &pos, const glm::quat &rot);
    void tick (glm::vec3 &pos, glm::quat &rot, float dt);
  };

  camera_anim m_cam_anim;

  template <class Archive>
  void
  serialize (Archive & /*unused*/)
  {
  }
};

} // namespace comp::singl

} // namespace wsl
