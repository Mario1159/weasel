#include "shadow_system.hpp"

#include "../comp/directional_light.hpp"
#include "../comp/point_light.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/spot_light.hpp"
#include "../comp/world_transform.hpp"

#include "../gfx/scene_renderer.hpp"

#include <cmath>
#include <cstddef>
#include <entt/entity/fwd.hpp>
#include <glm/common.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

#include <tracy/Tracy.hpp>

namespace wsl
{

namespace sys
{

void
sys::shadow_system::on_render_record_draw_cmd (entt::registry &registry)
{
  ZoneScoped;
  auto &ctx = registry.ctx ();

  // Robust check for runtime_context pointer in context storage
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }

  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();

  auto *renderer = runtime_ctx.try_get_active_scene_renderer ();
  if (renderer == nullptr) {
    return;
  }
  if ((renderer == nullptr) || !renderer->has_active_frame ()) {
    return;
  }

  glm::vec3 caster_direction_world (0.0F, -1.0F, 0.0F);
  bool have_directional_caster = false;

  auto dir_light_view
      = registry.view<comp::world_transform, comp::directional_light> ();
  for (entt::entity const entity : dir_light_view) {
    const comp::world_transform &world
        = dir_light_view.get<comp::world_transform> (entity);
    glm::mat4 const wm = world.value;
    caster_direction_world = -glm::normalize (glm::vec3 (wm[2]));
    have_directional_caster = true;
    break;
  }

  renderer->set_directional_shadows_enabled (have_directional_caster);
  if (have_directional_caster) {
    renderer->set_shadow_direction (caster_direction_world);
  }
  renderer->update_directional_shadow_view ();

  const auto &draws = renderer->visible_draws ();

  if (have_directional_caster) {
    renderer->begin_shadow_pass ();
    for (const auto &draw : draws) {
      if (draw.model == nullptr) {
        continue;
      }

      renderer->draw_model_shadow (*draw.model, draw.scene_index,
                                   draw.transform);
    }
    renderer->end_shadow_pass ();
  }

  auto &spot_shadows = renderer->spot_shadow_maps ();
  for (auto &shadow : spot_shadows) {
    shadow.enabled = false;
    shadow.light_vp = glm::mat4 (1.0F);
  }

  auto spot_light_view
      = registry.view<comp::world_transform, comp::spot_light> ();
  size_t spot_index = 0;
  for (entt::entity const entity : spot_light_view) {
    if (spot_index >= spot_shadows.size ()) {
      break;
    }

    const comp::world_transform &world
        = spot_light_view.get<comp::world_transform> (entity);
    const comp::spot_light &light
        = spot_light_view.get<comp::spot_light> (entity);
    if (!light.cast_shadows) {
      continue;
    }

    glm::mat4 const wm = world.value;
    const glm::vec3 position = glm::vec3 (wm[3]);
    const glm::vec3 direction = -glm::normalize (glm::vec3 (wm[2]));
    const float outer_angle
        = std::acos (glm::clamp (light.outer_cos, -1.0F, 1.0F));
    const glm::mat4 light_vp = renderer->make_spot_light_vp (
        position, direction, outer_angle, 0.1F, light.range);

    auto &shadow = spot_shadows[spot_index];
    shadow.light_vp = light_vp;
    shadow.enabled = true;
    shadow.bias = renderer->shadow_map_bias ();
    shadow.strength = renderer->shadow_map_strength ();
    shadow.map_size = static_cast<float> (renderer->shadow_map_resolution ());

    renderer->begin_spot_shadow_pass (static_cast<int> (spot_index));
    for (const auto &draw : draws) {
      if (draw.model == nullptr) {
        continue;
      }

      renderer->draw_model_spot_shadow (*draw.model, draw.scene_index,
                                        draw.transform, light_vp);
    }
    renderer->end_spot_shadow_pass ();

    ++spot_index;
  }

  auto &point_shadows = renderer->point_shadow_maps ();
  for (auto &shadow : point_shadows) {
    shadow.enabled = false;
  }

  auto point_light_view
      = registry.view<comp::world_transform, comp::point_light> ();
  size_t point_index = 0;
  for (entt::entity const entity : point_light_view) {
    if (point_index >= point_shadows.size ()) {
      break;
    }

    const comp::world_transform &world
        = point_light_view.get<comp::world_transform> (entity);
    const comp::point_light &light
        = point_light_view.get<comp::point_light> (entity);
    if (!light.cast_shadows) {
      continue;
    }

    const glm::vec3 position
        = glm::vec3 (static_cast<glm::mat4> (world.value)[3]);

    auto &shadow = point_shadows[point_index];
    shadow.light_pos = position;
    shadow.near_plane = 0.1F;
    shadow.far_plane = light.shadow_far;
    shadow.bias = light.shadow_bias;
    shadow.enabled = true;
    shadow.strength = renderer->shadow_map_strength ();

    for (int face = 0; face < 6; ++face) {
      const glm::mat4 light_vp = renderer->make_point_light_view_proj (
          position, face, shadow.near_plane, shadow.far_plane);

      renderer->begin_point_shadow_pass (static_cast<int> (point_index), face);
      for (const auto &draw : draws) {
        if (draw.model == nullptr) {
          continue;
        }

        renderer->draw_model_point_shadow (*draw.model, draw.scene_index,
                                           draw.transform, light_vp, position,
                                           shadow.far_plane);
      }
      renderer->end_point_shadow_pass ();
    }

    ++point_index;
  }
}

} // namespace sys

} // namespace wsl
