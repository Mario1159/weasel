#pragma once

#include "../gfx/cubemap.hpp"
#include "../gfx/scene_renderer.hpp"

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

bool build_render_frame (entt::registry &registry, comp::singl::runtime_context &runtime_ctx,
                         render_submission &out);

} // namespace sys

} // namespace wsl
