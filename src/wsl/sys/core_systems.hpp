#pragma once

#include "lighting_system.hpp"
#include "audio_system.hpp"
#include "physics_system.hpp"
#include "render_3d_system.hpp"
#include "render_ui_system.hpp"
#include "shadow_system.hpp"
#include "skybox_system.hpp"
#include "transform_system.hpp"

#include <SDL3/SDL_events.h>
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace wsl
{
namespace gfx
{
class render_window;
}
namespace comp::singl
{
class runtime_context;
class editor_context;
}
}

namespace wsl
{

namespace sys
{

class core_systems
{
public:
  struct render_callbacks
  {
    std::function<void (entt::registry &)> build_draw_data;
    std::function<void (entt::registry &)> prepare_gpu_rsc;
    std::function<void (entt::registry &)> record_ui_draw_cmd;
  };

  core_systems ();

  void init (comp::singl::runtime_context *runtime_ctx,
             comp::singl::editor_context *editor_ctx);
  void sync_activation ();
  void update (double dt);
  void event_handler (const SDL_Event &e);
  void render (wsl::gfx::render_window &window,
               const render_callbacks &callbacks = {});

  std::vector<sys::ecs_system *> to_vec () const;

  std::unique_ptr<render_3d_system> render_3d_sys;
  std::unique_ptr<physics_system> physics_sys;
  
  std::unique_ptr<render_ui_system> render_ui_sys;
  std::unique_ptr<lighting_system> lighting_sys;
  std::unique_ptr<skybox_system> skybox_sys;
  std::unique_ptr<transform_system> transform_sys;
  std::unique_ptr<shadow_system> shadow_sys;
  std::unique_ptr<audio_system> audio_sys;

private:
  void register_debug_metadata ();
  void ensure_dummy_context_bindings ();
  void render_impl (wsl::gfx::render_window &window,
                    const render_callbacks &callbacks);

  comp::singl::runtime_context *m_runtime_ctx = nullptr;
  comp::singl::editor_context *m_editor_ctx = nullptr;
  entt::registry *m_bound_registry = nullptr;
  entt::registry m_dummy_registry;
};

} // namespace sys

} // namespace wsl
