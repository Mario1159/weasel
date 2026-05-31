#include "core_systems.hpp"
#include "comp/singl/ui_manager.hpp"
#include "rsc/resource_manager.hpp"
#include "sys/audio_system.hpp"
#include "sys/lighting_system.hpp"
#include "sys/physics_system.hpp"
#include "sys/render_3d_system.hpp"
#include "sys/render_ui_system.hpp"
#include "sys/shadow_system.hpp"
#include "sys/skybox_system.hpp"
#include "sys/system.hpp"
#include "sys/transform_system.hpp"
#include "wsl/log/log.hpp"
#include <SDL3/SDL_events.h>
#include <algorithm>
#include <entt/entity/fwd.hpp>
#include <glm/ext/vector_float3.hpp>
#include <memory>
#include <vector>

#ifdef WEASEL_BUILD_EDITOR
#include "wsl/comp/singl/editor_context.hpp"
#endif
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "render_frame.hpp"

#include <utility>

namespace wsl
{

namespace
{

void
apply_rendering_manager (entt::registry &registry,
                         comp::singl::runtime_context &runtime_ctx)
{
  if (!registry.ctx ().contains<comp::singl::rendering_manager> ()) {
    return;
  }

  auto &rendering = registry.ctx ().get<comp::singl::rendering_manager> ();
  auto &renderer
      = rendering.ensure_renderer (runtime_ctx.window, runtime_ctx.render_ctx,
                                   &runtime_ctx.resource_manager);
  renderer.ssao_enabled = rendering.ssao_enabled;
  renderer.ssao_radius = rendering.ssao_radius;
  renderer.ssao_bias = rendering.ssao_bias;
  renderer.ssao_power = rendering.ssao_power;
  renderer.ssao_intensity = rendering.ssao_intensity;
  renderer.bloom_threshold = rendering.bloom_threshold;
  renderer.bloom_knee = rendering.bloom_knee;
  renderer.bloom_intensity = rendering.bloom_intensity;
  renderer.outline_color
      = glm::vec4 (static_cast<glm::vec3> (rendering.outline_color),
                   rendering.outline_alpha);
  renderer.outline_width = rendering.outline_width;
  renderer.set_shadow_map_bias (rendering.shadow_bias);
  renderer.set_shadow_map_strength (rendering.shadow_strength);
  renderer.set_ibl_intensity (rendering.ibl_intensity);

  runtime_ctx.window.scene_clear_color
      = SDL_FColor{ rendering.clear_color.x, rendering.clear_color.y,
                    rendering.clear_color.z, rendering.clear_alpha };
  runtime_ctx.window.exposure = rendering.exposure;
  runtime_ctx.window.bloom_intensity = rendering.bloom_intensity;
}

} // namespace

namespace sys
{

core_systems::core_systems () = default;

void
core_systems::register_debug_metadata ()
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    m_runtime_ctx->signal_hub.clear_system_declarations (sys->get_type_id ());
    sys->register_signals (m_runtime_ctx->signal_hub);
    sys->register_event_handlers (m_runtime_ctx->signal_hub);
    sys->register_iterations (m_runtime_ctx->signal_hub);
  }

  if (rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ()) {
    for (auto &sys : scene->systems) {
      m_runtime_ctx->signal_hub.clear_system_declarations (sys->get_type_id ());
      sys->register_signals (m_runtime_ctx->signal_hub);
      sys->register_event_handlers (m_runtime_ctx->signal_hub);
      sys->register_iterations (m_runtime_ctx->signal_hub);
    }
  }
}

