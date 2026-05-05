#include "editor_context.hpp"

#include "comp/component_meta.hpp"
#include "comp/world_transform.hpp"
#include "debug/debug_renderer.hpp"
#include "input.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/rsc/scene.hpp"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <cmath>
#include <cstdint>
#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>
#include <vector>

#ifdef WEASEL_BUILD_EDITOR
#include "editor/renderer_imgui.hpp"
#include <imgui.h>
#endif

namespace wsl::comp::singl
{

void (*editor_context::on_init_hook)(editor_context*) = nullptr;
void (*editor_context::on_deinit_hook)(editor_context*) = nullptr;

editor_context::editor_context (wsl::comp::singl::runtime_context &runtime_ctx)
    : runtime_ctx (runtime_ctx),
      editor_resources (runtime_ctx)
{
#ifdef WEASEL_BUILD_EDITOR
  spdlog::debug ("editor_context: core constructor started");
  if (on_init_hook != nullptr) {
    on_init_hook(this);
  }
  editor_resources.set_editor_context (this);
#else
  spdlog::warn ("editor_context: initializing WITHOUT WEASEL_BUILD_EDITOR");
#endif

#ifdef WEASEL_BUILD_DIR
  runtime_ctx.resource_manager.set_engine_resource_path (WEASEL_BUILD_DIR);
#elif defined(WEASEL_SOURCE_DIR)
  runtime_ctx.resource_manager.set_engine_resource_path (WEASEL_SOURCE_DIR);
#endif

#ifdef WEASEL_SOURCE_DIR
  wsl_library_path = WEASEL_SOURCE_DIR;
#endif

  // Register engine/builtin fonts
  const std::vector<std::string> engine_fonts = {
    "engine://otf/fanwood.otf",
    "engine://otf/splinesans-regular.otf",
    "engine://otf/splinesans-bold.otf",
    "engine://otf/splinesans-light.otf",
    "engine://otf/splinesans-medium.otf",
    "engine://otf/splinesans-semibold.otf"
  };

  for (const auto& f : engine_fonts) {
    editor_resources.register_font(f);
  }

  // Register editor icons
  icon_signal = editor_resources.register_image ("engine://icons/signal.svg");
  icon_singleton = editor_resources.register_image ("engine://icons/singleton.svg");
  icon_system = editor_resources.register_image ("engine://icons/system.svg");
  icon_entity = editor_resources.register_image ("engine://icons/entity.svg");
  icon_inspector = editor_resources.register_image ("engine://icons/inspector.svg");

  icon_show = editor_resources.register_image ("engine://icons/show.svg");
  icon_hide = editor_resources.register_image ("engine://icons/hide.svg");
  editor_resources.load (icon_show);
  editor_resources.load (icon_hide);

  icon_focus_cam = editor_resources.register_image ("engine://icons/focus_cam.svg");
  icon_reset_cam = editor_resources.register_image ("engine://icons/reset_cam.svg");
  icon_play = editor_resources.register_image ("engine://icons/play.svg");
  editor_resources.load (icon_play);
  icon_pause = editor_resources.register_image ("engine://icons/pause.svg");
  icon_stop = editor_resources.register_image ("engine://icons/stop.svg");
  icon_translate = editor_resources.register_image ("engine://icons/translate.svg");  icon_rotate = editor_resources.register_image ("engine://icons/rotate.svg");
  icon_scale = editor_resources.register_image ("engine://icons/scale.svg");
  icon_refresh = editor_resources.register_image ("engine://icons/refresh.svg");
  editor_resources.load (icon_refresh);
  icon_grid = editor_resources.register_image ("engine://icons/grid.svg");
  editor_resources.load (icon_grid);
  icon_welcome_bg = editor_resources.register_image ("engine://icons/welcome_bg.svg");
  editor_resources.load (icon_welcome_bg);

  // Default editor bindings
  editor_input_map.bindings["toggle_game_focus"] = wsl::input::key_binding{
    .mod = SDL_KMOD_NONE,
    .scancode = SDL_SCANCODE_F1
  };
}

editor_context::~editor_context ()
{
  if (on_deinit_hook != nullptr) {
    on_deinit_hook(this);
  }
}

void
editor_context::re_register_editor_resources ()
{
  spdlog::debug("re_register_editor_resources: called");

  // Re-register all editor icons after a project load clears resources
  icon_signal = editor_resources.register_image ("engine://icons/signal.svg");
  icon_singleton = editor_resources.register_image ("engine://icons/singleton.svg");
  icon_system = editor_resources.register_image ("engine://icons/system.svg");
  icon_entity = editor_resources.register_image ("engine://icons/entity.svg");
  icon_inspector = editor_resources.register_image ("engine://icons/inspector.svg");

  icon_show = editor_resources.register_image ("engine://icons/show.svg");
  icon_hide = editor_resources.register_image ("engine://icons/hide.svg");
  editor_resources.load (icon_show);
  editor_resources.load (icon_hide);

  icon_focus_cam = editor_resources.register_image ("engine://icons/focus_cam.svg");
  icon_reset_cam = editor_resources.register_image ("engine://icons/reset_cam.svg");
  icon_play = editor_resources.register_image ("engine://icons/play.svg");
  editor_resources.load (icon_play);
  icon_pause = editor_resources.register_image ("engine://icons/pause.svg");
  icon_stop = editor_resources.register_image ("engine://icons/stop.svg");
  icon_translate = editor_resources.register_image ("engine://icons/translate.svg");
  icon_rotate = editor_resources.register_image ("engine://icons/rotate.svg");
  icon_scale = editor_resources.register_image ("engine://icons/scale.svg");
  icon_refresh = editor_resources.register_image ("engine://icons/refresh.svg");
  editor_resources.load (icon_refresh);
  icon_grid = editor_resources.register_image ("engine://icons/grid.svg");
  editor_resources.load (icon_grid);
  icon_welcome_bg = editor_resources.register_image ("engine://icons/welcome_bg.svg");
  editor_resources.load (icon_welcome_bg);
}

void
editor_context::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<editor_context> ()
      .type (entt::type_hash<editor_context>::value ())
      .custom<wsl::comp::meta_info> (wsl::comp::meta_info{
          "Editor Context", "Global state for the editor application." })
      .func<&editor_context::custom_inspect> ("custom_inspect"_hs);
}

