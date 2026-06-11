#pragma once

#include "../gfx/cubemap.hpp"
#include "../gfx/scene_renderer.hpp"
#include "../gfx/viewport.hpp"

#include <entt/entt.hpp>

namespace wsl
{

namespace comp::singl
{
class runtime_context;
}

namespace sys
{

struct render_submission
{
  gfx::scene_renderer::view_state view{};
  std::vector<gfx::scene_renderer::draw_command> draw_commands;
  const gfx::cubemap *environment = nullptr;

  void reset ();
};

bool build_render_frame (entt::registry &registry,
                         comp::singl::runtime_context &runtime_ctx,
                         render_submission &out);

/*!
 * \brief Builds a view_state for a specific camera entity and viewport.
 *
 * If camera_entity is entt::null, fallback_camera is used. If that is also
 * entt::null, a default fallback camera at (0,0,5) is created.
 */
gfx::scene_renderer::view_state
build_camera_view_state (entt::registry &registry, entt::entity camera_entity,
                         entt::entity fallback_camera, const gfx::viewport &vp,
                         uint32_t window_width, uint32_t window_height);

} // namespace sys

} // namespace wsl
