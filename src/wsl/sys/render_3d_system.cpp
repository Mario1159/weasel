#include "render_3d_system.hpp"

#include "../comp/singl/editor_context.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "comp/camera.hpp"
#include "comp/world_transform.hpp"
#include <cmath>
#include <entt/entity/fwd.hpp>
#include <glm/common.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <vector>

namespace wsl
{

namespace sys
{

void
render_3d_system::on_render_record_draw_cmd (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();

  auto *renderer = runtime_ctx.try_get_active_scene_renderer ();
  if (renderer == nullptr) {
    return;
  }

  renderer->draw_visible_models ();
  renderer->draw_visible_model_outlines ();

  // If in editor mode, draw the infinite grid and camera gizmos.
  // Skip these for subviewport offscreen passes so that editor decorations
  // do not occlude the subviewport's own contents (e.g. skybox).
  bool const is_subviewport_pass
      = ctx.contains<entt::entity> () && ctx.get<entt::entity> () != entt::null;

  if (!is_subviewport_pass && ctx.contains<comp::singl::editor_context *> ()) {
    auto &editor_ctx = *ctx.get<comp::singl::editor_context *> ();
    if (editor_ctx.grid_visible ()) {
      renderer->draw_grid (editor_ctx.grid_camera_pos (),
                           editor_ctx.grid_fog_center (),
                           editor_ctx.grid_fog_radius ());
    }

    // Draw camera gizmos in the 3D pass (editor-only, not tied to physics
    // debug)
    if ((renderer != nullptr)
        && ctx.contains<comp::singl::runtime_context *> ()) {
      auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();
      if (!runtime_ctx.is_running ()) {
        comp::singl::editor_context::resolved_camera rc;
        auto *scene = runtime_ctx.scene_manager ().get_active ();
        bool const have_rc
            = editor_ctx.resolve_game_view_camera (registry, scene, rc);

        // Colors
        glm::vec4 const default_color = glm::vec4 (
            150.0F / 255.0F, 200.0F / 255.0F, 255.0F / 255.0F, 1.0F);
        glm::vec4 const selected_color = glm::vec4 (
            255.0F / 255.0F, 185.0F / 255.0F, 80.0F / 255.0F, 1.0F);

        auto view = registry.view<comp::camera, comp::world_transform> ();
        std::vector<gfx::scene_renderer::debug_vertex> lines;
        lines.reserve (256);

        for (entt::entity const entity : view) {
          if (have_rc && !rc.using_engine_default ()
              && entity == rc.entity ()) {
            continue;
          }

          const comp::camera &camera = view.get<comp::camera> (entity);
          const comp::world_transform &wt
              = view.get<comp::world_transform> (entity);
          glm::mat4 const wtm = wt.value ();

          // compute basis
          glm::vec3 const origin = glm::vec3 (wtm[3]);
          glm::vec3 right = glm::normalize (glm::vec3 (wtm[0]));
          if (!std::isfinite (right.x)) {
            right = glm::vec3 (1, 0, 0);
          }
          glm::vec3 up_guess = glm::normalize (glm::vec3 (wtm[1]));
          if (!std::isfinite (up_guess.x)) {
            up_guess = glm::vec3 (0, 1, 0);
          }
          glm::vec3 forward = glm::normalize (-glm::cross (right, up_guess));
          if (!std::isfinite (forward.x)) {
            forward = glm::vec3 (0, 0, -1);
          }
          glm::vec3 const up = glm::normalize (glm::cross (right, forward));
          right = glm::normalize (glm::cross (forward, up));

          float const vertical_fov_deg
              = glm::clamp (camera.fov (), 1.0F, 175.0F);
          float const aspect_ratio = glm::clamp (
              camera.aspect_ratio () > 0.001F
                  ? camera.aspect_ratio ()
                  : (rc.aspect_ratio () > 0.0F ? rc.aspect_ratio () : 1.0F),
              0.1F, 10.0F);
          float const tan_half_fov
              = std::tan (glm::radians (vertical_fov_deg) * 0.5F);

          float const view_distance
              = glm::distance (renderer->camera_position (), origin);
          float const corner_length
              = glm::clamp (view_distance * 0.18F, 0.45F, 5.0F);
          float const shape_term
              = std::sqrt (1.0F
                           + (tan_half_fov * tan_half_fov
                              * (1.0F + (aspect_ratio * aspect_ratio))));
          float const quad_depth = corner_length / shape_term;
          float const quad_half_height = quad_depth * tan_half_fov;
          float const quad_half_width = quad_half_height * aspect_ratio;

          glm::vec3 const quad_center = origin + forward * quad_depth;
          glm::vec3 const quad[4] = {
            quad_center + up * quad_half_height - right * quad_half_width,
            quad_center + up * quad_half_height + right * quad_half_width,
            quad_center - up * quad_half_height + right * quad_half_width,
            quad_center - up * quad_half_height - right * quad_half_width,
          };

          float const origin_extent
              = glm::clamp (corner_length * 0.12F, 0.08F, 0.35F);

          glm::vec4 const col = (entity == editor_ctx.selected_entity ())
                                    ? selected_color
                                    : default_color;

          auto push_line
              = [&lines, col] (const glm::vec3 &a, const glm::vec3 &b) {
                  lines.push_back (gfx::scene_renderer::debug_vertex{ a, col });
                  lines.push_back (gfx::scene_renderer::debug_vertex{ b, col });
                };

          push_line (origin - right * origin_extent,
                     origin + right * origin_extent);
          push_line (origin - up * origin_extent, origin + up * origin_extent);
          push_line (origin - forward * origin_extent,
                     origin + forward * origin_extent);

          for (int i = 0; i < 4; ++i) {
            push_line (quad[i], quad[(i + 1) % 4]);
            push_line (origin, quad[i]);
          }
        }

        if (!lines.empty ()) {
          renderer->draw_debug_lines (lines, renderer->frame_view ().view_proj);
        }
      }
    }
  }
}

} // namespace sys

} // namespace wsl