bool
editor_context::custom_inspect (const char *label,
                                wsl::comp::singl::runtime_context *runtime_ctx_ptr)
{
  (void)label;
  (void)runtime_ctx_ptr;
#ifdef WEASEL_BUILD_EDITOR
  ImGui::Separator ();
  ImGui::Value ("Game Fullscreen", game_fullscreen);
  ImGui::Value ("Pending Project Load",
                pending_project_load.value_or ("None").c_str () != nullptr);
  ImGui::Value ("Selected Entity", (uint32_t)selected_entity);

  if (ImGui::TreeNode ("Editor Camera")) {
    ImGui::DragFloat3 ("Position", &editor_cam_pos.x, 0.1F);
    
    glm::vec3 euler = glm::degrees (glm::eulerAngles (editor_cam_rot));
    if (ImGui::DragFloat3 ("Rotation (Euler)", &euler.x, 0.1F)) {
      editor_cam_rot = glm::quat (glm::radians (euler));
    }

    ImGui::DragFloat ("FOV", &editor_camera.fov, 1.0F, 10.0F, 120.0F);
    ImGui::DragFloat ("Near", &editor_camera.near, 0.01F, 0.01F, 10.0F);
    ImGui::DragFloat ("Far", &editor_camera.far, 10.0F, 10.0F, 10000.0F);

    if (ImGui::Button ("Reset Camera")) {
      reset_editor_camera ();
    }

    ImGui::TreePop ();
  }
#endif

  return false;
}

void
editor_context::resolved_camera::reset ()
{
  valid = false;
  using_engine_default = false;
  entity = entt::null;
  aspect_ratio = 1.0F;
  world_pos = glm::vec3{ 0.0F };
  view = glm::mat4{ 1.0F };
  proj = glm::mat4{ 1.0F };
  vp = glm::mat4{ 1.0F };
}

