#include "editor_context.hpp"

#include "comp/component_meta.hpp"
#include "comp/world_transform.hpp"
#include "debug/debug_renderer.hpp"
#include "input.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/subviewport.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/transform_2d.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <climits>
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
#include <unistd.h>
#include <vector>

#include "wsl/gfx/imgui_renderer_interface.hpp"
#include <imgui.h>

namespace wsl::comp::singl
{

editor_context::editor_context (wsl::comp::singl::runtime_context &runtime_ctx)
    : runtime_ctx (runtime_ctx), editor_resources (runtime_ctx)
{
  wsl::log::editor ()->trace ("Core constructor started");
  editor_resources.set_editor_context (this);

  // Only override the engine resource path with compile-time paths if the
  // current path (set by the launcher) does not already contain resources.
  // This allows installed / archived builds to work without recompilation.
  {
    const std::string &current_path
        = runtime_ctx.resource_manager.get_engine_resource_path ();
    bool has_dev_resources = std::filesystem::exists (
        std::filesystem::path (current_path) / "compiled_shaders");
    bool has_packaged_resources = std::filesystem::exists (
        std::filesystem::path (current_path) / "share/weasel/compiled_shaders");

    if (!has_dev_resources && !has_packaged_resources) {
      // Try to derive the install prefix from the executable path.
      // On Linux /usr/local/bin/weasel -> prefix /usr/local ->
      // /usr/local/share/weasel/compiled_shaders
      char exe_buf[PATH_MAX];
      ssize_t len = readlink ("/proc/self/exe", exe_buf, sizeof (exe_buf) - 1);
      if (len != -1) {
        exe_buf[len] = '\0';
        std::filesystem::path exe_path (exe_buf);
        std::filesystem::path prefix = exe_path.parent_path ().parent_path ();
        if (std::filesystem::exists (prefix
                                     / "share/weasel/compiled_shaders")) {
          runtime_ctx.resource_manager.set_engine_resource_path (
              prefix.string ());
        } else {
#ifdef WEASEL_BUILD_DIR
          runtime_ctx.resource_manager.set_engine_resource_path (
              WEASEL_BUILD_DIR);
#elif defined(WEASEL_SOURCE_DIR)
          runtime_ctx.resource_manager.set_engine_resource_path (
              WEASEL_SOURCE_DIR);
#endif
        }
      } else {
#ifdef WEASEL_BUILD_DIR
        runtime_ctx.resource_manager.set_engine_resource_path (
            WEASEL_BUILD_DIR);
#elif defined(WEASEL_SOURCE_DIR)
        runtime_ctx.resource_manager.set_engine_resource_path (
            WEASEL_SOURCE_DIR);
#endif
      }
    }
  }

#ifdef WEASEL_SOURCE_DIR
  wsl_library_path = WEASEL_SOURCE_DIR;
#endif

  // Register engine/builtin fonts
  const std::vector<std::string> engine_fonts = {
    "engine://otf/fanwood.otf",           "engine://otf/splinesans-regular.otf",
    "engine://otf/splinesans-bold.otf",   "engine://otf/splinesans-light.otf",
    "engine://otf/splinesans-medium.otf", "engine://otf/splinesans-semibold.otf"
  };

  for (const auto &f : engine_fonts) {
    editor_resources.register_font (f);
  }

  // Register editor icons
  icon_signal = editor_resources.register_image ("engine://icons/signal.svg");
  icon_singleton
      = editor_resources.register_image ("engine://icons/singleton.svg");
  icon_system = editor_resources.register_image ("engine://icons/system.svg");
  icon_entity = editor_resources.register_image ("engine://icons/entity.svg");
  icon_inspector
      = editor_resources.register_image ("engine://icons/inspector.svg");

  icon_show = editor_resources.register_image ("engine://icons/show.svg");
  icon_hide = editor_resources.register_image ("engine://icons/hide.svg");
  editor_resources.load (icon_show);
  editor_resources.load (icon_hide);

  icon_focus_cam
      = editor_resources.register_image ("engine://icons/focus_cam.svg");
  icon_reset_cam
      = editor_resources.register_image ("engine://icons/reset_cam.svg");
  icon_play = editor_resources.register_image ("engine://icons/play.svg");
  editor_resources.load (icon_play);
  icon_pause = editor_resources.register_image ("engine://icons/pause.svg");
  icon_stop = editor_resources.register_image ("engine://icons/stop.svg");
  icon_translate
      = editor_resources.register_image ("engine://icons/translate.svg");
  icon_rotate = editor_resources.register_image ("engine://icons/rotate.svg");
  icon_scale = editor_resources.register_image ("engine://icons/scale.svg");
  icon_refresh = editor_resources.register_image ("engine://icons/refresh.svg");
  editor_resources.load (icon_refresh);
  icon_grid = editor_resources.register_image ("engine://icons/grid.svg");
  editor_resources.load (icon_grid);
  icon_welcome_bg
      = editor_resources.register_image ("engine://icons/welcome_bg.svg");
  editor_resources.load (icon_welcome_bg);

  // Default editor bindings
  editor_input_map.bindings["toggle_game_focus"]
      = wsl::input::key_binding{ .mod = SDL_KMOD_NONE,
                                 .scancode = SDL_SCANCODE_F1 };
}

editor_context::~editor_context () = default;

wsl::gfx::imgui_renderer_interface *
editor_context::get_imgui_renderer () const
{
  return imgui_renderer ? imgui_renderer.get () : nullptr;
}

wsl::debug::debug_renderer_interface *
editor_context::get_debug_renderer () const
{
  return debug_renderer ? debug_renderer.get () : nullptr;
}

void
editor_context::re_register_editor_resources ()
{
  wsl::log::editor ()->trace ("Re-registering editor resources");

  // Re-register all editor icons after a project load clears resources
  icon_signal = editor_resources.register_image ("engine://icons/signal.svg");
  icon_singleton
      = editor_resources.register_image ("engine://icons/singleton.svg");
  icon_system = editor_resources.register_image ("engine://icons/system.svg");
  icon_entity = editor_resources.register_image ("engine://icons/entity.svg");
  icon_inspector
      = editor_resources.register_image ("engine://icons/inspector.svg");

  icon_show = editor_resources.register_image ("engine://icons/show.svg");
  icon_hide = editor_resources.register_image ("engine://icons/hide.svg");
  editor_resources.load (icon_show);
  editor_resources.load (icon_hide);

  icon_focus_cam
      = editor_resources.register_image ("engine://icons/focus_cam.svg");
  icon_reset_cam
      = editor_resources.register_image ("engine://icons/reset_cam.svg");
  icon_play = editor_resources.register_image ("engine://icons/play.svg");
  editor_resources.load (icon_play);
  icon_pause = editor_resources.register_image ("engine://icons/pause.svg");
  icon_stop = editor_resources.register_image ("engine://icons/stop.svg");
  icon_translate
      = editor_resources.register_image ("engine://icons/translate.svg");
  icon_rotate = editor_resources.register_image ("engine://icons/rotate.svg");
  icon_scale = editor_resources.register_image ("engine://icons/scale.svg");
  icon_refresh = editor_resources.register_image ("engine://icons/refresh.svg");
  editor_resources.load (icon_refresh);
  icon_grid = editor_resources.register_image ("engine://icons/grid.svg");
  editor_resources.load (icon_grid);
  icon_welcome_bg
      = editor_resources.register_image ("engine://icons/welcome_bg.svg");
  editor_resources.load (icon_welcome_bg);
}

void
editor_context::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<editor_context> ()
      .type (entt::type_hash<editor_context>::value ())
      .custom<wsl::comp::meta_info> (wsl::comp::meta_info{
          "Editor Context", "Global state for the editor application.", "" })
      .func<&editor_context::custom_inspect> ("custom_inspect"_hs);
}

