#include "render_2d_system.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/sprite_2d.hpp"
#include "wsl/comp/subviewport.hpp"
#include "wsl/comp/transform_2d.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/rendering_manager.hpp"
#include "wsl/gfx/batch_renderer_2d.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/trigonometric.hpp>

#include <cstdint>

namespace wsl::sys
{

namespace
{

void
submit_sprite (gfx::batch_renderer_2d &renderer, comp::sprite_2d const &sprite,
               glm::vec2 position, glm::vec2 size, float rotation)
{
  gfx::batch_renderer_2d::draw_command cmd;
  cmd.image = sprite.image;
  cmd.position = position;
  cmd.size = size;
  cmd.rotation = rotation;
  cmd.color = sprite.color;
  cmd.uv_offset = sprite.uv_offset;
  cmd.uv_scale = sprite.uv_scale;
  cmd.z_index = sprite.z_index;
  cmd.flip_h = sprite.flip_h;
  cmd.flip_v = sprite.flip_v;
  renderer.submit (cmd);
}

} // namespace

void
render_2d_system::on_render_build_draw_data (entt::registry & /*registry*/)
{
  // Build data is now handled per-viewport during record_draw_cmd
}

void
render_2d_system::on_render_record_draw_cmd (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();

  auto *rendering = runtime.get_active_rendering_manager ();
  if (rendering == nullptr) {
    return;
  }

  auto *renderer_2d = rendering->try_renderer_2d ();
  if (renderer_2d == nullptr) {
    return;
  }

  // Determine which viewport we are currently rendering for.
  entt::entity target_viewport = entt::null;
  if (ctx.contains<entt::entity> ()) {
    target_viewport = ctx.get<entt::entity> ();
  }

  // Sprites with a 3D world_transform (placed in 3D space).
  // Skip entities that also have transform_2d — those are pure 2D sprites
  // and should be rendered only via the transform_2d loop.
  int world_sprites = 0;
  {
    auto view = registry.view<comp::sprite_2d, comp::world_transform> ();
    for (auto entity : view) {
      if (registry.all_of<comp::transform_2d> (entity)) {
        continue;
      }
      entt::entity nearest_vp
          = wsl::comp::find_nearest_viewport (registry, entity);
      if (nearest_vp != target_viewport) {
        continue;
      }
      const auto &sprite = view.get<comp::sprite_2d> (entity);
      const auto &transform = view.get<comp::world_transform> (entity);

      glm::mat4 const m = static_cast<glm::mat4> (transform.value);
      glm::vec2 const position (m[3][0], m[3][1]);
      float const scale_x = glm::length (glm::vec3 (m[0]));
      float const scale_y = glm::length (glm::vec3 (m[1]));
      glm::vec2 const size
          = glm::vec2 (sprite.size) * glm::vec2 (scale_x, scale_y);
      float const rotation = std::atan2 (m[0][1], m[0][0]);

      submit_sprite (*renderer_2d, sprite, position, size, rotation);
      ++world_sprites;
    }
  }

  // Sprites with a native transform_2d (pure 2D placement)
  int t2d_sprites = 0;
  {
    auto view = registry.view<comp::sprite_2d, comp::transform_2d> ();
    for (auto entity : view) {
      entt::entity nearest_vp
          = wsl::comp::find_nearest_viewport (registry, entity);
      if (nearest_vp != target_viewport) {
        continue;
      }
      const auto &sprite = view.get<comp::sprite_2d> (entity);
      const auto &t2d = view.get<comp::transform_2d> (entity);

      glm::vec2 const pos (t2d.position);
      glm::vec2 pivot (t2d.pivot);
      glm::vec2 scale (t2d.scale);
      glm::vec2 size (sprite.size);

      glm::vec2 position = pos - pivot * size * scale;
      glm::vec2 final_size = size * scale;
      float rotation = glm::radians (t2d.rotation);

      submit_sprite (*renderer_2d, sprite, position, final_size, rotation);
      ++t2d_sprites;
    }
  }

  // NOTE: flush is now split into build_and_upload() / draw() so that
  // the vertex upload (copy pass) can happen outside an active render pass.
  // core_systems calls build_and_upload() before begin_3d_pass and draw()
  // inside the pass.
}

} // namespace wsl::sys
