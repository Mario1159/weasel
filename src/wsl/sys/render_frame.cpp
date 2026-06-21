#include "render_frame.hpp"

#include "../comp/hierarchy.hpp"
#include "../comp/model_instance_3d.hpp"
#include "../comp/singl/editor_context.hpp"
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/subviewport.hpp"
#include "../comp/world_transform.hpp"
#include "comp/camera.hpp"
#include "comp/camera_2d.hpp"
#include "comp/transform_2d.hpp"

#include <tracy/Tracy.hpp>
#include "gfx/scene_renderer.hpp"
#include <cstdint>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

namespace wsl
{

void
sys::render_submission::reset ()
{
  view = gfx::scene_renderer::view_state{};
  draw_commands.clear ();
  environment = nullptr;
}

bool
sys::build_render_frame (entt::registry &registry,
                         comp::singl::runtime_context &runtime_ctx,
                         render_submission &out, entt::entity target_viewport)
{
  ZoneScoped;
  out.reset ();

  auto *scene = runtime_ctx.scene_manager.get_active ();
  if (scene == nullptr) {
    return false;
  }

  auto &ctx = registry.ctx ();

  entt::entity effective_camera = entt::null;

  if (runtime_ctx.editor_ctx != nullptr) {
    auto &editor_ctx = *runtime_ctx.editor_ctx;
    wsl::comp::singl::editor_context::resolved_camera resolved_camera;
    if (!editor_ctx.resolve_game_view_camera (registry, scene,
                                              resolved_camera)) {
      return false;
    }

    out.view.valid = resolved_camera.valid;
    out.view.aspect_ratio = resolved_camera.aspect_ratio;
    out.view.world_position = resolved_camera.world_pos;
    out.view.view = resolved_camera.view;
    out.view.proj = resolved_camera.proj;
    out.view.view_proj = resolved_camera.vp;

    effective_camera = resolved_camera.entity;

    // Determine 2D mode from editor camera selection
    if (!runtime_ctx.is_running) {
      auto mode = editor_ctx.resolve_game_view_mode (registry, scene);
      out.is_2d_view = (mode
                            == wsl::comp::singl::editor_context::
                                game_view_mode::mode_2d_edit
                        || mode
                               == wsl::comp::singl::editor_context::
                                   game_view_mode::mode_2d_view);
    }
  } else {
    // Determine the camera to use for this viewport
    if (target_viewport != entt::null) {
      if (auto *sv = registry.try_get<comp::subviewport> (target_viewport)) {
        effective_camera = sv->camera.value;
      }
    } else {
      effective_camera = scene->camera;
    }

    uint32_t w;
    uint32_t h;
    runtime_ctx.window.get_size (w, h);
    float const aspect = (w > 0 && h > 0) ? (float)w / (float)h : 1.0F;

    if (effective_camera != entt::null
        && (registry.all_of<comp::camera, comp::world_transform> (
                effective_camera)
            || registry.all_of<comp::camera_2d, comp::transform_2d> (
                effective_camera))) {

      if (registry.all_of<comp::camera> (effective_camera)) {
        const auto &cam = registry.get<comp::camera> (effective_camera);
        const comp::world_transform &wt
            = registry.get<comp::world_transform> (effective_camera);
        glm::mat4 const wtm = wt.value;

        out.view.valid = true;
        out.view.aspect_ratio = aspect;
        out.view.world_position = glm::vec3 (wtm[3]);
        out.view.view = glm::inverse (wtm);
        out.view.proj = glm::perspective (glm::radians (cam.fov), aspect,
                                          cam.near, cam.far);
      } else {
        const auto &cam2d = registry.get<comp::camera_2d> (effective_camera);
        const auto &t2d = registry.get<comp::transform_2d> (effective_camera);
        float const half_w = (float)w * 0.5F / cam2d.zoom;
        float const half_h = (float)h * 0.5F / cam2d.zoom;

        out.view.valid = true;
        out.view.aspect_ratio = aspect;
        out.view.world_position = glm::vec3 (t2d.position.x, t2d.position.y, 0);
        out.view.view = glm::mat4 (1.0F);
        out.view.proj = glm::ortho (
            t2d.position.x - half_w, t2d.position.x + half_w,
            t2d.position.y + half_h, t2d.position.y - half_h, -1.0F, 1.0F);
        out.is_2d_view = true;
      }
      out.view.view_proj = out.view.proj * out.view.view;
    } else {
      // Standalone Fallback: Look at origin from (0,0,5)
      out.view.valid = true;
      out.view.aspect_ratio = aspect;
      out.view.world_position = glm::vec3 (0, 0, 5);
      out.view.view = glm::lookAt (out.view.world_position, glm::vec3 (0),
                                   glm::vec3 (0, 1, 0));
      out.view.proj
          = glm::perspective (glm::radians (60.0F), aspect, 0.1F, 1000.0F);
      out.view.view_proj = out.view.proj * out.view.view;
    }
  }

  // If the editor did not already set is_2d_view, check the camera entity
  if (!out.is_2d_view && effective_camera != entt::null
      && registry.valid (effective_camera)
      && registry.all_of<comp::camera_2d> (effective_camera)) {
    out.is_2d_view = true;
  }

  // Only collect 3D draw commands for non-2D views
  if (!out.is_2d_view) {
    const entt::entity selected_entity
        = ((runtime_ctx.editor_ctx != nullptr) && !runtime_ctx.is_running)
              ? runtime_ctx.editor_ctx->selected_entity
              : entt::null;

    auto draw_view
        = registry.view<comp::world_transform, comp::model_instance_3d> ();
    out.draw_commands.reserve (draw_view.size_hint ());

    for (entt::entity const entity : draw_view) {
      // Hierarchical filtering: only collect entities belonging to this
      // viewport
      if (comp::find_nearest_viewport (registry, entity) != target_viewport) {
        continue;
      }

      const comp::world_transform &world
          = draw_view.get<comp::world_transform> (entity);
      const comp::model_instance_3d &instance
          = draw_view.get<comp::model_instance_3d> (entity);

      auto model = runtime_ctx.resource_manager.get (instance.id);
      if (!model) {
        continue;
      }

      out.draw_commands.push_back (gfx::scene_renderer::draw_command{
          .model = &(*model),
          .scene_index = instance.scene_index,
          .transform = static_cast<glm::mat4> (world.value),
          .entity = entity,
          .draw_outline = (entity == selected_entity),
          .mip_lod_bias = instance.mip_lod_bias,
          .geometry_lod_bias = instance.geometry_lod_bias,
          .visibility_range = instance.visibility_range,
      });
    }

    if (ctx.contains<comp::singl::rendering_manager> ()) {
      const auto &rendering = ctx.get<comp::singl::rendering_manager> ();
      auto cubemap = runtime_ctx.resource_manager.get (rendering.skybox);
      if (cubemap) {
        out.environment = &(*cubemap);
      }
    }
  }

  return out.view.valid;
}

gfx::scene_renderer::view_state
sys::build_camera_view_state (entt::registry &registry,
                              entt::entity camera_entity,
                              entt::entity fallback_camera,
                              const gfx::viewport &vp, uint32_t window_width,
                              uint32_t window_height)
{
  gfx::scene_renderer::view_state out{};
  out.aspect_ratio = vp.aspect_ratio (window_width, window_height);

  if (camera_entity == entt::null) {
    camera_entity = fallback_camera;
  }

  if (camera_entity != entt::null
      && registry.all_of<comp::camera, comp::world_transform> (camera_entity)) {
    const auto &cam = registry.get<comp::camera> (camera_entity);
    const auto &wt = registry.get<comp::world_transform> (camera_entity);
    glm::mat4 const wtm = wt.value;

    out.valid = true;
    out.world_position = glm::vec3 (wtm[3]);
    out.view = glm::inverse (wtm);
    out.proj = glm::perspective (glm::radians (cam.fov), out.aspect_ratio,
                                 cam.near, cam.far);
    out.view_proj = out.proj * out.view;
  } else if (camera_entity != entt::null
             && registry.all_of<comp::camera_2d, comp::transform_2d> (
                 camera_entity)) {
    const auto &cam2d = registry.get<comp::camera_2d> (camera_entity);
    const auto &t2d = registry.get<comp::transform_2d> (camera_entity);

    float const vp_w = static_cast<float> (window_width);
    float const vp_h = static_cast<float> (window_height);
    float const half_w = vp_w * 0.5F / cam2d.zoom;
    float const half_h = vp_h * 0.5F / cam2d.zoom;

    out.valid = true;
    out.world_position = glm::vec3 (t2d.position.x, t2d.position.y, 0.0F);
    out.view = glm::mat4 (1.0F);
    out.proj = glm::ortho (t2d.position.x - half_w, t2d.position.x + half_w,
                           t2d.position.y + half_h, t2d.position.y - half_h,
                           -1.0F, 1.0F);
    out.view_proj = out.proj * out.view;
  } else {
    // Fallback camera
    out.valid = true;
    out.world_position = glm::vec3 (0, 0, 5);
    out.view
        = glm::lookAt (out.world_position, glm::vec3 (0), glm::vec3 (0, 1, 0));
    out.proj = glm::perspective (glm::radians (60.0F), out.aspect_ratio, 0.1F,
                                 1000.0F);
    out.view_proj = out.proj * out.view;
  }

  return out;
}

} // namespace wsl