void
core_systems::init (comp::singl::runtime_context *runtime_ctx,
                    comp::singl::editor_context *editor_ctx)
{
  (void)editor_ctx;
  this->m_runtime_ctx = runtime_ctx;
  this->m_editor_ctx = editor_ctx;

  ensure_dummy_context_bindings ();

  if (!render_3d_sys) {
    render_3d_sys = std::make_unique<render_3d_system> ("3D Render System");
  }
  if (!physics_sys) {
    physics_sys = std::make_unique<physics_system> ("Jolt Physics System");
  }
  if (!render_ui_sys) {
    render_ui_sys
        = std::make_unique<render_ui_system> ("Application UI System");
  }
  if (!lighting_sys) {
    lighting_sys = std::make_unique<lighting_system> ("Lighting System");
  }
  if (!skybox_sys) {
    skybox_sys = std::make_unique<skybox_system> ("Skybox System");
  }
  if (!transform_sys) {
    transform_sys = std::make_unique<transform_system> ("Transform System");
  }
  if (!shadow_sys) {
    shadow_sys = std::make_unique<shadow_system> ("Shadow System");
  }
  if (!audio_sys) {
    audio_sys = std::make_unique<audio_system> ("Audio System");
  }

  // Register system factories so CLI `sys add` can create them dynamically.
  if (m_runtime_ctx) {
    auto &factory = m_runtime_ctx->system_factory_registry;
    factory.register_system_type<transform_system> ({ "Transform" });
    factory.register_system_type<physics_system> ({ "Physics" });
    factory.register_system_type<render_3d_system> ({ "3D Render" });
    factory.register_system_type<audio_system> ({ "Audio" });
    factory.register_system_type<lighting_system> ({ "Lighting" });
    factory.register_system_type<skybox_system> ({ "Skybox" });
    factory.register_system_type<shadow_system> ({ "Shadow" });
    factory.register_system_type<render_ui_system> ({ "UI" });
  }

  register_debug_metadata ();

  sync_activation ();

  wsl::log::sys ()->debug ("Initialized {} built-in systems",
                           to_vec ().size ());
}

void
core_systems::sync_activation ()
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  ensure_dummy_context_bindings ();

  rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();
  entt::registry *registry
      = (scene != nullptr) ? &scene->get_registry () : &m_dummy_registry;

  if ((m_bound_registry != nullptr) && m_bound_registry != registry) {
    wsl::log::sys ()->trace ("Shutting down systems for old registry");
    for (sys::ecs_system *sys : to_vec ()) {
      if (sys == nullptr) {
        continue;
      }

      sys->shutdown (m_bound_registry);
    }
  }

  m_bound_registry = registry;
  if (registry == nullptr) {
    return;
  }

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->refresh_activation (registry, m_runtime_ctx->is_running);
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      sys->refresh_activation (registry, m_runtime_ctx->is_running);
    }
  }
}

void
core_systems::update (double dt)
{
  sync_activation ();
  m_runtime_ctx->sync ();
  m_runtime_ctx->resource_manager.update_async_uploads ();

  if (m_editor_ctx != nullptr) {
    m_editor_ctx->editor_resources.update_async_uploads ();
  }

  rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();
  entt::registry &registry
      = (scene != nullptr) ? scene->get_registry () : m_dummy_registry;

  int active_count = 0;
  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->update (&registry, dt);
    if (sys->is_active ()) {
      ++active_count;
    }
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      sys->update (&registry, dt);
      active_count++;
    }
  }

  wsl::log::sys ()->trace ("Update: {} active systems, dt={}s, scene='{}'",
                           active_count, dt,
                           scene ? scene->get_name ().c_str () : "(none)");
}

void
core_systems::event_handler (const SDL_Event &e)
{
  sync_activation ();

  rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();
  entt::registry &registry
      = (scene != nullptr) ? scene->get_registry () : m_dummy_registry;

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->event_handler (&registry, e);
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      sys->event_handler (&registry, e);
    }
  }
}

void
core_systems::render (wsl::gfx::render_window &window,
                      const render_callbacks &callbacks)
{
  render_impl (window, callbacks);
}

std::vector<sys::ecs_system *>
core_systems::to_vec () const
{
  std::vector<sys::ecs_system *> out;
  out.reserve (10);

  const auto push = [&out] (sys::ecs_system *sys) {
    if (sys) {
      out.push_back (sys);
    }
  };

  push (audio_sys.get ());
  push (physics_sys.get ());
  push (transform_sys.get ());
  push (shadow_sys.get ());
  push (lighting_sys.get ());
  push (skybox_sys.get ());
  push (render_3d_sys.get ());
  push (render_ui_sys.get ());

  return out;
}