bool
editor_context::custom_inspect (
    const char *label, wsl::comp::singl::runtime_context *runtime_ctx_ptr)
{
  (void)label;
  (void)runtime_ctx_ptr;
#ifdef WEASEL_BUILD_EDITOR
  ImGui::Separator ();
  ImGui::Value ("Game Fullscreen", game_fullscreen);
  ImGui::Value ("Pending Project Load",
                pending_project_load.value_or ("None").c_str () != nullptr);
  ImGui::Value ("Selected Entity", (uint32_t)selected_entity);

  if (ImGui::TreeNode ("Editor 3D Camera")) {
    ImGui::DragFloat3 ("Position", &editor_cam_pos.x, 0.1F);

    glm::vec3 euler = glm::degrees (glm::eulerAngles (editor_cam_rot));
    if (ImGui::DragFloat3 ("Rotation (Euler)", &euler.x, 0.1F)) {
      editor_cam_rot = glm::quat (glm::radians (euler));
    }

    ImGui::DragFloat ("FOV", &editor_camera.fov, 1.0F, 10.0F, 120.0F);
    ImGui::DragFloat ("Near", &editor_camera.near, 0.01F, 0.01F, 10.0F);
    ImGui::DragFloat ("Far", &editor_camera.far, 10.0F, 10.0F, 10000.0F);

    if (ImGui::Button ("Reset 3D Camera")) {
      reset_editor_camera ();
    }

    ImGui::TreePop ();
  }

  if (ImGui::TreeNode ("Editor 2D Camera")) {
    ImGui::DragFloat2 ("Position", &editor_cam_2d_pos.x, 0.1F);
    ImGui::DragFloat ("Zoom", &editor_camera_2d.zoom, 0.01F, 0.01F, 100.0F);

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

editor_context::game_view_mode
editor_context::resolve_game_view_mode (entt::registry &registry,
                                        wsl::rsc::scene *scene) const
{
  if (scene == nullptr) {
    return game_view_mode::mode_3d_edit;
  }

  // Determine target viewport
  entt::entity target_viewport = entt::null;
  if (game_view_selected_viewport != entt::null) {
    target_viewport = game_view_selected_viewport;
  } else {
    auto &ctx = scene->get_registry ().ctx ();
    if (ctx.contains<comp::singl::rendering_manager> ()) {
      target_viewport
          = ctx.get<comp::singl::rendering_manager> ().render_viewport;
    }
  }

  switch (game_view_camera_selection) {
  case game_view_camera_sel::editor_3d:
    return game_view_mode::mode_3d_edit;

  case game_view_camera_sel::editor_2d:
    return game_view_mode::mode_2d_edit;

  case game_view_camera_sel::default_runtime:
  case game_view_camera_sel::entity: {
    entt::entity cam_entity = entt::null;
    if (game_view_camera_selection == game_view_camera_sel::default_runtime) {
      if (target_viewport != entt::null) {
        if (auto *sv = registry.try_get<comp::subviewport> (target_viewport)) {
          cam_entity = sv->camera.value;
        }
      } else {
        cam_entity = scene->camera;
      }
    } else {
      cam_entity = game_view_selected_camera_entity;
    }

    if (cam_entity != entt::null && registry.valid (cam_entity)) {
      if (registry.all_of<comp::camera_2d> (cam_entity)) {
        return game_view_mode::mode_2d_view;
      } else if (registry.all_of<comp::camera> (cam_entity)) {
        return game_view_mode::mode_3d_view;
      }
    }
    // Fallback to 3D edit if no valid camera
    return game_view_mode::mode_3d_edit;
  }

  case game_view_camera_sel::default_editor:
  default: {
    // Check if subviewport has render_2d_only
    if (target_viewport != entt::null) {
      if (auto *sv = registry.try_get<comp::subviewport> (target_viewport)) {
        if (sv->render_2d_only) {
          return game_view_mode::mode_2d_edit;
        }
      }
    }

    // Analyze viewport contents
    bool has_3d = false;
    bool has_2d = false;

    auto t3d_view = registry.view<comp::transform> ();
    for (auto e : t3d_view) {
      if (comp::find_nearest_viewport (registry, e) == target_viewport) {
        has_3d = true;
        break;
      }
    }

    if (!has_3d) {
      auto t2d_view = registry.view<comp::transform_2d> ();
      for (auto e : t2d_view) {
        if (comp::find_nearest_viewport (registry, e) == target_viewport) {
          has_2d = true;
          break;
        }
      }
    }

    if (has_3d) {
      return game_view_mode::mode_3d_edit;
    } else if (has_2d) {
      return game_view_mode::mode_2d_edit;
    } else {
      // No entities - default to 3d edit
      return game_view_mode::mode_3d_edit;
    }
  }
  }
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
  entt::entity target_viewport = entt::null;

  if (running) {
    // During play mode, always use the rendering manager's active viewport.
    auto &scene_reg = scene->get_registry ();
    auto &ctx = scene_reg.ctx ();
    if (ctx.contains<comp::singl::rendering_manager> ()) {
      auto &rendering = ctx.get<comp::singl::rendering_manager> ();
      target_viewport = rendering.render_viewport;
    }
  } else {
    // In edit mode, use the toolbar selection.
    if (game_view_selected_viewport != entt::null) {
      target_viewport = game_view_selected_viewport;
    } else {
      auto &scene_reg = scene->get_registry ();
      auto &ctx = scene_reg.ctx ();
      if (ctx.contains<comp::singl::rendering_manager> ()) {
        auto &rendering = ctx.get<comp::singl::rendering_manager> ();
        target_viewport = rendering.render_viewport;
      }
    }
  }

  uint32_t w;
  uint32_t h;
  runtime_ctx.window.get_size (w, h);
  out.aspect_ratio = (float)w / (float)h;

  // During play mode, preserve previous behavior
  if (running) {
    // Resolve camera from the target viewport
    if (target_viewport != entt::null && registry.valid (target_viewport)) {
      if (auto *sv
          = registry.try_get<wsl::comp::subviewport> (target_viewport)) {
        if (sv->camera.value != entt::null
            && registry.valid (sv->camera.value)) {
          out.entity = sv->camera.value;
          out.using_engine_default = false;
        } else {
          out.using_engine_default = true;
        }
      } else {
        bool is_camera
            = registry.all_of<wsl::comp::camera> (target_viewport)
              || registry.all_of<wsl::comp::camera_2d> (target_viewport);
        if (is_camera) {
          out.entity = target_viewport;
          out.using_engine_default = false;
        } else {
          out.using_engine_default = true;
        }
      }
    } else {
      out.using_engine_default = true;
      out.entity = scene->camera;
      if (out.entity != entt::null) {
        out.using_engine_default = false;
      }
    }
  } else {
    // Edit mode: use camera selection
    auto mode = resolve_game_view_mode (registry, scene);

    switch (mode) {
    case game_view_mode::mode_2d_edit:
      out.using_engine_default = true;
      {
        out.world_pos
            = glm::vec3 (editor_cam_2d_pos.x, editor_cam_2d_pos.y, 0.0F);
        // World (0, 0) lands at the top-left of the game view in
        // both the 2D and 3D paths. The projection is
        // `ortho(0, w, h, 0, -1, 1)` — Y=0 is the top in
        // projection space, which becomes the top of the framebuffer
        // after the SDL GPU viewport Y-flip. The camera's
        // `position` is the *top-left* of the visible area (matching
        // the 3D-view top-left convention), and the zoom scales
        // world units. Sprite `transform_2d.position` is therefore
        // in screen-pixel coordinates with (0, 0) at the top-left,
        // just like the 3D view.
        out.proj = glm::ortho (0.0F, (float)w, (float)h, 0.0F, -1.0F, 1.0F);

        glm::mat4 view_mat = glm::mat4 (1.0F);
        view_mat
            = glm::scale (view_mat, glm::vec3 (editor_camera_2d.zoom,
                                               editor_camera_2d.zoom, 1.0F));
        view_mat = glm::translate (
            view_mat,
            glm::vec3 (-editor_cam_2d_pos.x * editor_camera_2d.zoom,
                       -editor_cam_2d_pos.y * editor_camera_2d.zoom, 0.0F));
        out.view = view_mat;
        out.valid = true;
      }
      break;

    case game_view_mode::mode_3d_edit:
    case game_view_mode::mode_3d_fly:
      out.using_engine_default = true;
      out.world_pos = editor_cam_pos;
      out.view = glm::mat4_cast (glm::inverse (editor_cam_rot));
      out.view = glm::translate (out.view, -editor_cam_pos);
      out.proj = glm::perspective (glm::radians (editor_camera.fov),
                                   out.aspect_ratio, editor_camera.near,
                                   editor_camera.far);
      out.valid = true;
      break;

    case game_view_mode::mode_2d_view:
    case game_view_mode::mode_3d_view: {
      entt::entity cam_entity = entt::null;
      if (game_view_camera_selection == game_view_camera_sel::default_runtime) {
        if (target_viewport != entt::null) {
          if (auto *sv
              = registry.try_get<comp::subviewport> (target_viewport)) {
            cam_entity = sv->camera.value;
          }
        } else {
          cam_entity = scene->camera;
        }
      } else {
        cam_entity = game_view_selected_camera_entity;
      }

      if (cam_entity != entt::null && registry.valid (cam_entity)) {
        if (registry.all_of<comp::camera, comp::world_transform> (cam_entity)) {
          const auto &cam = registry.get<comp::camera> (cam_entity);
          const auto &wt = registry.get<comp::world_transform> (cam_entity);
          glm::mat4 const wtm = wt.value;
          out.world_pos = glm::vec3 (wtm[3]);
          out.view = glm::inverse (wtm);
          out.proj = glm::perspective (glm::radians (cam.fov), out.aspect_ratio,
                                       cam.near, cam.far);
          out.valid = true;
          out.entity = cam_entity;
          out.using_engine_default = false;
        } else if (registry.all_of<comp::camera_2d, comp::transform_2d> (
                       cam_entity)) {
          const auto &cam2d = registry.get<comp::camera_2d> (cam_entity);
          const auto &t2d = registry.get<comp::transform_2d> (cam_entity);
          float const vp_w = cam2d.use_window_as_viewport
                                 ? static_cast<float> (w)
                                 : cam2d.viewport_size.x;
          float const vp_h = cam2d.use_window_as_viewport
                                 ? static_cast<float> (h)
                                 : cam2d.viewport_size.y;
          out.world_pos = glm::vec3 (t2d.position.x, t2d.position.y, 0.0F);
          // Same screen-space convention as the 3D view: world
          // (0, 0) lands at the top-left of the game view. The
          // camera's `position` is the *top-left* of the visible
          // area, and the zoom scales world units.
          out.proj = glm::ortho (0.0F, vp_w, (float)vp_h, 0.0F, -1.0F, 1.0F);

          glm::mat4 view_mat = glm::mat4 (1.0F);
          view_mat
              = glm::scale (view_mat, glm::vec3 (cam2d.zoom, cam2d.zoom, 1.0F));
          view_mat = glm::translate (
              view_mat, glm::vec3 (-t2d.position.x * cam2d.zoom,
                                   -t2d.position.y * cam2d.zoom, 0.0F));
          out.view = view_mat;
          out.valid = true;
          out.entity = cam_entity;
          out.using_engine_default = false;
        }
      }
      break;
    }
    }
  }

  // Compute matrices for play-mode runtime cameras
  if (running && out.entity != entt::null && registry.valid (out.entity)) {
    if (registry.all_of<comp::camera, comp::world_transform> (out.entity)) {
      const auto &cam = registry.get<comp::camera> (out.entity);
      const auto &wt = registry.get<comp::world_transform> (out.entity);
      glm::mat4 const wtm = wt.value;
      out.world_pos = glm::vec3 (wtm[3]);
      out.view = glm::inverse (wtm);
      out.proj = glm::perspective (glm::radians (cam.fov), out.aspect_ratio,
                                   cam.near, cam.far);
      out.valid = true;
    } else if (registry.all_of<comp::camera_2d, comp::transform_2d> (
                   out.entity)) {
      const auto &cam2d = registry.get<comp::camera_2d> (out.entity);
      const auto &t2d = registry.get<comp::transform_2d> (out.entity);
      float const vp_w = cam2d.use_window_as_viewport ? static_cast<float> (w)
                                                      : cam2d.viewport_size.x;
      float const vp_h = cam2d.use_window_as_viewport ? static_cast<float> (h)
                                                      : cam2d.viewport_size.y;
      out.world_pos = glm::vec3 (t2d.position.x, t2d.position.y, 0.0F);
      // Top-left convention: the camera's `position` is the top-left
      // of the visible area, matching the 3D-view top-left
      // convention. The zoom scales world units.
      glm::mat4 view_mat = glm::mat4 (1.0F);
      view_mat
          = glm::scale (view_mat, glm::vec3 (cam2d.zoom, cam2d.zoom, 1.0F));
      view_mat = glm::translate (
          view_mat, glm::vec3 (-t2d.position.x * cam2d.zoom,
                               -t2d.position.y * cam2d.zoom, 0.0F));
      out.view = view_mat;
      out.proj = glm::ortho (0.0F, vp_w, (float)vp_h, 0.0F, -1.0F, 1.0F);
      out.valid = true;
    }
  } else if (running && out.using_engine_default) {
    out.world_pos = editor_cam_pos;
    out.view = glm::mat4_cast (glm::inverse (editor_cam_rot));
    out.view = glm::translate (out.view, -editor_cam_pos);
    out.proj
        = glm::perspective (glm::radians (editor_camera.fov), out.aspect_ratio,
                            editor_camera.near, editor_camera.far);
    out.valid = true;
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
  glm::quat const new_rot
      = make_look_at_quat (new_pos, target_world_pos, glm::vec3 (0, 1, 0));

  m_cam_anim.begin (new_pos, new_rot);
}

void
editor_context::cancel_editor_camera_anim ()
{
  m_cam_anim.active = false;
}

glm::quat
editor_context::make_look_at_quat (const glm::vec3 &cam_pos,
                                   const glm::vec3 &target, const glm::vec3 &up)
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
