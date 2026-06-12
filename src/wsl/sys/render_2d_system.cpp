#include "render_2d_system.hpp"
#include "wsl/comp/sprite_2d.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/rendering_manager.hpp"
#include "wsl/gfx/batch_renderer_2d.hpp"

namespace wsl::sys
{

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

  auto &renderer_2d = rendering->ensure_renderer_2d (runtime.window, runtime.render_ctx, &runtime.resource_manager);

  auto view = registry.view<comp::sprite_2d, comp::world_transform> ();
  for (auto entity : view) {
    const auto &sprite = view.get<comp::sprite_2d> (entity);
    const auto &transform = view.get<comp::world_transform> (entity);

    glm::mat4 const m = static_cast<glm::mat4> (transform.value);

    gfx::batch_renderer_2d::draw_command cmd;
    cmd.image = sprite.image;
    
    // Convert 3D world transform to 2D screen position (for now just use x, y)
    cmd.position = glm::vec2(m[3][0], m[3][1]);
    
    // Scale is the length of the basis vectors
    float const scale_x = glm::length(glm::vec3(m[0]));
    float const scale_y = glm::length(glm::vec3(m[1]));
    cmd.size = glm::vec2(sprite.size) * glm::vec2(scale_x, scale_y);
    
    // Extract Z rotation from matrix: atan2(m[0][1], m[0][0])
    cmd.rotation = std::atan2(m[0][1], m[0][0]);
    
    cmd.color = sprite.color;
    cmd.uv_offset = sprite.uv_offset;
    cmd.uv_scale = sprite.uv_scale;
    cmd.z_index = sprite.z_index;
    cmd.flip_h = sprite.flip_h;
    cmd.flip_v = sprite.flip_v;

    renderer_2d.submit (cmd);
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
