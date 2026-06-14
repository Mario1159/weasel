#include "render_2d_system.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/sprite_2d.hpp"
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
render_2d_system::on_render_build_draw_data (entt::registry &registry)
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

  auto &renderer_2d = rendering->ensure_renderer_2d (
      runtime.window, runtime.render_ctx, &runtime.resource_manager);

  // Sprites with a 3D world_transform (placed in 3D space)
  {
    auto view = registry.view<comp::sprite_2d, comp::world_transform> ();
    for (auto entity : view) {
      const auto &sprite = view.get<comp::sprite_2d> (entity);
      const auto &transform = view.get<comp::world_transform> (entity);

      glm::mat4 const m = static_cast<glm::mat4> (transform.value);
      glm::vec2 const position (m[3][0], m[3][1]);
      float const scale_x = glm::length (glm::vec3 (m[0]));
      float const scale_y = glm::length (glm::vec3 (m[1]));
      glm::vec2 const size
          = glm::vec2 (sprite.size) * glm::vec2 (scale_x, scale_y);
      float const rotation = std::atan2 (m[0][1], m[0][0]);

      submit_sprite (renderer_2d, sprite, position, size, rotation);
    }
  }

  // Sprites with a native transform_2d (pure 2D placement)
  {
    auto view = registry.view<comp::sprite_2d, comp::transform_2d> ();
    for (auto entity : view) {
      const auto &sprite = view.get<comp::sprite_2d> (entity);
      const auto &t2d = view.get<comp::transform_2d> (entity);

      glm::vec2 const pos (t2d.position);
      glm::vec2 pivot (t2d.pivot);
      glm::vec2 scale (t2d.scale);
      glm::vec2 size (sprite.size);

      glm::vec2 position = pos - pivot * size * scale;
      glm::vec2 final_size = size * scale;
      float rotation = glm::radians (t2d.rotation);

      submit_sprite (renderer_2d, sprite, position, final_size, rotation);
    }
  }

  // Apply the camera_2d projection, if a 2D camera exists.
  auto cam2d_view = registry.view<comp::camera_2d, comp::transform_2d> ();
  for (auto entity : cam2d_view) {
    // Use the first camera_2d entity found.
    const auto &cam2d = cam2d_view.get<comp::camera_2d> (entity);
    const auto &t2d = cam2d_view.get<comp::transform_2d> (entity);

    uint32_t win_w, win_h;
    runtime.window.get_size (win_w, win_h);
    float const vp_w = cam2d.use_window_as_viewport ? static_cast<float> (win_w)
                                                    : cam2d.viewport_size.x;
    float const vp_h = cam2d.use_window_as_viewport ? static_cast<float> (win_h)
                                                    : cam2d.viewport_size.y;
    float const half_w = vp_w * 0.5F / cam2d.zoom;
    float const half_h = vp_h * 0.5F / cam2d.zoom;

    glm::mat4 const proj = glm::ortho (
        t2d.position.x - half_w, t2d.position.x + half_w,
        t2d.position.y + half_h, t2d.position.y - half_h, -1.0F, 1.0F);
    renderer_2d.set_projection (proj);
    break; // only the first camera_2d for now
  }
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

  if (auto *renderer_2d = rendering->try_renderer_2d ()) {
    renderer_2d->flush ();
  }
}

} // namespace wsl::sys