void
core_systems::ensure_dummy_context_bindings ()
{
  auto &ctx = m_dummy_registry.ctx ();

  if (m_runtime_ctx != nullptr) {
    if (ctx.contains<comp::singl::runtime_context *> ()) {
      ctx.erase<comp::singl::runtime_context *> ();
    }
    ctx.emplace<comp::singl::runtime_context *> (m_runtime_ctx);
  }

  if (m_editor_ctx != nullptr) {
    if (ctx.contains<comp::singl::editor_context *> ()) {
      ctx.erase<comp::singl::editor_context *> ();
    }
    ctx.emplace<comp::singl::editor_context *> (m_editor_ctx);
  }

  if (m_runtime_ctx != nullptr) {
    if (ctx.contains<rsc::scene_manager *> ()) {
      ctx.erase<rsc::scene_manager *> ();
    }
    ctx.emplace<rsc::scene_manager *> (&m_runtime_ctx->scene_manager);

    if (ctx.contains<rsc::resource_manager_view *> ()) {
      ctx.erase<rsc::resource_manager_view *> ();
    }
    ctx.emplace<rsc::resource_manager_view *> (
        &m_runtime_ctx->resource_manager_view);

    if (ctx.contains<comp::singl::ui_manager *> ()) {
      ctx.erase<comp::singl::ui_manager *> ();
    }
    ctx.emplace<comp::singl::ui_manager *> (&m_runtime_ctx->ui_manager);

    m_runtime_ctx->singleton_registry.apply_core_singletons (m_dummy_registry);
  }
}

void
core_systems::render_impl (wsl::gfx::render_window &window,
                           const render_callbacks &callbacks)
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  sync_activation ();

  rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();
  entt::registry &registry
      = (scene != nullptr) ? scene->get_registry () : m_dummy_registry;

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->render_build_draw_data (&registry);
  }

  if (callbacks.build_draw_data) {
    callbacks.build_draw_data (registry);
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      sys->render_build_draw_data (&registry);
    }
  }

  m_runtime_ctx->render_ctx.begin_cmd ();
  if (!m_runtime_ctx->render_ctx.has_active_frame ()) {
    return;
  }

  window.new_swapchain ();

  // Pre-compute set of core system type_ids so that scene-system loops can
  // skip duplicate entries (they have uninitialised render state).
  entt::id_type core_ids[]{};
  size_t n_core = 0;
  for (sys::ecs_system *sys : to_vec ()) {
    if (sys != nullptr) {
      core_ids[n_core++] = sys->get_type_id ();
    }
  }
  std::sort (core_ids, core_ids + n_core);

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->render_prepare_gpu_rsc (&registry);
  }

  if (callbacks.prepare_gpu_rsc) {
    callbacks.prepare_gpu_rsc (registry);
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      if (sys == nullptr) {
        continue;
      }
      if (std::binary_search (core_ids, core_ids + n_core,
                              sys->get_type_id ())) {
        continue;
      }
      sys->render_prepare_gpu_rsc (&registry);
    }
  }

  gfx::scene_renderer *renderer = nullptr;
  sys::render_submission submission{};

  if ((scene != nullptr)
      && sys::build_render_frame (registry, *m_runtime_ctx, submission)) {
    apply_rendering_manager (registry, *m_runtime_ctx);

    renderer = &m_runtime_ctx->get_active_scene_renderer ();
    renderer->begin_frame (submission.view);
    renderer->set_visible_draws (std::move (submission.draw_commands));
    renderer->set_environment (submission.environment);

    wsl::log::sys ()->trace ("Render frame: {} visible draws, scene='{}'",
                             submission.draw_commands.size (),
                             scene->get_name ());

    // These systems open their own offscreen passes, so they must run before
    // the main scene render pass is active.
    if (shadow_sys) {
      shadow_sys->render_record_draw_cmd (&registry);
    }
    if (lighting_sys) {
      lighting_sys->render_record_draw_cmd (&registry);
    }

    window.begin_3d_pass ();

    if (skybox_sys) {
      skybox_sys->render_record_draw_cmd (&registry);
    }
    if (render_3d_sys) {
      render_3d_sys->render_record_draw_cmd (&registry);
    }
    if (physics_sys) {
      physics_sys->render_record_draw_cmd (&registry);
    }

    if (scene != nullptr) {
      for (auto &sys : scene->systems) {
        if (sys == nullptr) {
          continue;
        }
        if (std::binary_search (core_ids, core_ids + n_core,
                                sys->get_type_id ())) {
          continue;
        }
        sys->render_record_draw_cmd (&registry);
      }
    }

    window.end_3d_pass ();
    renderer->end_frame ();
  }

  if (render_ui_sys) {
    render_ui_sys->render_record_draw_cmd (&registry);
  }

  window.begin_ui_pass ();
  if (callbacks.record_ui_draw_cmd) {
    callbacks.record_ui_draw_cmd (registry);
  }
  window.end_ui_pass ();

  m_runtime_ctx->render_ctx.end_cmd ();
}

} // namespace sys

} // namespace wsl
