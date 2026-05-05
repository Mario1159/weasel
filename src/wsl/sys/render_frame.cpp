#include "render_frame.hpp"

#include "../comp/model_instance_3d.hpp"
#include "../comp/singl/editor_context.hpp"
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/world_transform.hpp"
#include "comp/camera.hpp"
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
                          render_submission &out)
{
  out.reset ();

  auto *scene = runtime_ctx.scene_manager.get_active ();
  if (scene == nullptr) {
    return false;
  }

  auto &ctx = registry.ctx ();

  if (runtime_ctx.editor_ctx != nullptr) {
    auto &editor_ctx = *runtime_ctx.editor_ctx;
    wsl::comp::singl::editor_context::resolved_camera resolved_camera;
    if (!editor_ctx.resolve_game_view_camera (registry, scene, resolved_camera)) {
      return false;
    }

    out.view.valid = resolved_camera.valid;
    out.view.aspect_ratio = resolved_camera.aspect_ratio;
    out.view.world_position = resolved_camera.world_pos;
    out.view.view = resolved_camera.view;
    out.view.proj = resolved_camera.proj;
    out.view.view_proj = resolved_camera.vp;
  } else {
    uint32_t w;
    uint32_t h;
    runtime_ctx.window.get_size (w, h);
    float const aspect = (w > 0 && h > 0) ? (float)w / (float)h : 1.0F;

    if (scene->camera != entt::null && registry.all_of<comp::camera, comp::world_transform> (scene->camera)) {
      const auto &cam = registry.get<comp::camera> (scene->camera);
      const auto &wt = registry.get<comp::world_transform> (scene->camera);

      out.view.valid = true;
      out.view.aspect_ratio = aspect;
      out.view.world_position = glm::vec3 (wt.value[3]);
      out.view.view = glm::inverse (wt.value);
      out.view.proj = glm::perspective (glm::radians (cam.fov), aspect,
                                         cam.near, cam.far);
      out.view.view_proj = out.view.proj * out.view.view;
    } else {
      // Standalone Fallback: Look at origin from (0,0,5)
      out.view.valid = true;
      out.view.aspect_ratio = aspect;
      out.view.world_position = glm::vec3 (0, 0, 5);
      out.view.view = glm::lookAt (out.view.world_position, glm::vec3 (0), glm::vec3 (0, 1, 0));
      out.view.proj = glm::perspective (glm::radians (60.0F), aspect, 0.1F, 1000.0F);
      out.view.view_proj = out.view.proj * out.view.view;
    }
  }

  const entt::entity selected_entity
      = ((runtime_ctx.editor_ctx != nullptr) && !runtime_ctx.is_running)
            ? runtime_ctx.editor_ctx->selected_entity
            : entt::null;

  auto draw_view
      = registry.view<comp::world_transform, comp::model_instance_3d> ();
  out.draw_commands.reserve (draw_view.size_hint ());

  for (entt::entity const entity : draw_view) {
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
        .transform = world.value,
        .entity = entity,
        .draw_outline = (entity == selected_entity),
    });
  }

  if (ctx.contains<comp::singl::rendering_manager> ()) {
    const auto &rendering = ctx.get<comp::singl::rendering_manager> ();
    auto cubemap = runtime_ctx.resource_manager.get (rendering.skybox);
    if (cubemap) {
      out.environment = &(*cubemap);
    }
  }

  return out.view.valid;
}

} // namespace wsl
