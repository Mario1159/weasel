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
#ifdef WEASEL_ENABLE_RENDERDOC
#include "wsl/gfx/renderdoc.hpp"
#endif
#include "wsl/sys/tracy_telemetry.hpp"
#include <tracy/Tracy.hpp>
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
#include <string>

namespace wsl
{

namespace
{

// Snapshot hook installed into the Tracy telemetry thread. The
// background thread reads these fields every 250 ms. `m_runtime_ctx`
// itself is a unique_ptr that outlives the telemetry thread, so
// reading the pointer is safe.
wsl::comp::singl::runtime_context *s_tracy_rt = nullptr;
double s_smoothed_fps = 0.0;

void
tracy_runtime_snapshot (uint64_t &frame_index, double &fps, bool &is_running,
                        bool &in_play_session)
{
  if (s_tracy_rt == nullptr) {
    frame_index = 0;
    fps = 0.0;
    is_running = false;
    in_play_session = false;
    return;
  }
  frame_index = s_tracy_rt->render_ctx().frame_index ();
  fps = s_smoothed_fps;
  is_running = s_tracy_rt->is_running();
  in_play_session = s_tracy_rt->in_play_session();
}

} // namespace

app::app (const std::string &name, int width, int height,
          const std::string &engine_res_path)
{
  wsl::log::init ();

  // Label the main thread for Tracy. The Tracy docs require a
  // string literal here — the API stores the pointer and expects
  // the bytes to live for the entire process. Without this, the
  // thread shows up as "Main thread" (Tracy's default) which is
  // ambiguous in a process that spawns multiple worker threads.
  // (tracy::SetThreadName is a no-op when TRACY_ENABLE is not
  // defined, so this is safe in release builds.)
  tracy::SetThreadName ("Engine Main");

#ifdef WEASEL_ENABLE_RENDERDOC
  // RenderDoc must be initialised before the SDL_GPUDevice is created
  // so that capture options (vsync, callstacks, ...) take effect. This
  // is a safe no-op when the RenderDoc module is not loaded.
  wsl::gfx::rdoc::init ();
#endif

  m_runtime_context = std::make_unique<wsl::comp::singl::runtime_context> (
      name.c_str (), width, height, engine_res_path);

  // Start the periodic memory / playback / frame telemetry tick
  // AFTER the runtime context exists so the snapshot function can
  // read it. The thread is joined in ~app before the unique_ptr is
  // destroyed.
  s_tracy_rt = m_runtime_context.get ();
  wsl::sys::tracy_telemetry_init (tracy_runtime_snapshot);

  if (m_runtime_context->render_ctx().gpu_device == nullptr) {
    wsl::log::core ()->critical (
        "CRITICAL: GPU device could not be initialized. "
        "Application will likely crash.");
  }

  register_components<wsl::comp::hierarchy, wsl::comp::world_transform,
                      wsl::comp::transform, wsl::math::vec2f, wsl::math::vec3f,
                      wsl::math::vec4f, wsl::math::quatf, wsl::math::mat33f,
                      wsl::math::mat44f, wsl::rsc::model_id, wsl::rsc::image_id,
                      wsl::comp::model_instance_3d, wsl::comp::camera,
                      wsl::comp::point_light, wsl::comp::spot_light,
                      wsl::comp::directional_light, wsl::comp::rigid_body,
                      wsl::comp::area, wsl::comp::character_body,
                      wsl::comp::prefab_instance, wsl::comp::sprite_2d> ();

  // Register primitive types for the inspector
  using namespace entt::literals;
  entt::meta_factory<entt::entity> ().type (
      entt::type_hash<entt::entity>::value ());
  entt::meta_factory<float> ().type (entt::type_hash<float>::value ());
  entt::meta_factory<int> ().type (entt::type_hash<int>::value ());
  entt::meta_factory<bool> ().type (entt::type_hash<bool>::value ());
  entt::meta_factory<uint32_t> ().type (entt::type_hash<uint32_t>::value ());
  entt::meta_factory<std::string> ().type (
      entt::type_hash<std::string>::value ());

  wsl::comp::for_each_type<wsl::comp::component_types>::apply (
      [this]<typename T> () {
        m_runtime_context->component_registry().register_world_component<T> ();
      });

  wsl::comp::singl::runtime_context::register_meta ();
  wsl::comp::singl::ui_manager::register_meta ();

  m_runtime_context->singleton_registry()
      .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
          { "Runtime Context", true });
  m_runtime_context->singleton_registry()
      .register_bound_singleton_component<wsl::rsc::scene_manager> (
          { "Scene Manager", true });
  m_runtime_context->singleton_registry()
      .register_bound_singleton_component<wsl::rsc::resource_manager_view> (
          { "Resource Manager", true });
  m_runtime_context->singleton_registry()
      .register_bound_singleton_component<wsl::comp::singl::ui_manager> (
          { "UI Manager", true, false, true });
  m_runtime_context->singleton_registry()
      .register_singleton_component<wsl::comp::singl::rendering_manager> (
          { "Rendering Manager", true });
  m_runtime_context->singleton_registry()
      .register_singleton_component<wsl::comp::singl::physics_manager> (
          { "Physics Manager", true });