bool
editor_context::resolve_game_view_camera (entt::registry &registry,
                                          wsl::rsc::scene *scene,
                                          resolved_camera &out) const
{
  out.reset ();

  if (scene == nullptr) {
    return false;
}

  bool const running = runtime_ctx.is_running;

  if (running && scene->camera != entt::null) {
    out.entity = scene->camera;
    out.using_engine_default = false;
  } else if (game_view_camera_mode == game_view_cam_mode::entity
             && game_view_camera_entity != entt::null) {
    out.entity = game_view_camera_entity;
    out.using_engine_default = false;
  } else {
    out.using_engine_default = true;
  }

  uint32_t w;
  uint32_t h;
  runtime_ctx.window.get_size (w, h);
  out.aspect_ratio = (float)w / (float)h;

  if (out.using_engine_default) {
    out.world_pos = editor_cam_pos;
    out.view = glm::mat4_cast (glm::inverse (editor_cam_rot));
    out.view = glm::translate (out.view, -editor_cam_pos);
    out.proj = glm::perspective (glm::radians (editor_camera.fov),
                                 out.aspect_ratio, editor_camera.near,
                                 editor_camera.far);
    out.valid = true;
  } else {
    if (registry.valid (out.entity) && registry.all_of<wsl::comp::camera> (out.entity)) {
      const auto &cam = registry.get<wsl::comp::camera> (out.entity);
      if (registry.all_of<wsl::comp::world_transform> (out.entity)) {
        const auto &wt = registry.get<wsl::comp::world_transform> (out.entity);
        out.world_pos = glm::vec3 (wt.value[3]);
        out.view = glm::inverse (wt.value);
        out.proj = glm::perspective (glm::radians (cam.fov), out.aspect_ratio,
                                     cam.near, cam.far);
        out.valid = true;
      }
    }
  }

  if (out.valid) {
    out.vp = out.proj * out.view;
  }

  return out.valid;
}

void
editor_context::reset_editor_camera ()
{
  cancel_editor_camera_anim ();
  editor_cam_pos = glm::vec3 (6.64463F, 3.4202F, 6.64463F);
  editor_cam_rot = glm::quat (glm::radians (glm::vec3 (-20.0F, 45.0F, 0.0F)));
}

void
editor_context::tick_editor_camera_anim (float dt)
{
  if (m_cam_anim.active) {
    m_cam_anim.tick (editor_cam_pos, editor_cam_rot, dt);
  }
}

void
editor_context::focus_editor_camera_to_point (const glm::vec3 &target_world_pos)
{
  glm::vec3 dir = glm::normalize (editor_cam_pos - target_world_pos);
  if (glm::length (dir) < 0.001F) {
    dir = glm::vec3 (0, 0, 1);
}

  glm::vec3 const new_pos = target_world_pos + dir * 5.0F;
  glm::quat const new_rot = make_look_at_quat (new_pos, target_world_pos,
                                         glm::vec3 (0, 1, 0));

  m_cam_anim.begin (new_pos, new_rot);
}

void
editor_context::cancel_editor_camera_anim ()
{
  m_cam_anim.active = false;
}

glm::quat
editor_context::make_look_at_quat (const glm::vec3 &cam_pos,
                                   const glm::vec3 &target,
                                   const glm::vec3 &up) 
{
  glm::mat4 const m = glm::lookAt (cam_pos, target, up);
  return glm::inverse (glm::quat_cast (m));
}

void
editor_context::camera_anim::begin (const glm::vec3 &pos, const glm::quat &rot)
{
  active = true;
  target_pos = pos;
  target_rot = rot;
}

void
editor_context::camera_anim::tick (glm::vec3 &pos, glm::quat &rot, float dt)
{
  float const t = 1.0F - std::exp (-speed * dt);
  pos = glm::mix (pos, target_pos, t);
  rot = glm::slerp (rot, target_rot, t);

  if (glm::distance (pos, target_pos) < pos_eps
      && glm::abs (glm::dot (rot, target_rot)) > 1.0F - rot_eps) {
    pos = target_pos;
    rot = target_rot;
    active = false;
  }
}

} // namespace wsl::comp::singl
