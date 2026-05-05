#include "app.hpp"

#include "comp/camera.hpp"
#include "math/vector.hpp"
#include "rsc/resource_ids.hpp"
#include "wsl/comp/components.hpp"
#include "wsl/comp/area3d.hpp"
#include "wsl/comp/character_body.hpp"
#include "wsl/comp/directional_light.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/model_instance_3d.hpp"
#include "wsl/comp/point_light.hpp"
#include "wsl/comp/prefab_instance.hpp"
#include "wsl/comp/rigid_body.hpp"
#include "wsl/comp/singl/physics_manager.hpp"
#include "wsl/comp/singl/rendering_manager.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/ui_manager.hpp"
#include "wsl/comp/spot_light.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include "wsl/rsc/scene.hpp"

#include "wsl/log/log.hpp"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <cstdint>
#include <entt/core/type_info.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

namespace wsl
{

app::app (const std::string &name, int width, int height,
          const std::string &engine_res_path)
{
  wsl::log::init ();

  SDL_SetHint (SDL_HINT_GPU_DRIVER, "vulkan");
  if (!SDL_Init (SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
    spdlog::critical ("Failed to initialize SDL: {}", SDL_GetError ());
  } else {
    spdlog::debug ("SDL initialized successfully");
  }

  m_runtime_context = std::make_unique<wsl::comp::singl::runtime_context> (name.c_str (), width, height, engine_res_path);

  if (m_runtime_context->render_ctx.gpu_device == nullptr) {
    spdlog::critical ("CRITICAL: GPU device could not be initialized. Application will likely crash.");
  }

  register_components<
      wsl::comp::hierarchy, wsl::comp::world_transform, wsl::comp::transform, wsl::math::vec3f, wsl::math::quatf,
      wsl::rsc::model_id, wsl::comp::model_instance_3d, wsl::comp::camera, wsl::comp::point_light,
      wsl::comp::spot_light, wsl::comp::directional_light,
      wsl::comp::rigid_body, wsl::comp::area, wsl::comp::character_body,
      wsl::comp::prefab_instance> ();

  // Register primitive types for the inspector
  using namespace entt::literals;
  entt::meta_factory<entt::entity> ().type (entt::type_hash<entt::entity>::value ());
  entt::meta_factory<float> ().type (entt::type_hash<float>::value ());
  entt::meta_factory<int> ().type (entt::type_hash<int>::value ());
  entt::meta_factory<bool> ().type (entt::type_hash<bool>::value ());
  entt::meta_factory<uint32_t> ().type (entt::type_hash<uint32_t>::value ());
  entt::meta_factory<std::string> ().type (entt::type_hash<std::string>::value ());

  wsl::comp::for_each_type<wsl::comp::component_types>::apply ([this]<typename T> () {
    m_runtime_context->component_registry.register_world_component<T> ();
  });

  wsl::comp::singl::runtime_context::register_meta ();
  wsl::comp::singl::ui_manager::register_meta ();

  m_runtime_context->singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
          { "Runtime Context", true });
  m_runtime_context->singleton_registry
      .register_bound_singleton_component<wsl::rsc::scene_manager> (
          { "Scene Manager", true });
  m_runtime_context->singleton_registry
      .register_bound_singleton_component<wsl::rsc::resource_manager_view> (
          { "Resource Manager", true });
  m_runtime_context->singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::ui_manager> (
          { "UI Manager", true, false, true });
  m_runtime_context->singleton_registry
      .register_singleton_component<wsl::comp::singl::rendering_manager> (
          { "Rendering Manager", true });
  m_runtime_context->singleton_registry
      .register_singleton_component<wsl::comp::singl::physics_manager> (
          { "Physics Manager", true });
}

app::~app () { SDL_Quit (); }

void
app::set_project_path (const std::string &path)
{
  spdlog::debug("set_project_path: loading {}", path);
  m_runtime_context->resource_manager.load_project (path);
}

void
app::set_engine_resource_path (const std::string &path)
{
  m_runtime_context->resource_manager.set_engine_resource_path (path);
}

void
app::on_render ()
{
  if (m_runtime_context->core_systems) {
    m_runtime_context->core_systems->render (m_runtime_context->window);
  }
}

int
app::run ()
{
  if (m_runtime_context->render_ctx.gpu_device == nullptr) {
    return -1;
  }

  spdlog::debug("app::run: calling on_init");
  on_init ();
  spdlog::debug("app::run: on_init done, entering main loop");

  uint64_t last_time = SDL_GetTicks ();
  bool quit = false;

  while (!quit) {
    uint64_t const current_time = SDL_GetTicks ();
    double const dt = (current_time - last_time) / 1000.0;
    last_time = current_time;

    SDL_Event e;
    while (SDL_PollEvent (&e) != 0) {
      if (e.type == SDL_EVENT_QUIT) {
        quit = true;
      }
      m_runtime_context->scene_manager.handle_events (e);
      on_event (e);
    }

    m_runtime_context->resource_manager.update_async_uploads ();
    m_runtime_context->scene_manager.update (dt);
    on_update (dt);
    on_render ();
  }

  return 0;
}

} // namespace wsl