  wsl::log::core ()->info (
      "App initialized ({} world components, {} singletons, {} engine systems)",
      m_runtime_context->component_registry().get_world_components ().size (),
      m_runtime_context->singleton_registry().get_singleton_components ().size (),
      m_runtime_context->core_systems()
          ? m_runtime_context->core_systems()->to_vec ().size ()
          : 0);
}

app::~app ()
{
  // Shut down the Tracy telemetry thread BEFORE the runtime context
  // is destroyed so the background thread can't read freed memory.
  s_tracy_rt = nullptr;
  wsl::sys::tracy_telemetry_shutdown ();

#ifdef WEASEL_ENABLE_RENDERDOC
  wsl::gfx::rdoc::shutdown ();
#endif
}

void
app::set_project_path (const std::string &path)
{
  wsl::log::core ()->trace ("Loading project from {}", path);
  m_runtime_context->resource_manager().load_project (path);
}

void
app::set_engine_resource_path (const std::string &path)
{
  m_runtime_context->resource_manager().set_engine_resource_path (path);
}

void
app::on_render ()
{
  if (m_runtime_context->core_systems()) {
    m_runtime_context->core_systems()->render (m_runtime_context->window());
  }
}

int
app::run ()
{
  if (m_runtime_context->render_ctx().gpu_device == nullptr) {
    return -1;
  }

  wsl::log::core ()->trace ("Entering main loop");
  on_init ();

  if (m_runtime_context->editor_ctx() == nullptr) {
    m_runtime_context->set_running (true);
  }

  uint64_t last_time = SDL_GetTicks ();
  bool quit = false;

  while (!quit) {
    uint64_t const current_time = SDL_GetTicks ();
    double const dt = static_cast<double> (current_time - last_time) / 1000.0;
    last_time = current_time;

    // Update the EWMA-smoothed FPS that the Tracy telemetry thread
    // reads. First-frame dt can be 0 or enormous (just-initialised
    // state); guard against both.
    if (dt > 1e-6 && dt < 1.0) {
      double const instant = 1.0 / dt;
      s_smoothed_fps = (0.1 * instant) + (0.9 * s_smoothed_fps);
    }

    on_update (dt);

    SDL_Event e;
    while (SDL_PollEvent (&e) != 0) {
      if (e.type == SDL_EVENT_QUIT) {
        quit = true;
      }
      if (e.type == SDL_EVENT_WINDOW_RESIZED
          && e.window.windowID
                 == SDL_GetWindowID (m_runtime_context->window().handler())) {
        m_runtime_context->window().on_resize ();
      }
      m_runtime_context->scene_manager().handle_events (e);
      on_event (e);
    }

    // "Update" sub-frame: physics, ECS systems, etc. Closes before
    // "Render" starts. The two are side-by-side rows in Tracy's
    // Frame view; their gap equals the main frame time.
    wsl::sys::tracy_telemetry_secondary_frame_begin ("Update");
    m_runtime_context->core_systems()->update (dt);
    on_update (dt);
    wsl::sys::tracy_telemetry_secondary_frame_end ("Update");

    wsl::sys::tracy_telemetry_secondary_frame_begin ("Render");
    on_render ();
    wsl::sys::tracy_telemetry_secondary_frame_end ("Render");

    // Main frame boundary. Tracy reads frame time from the gap
    // between consecutive FrameMark calls. The label is shown in
    // the Frame view alongside the per-frame zone stack.
    wsl::sys::tracy_telemetry_frame_mark (
        m_runtime_context->render_ctx().frame_index ());
  }

  wsl::log::core ()->debug ("Exiting main loop");
  return 0;
}

} // namespace wsl
