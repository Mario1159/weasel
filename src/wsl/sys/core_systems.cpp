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
#include <SDL3/SDL_events.h>
#include <algorithm>
#include <entt/entity/fwd.hpp>

#include <tracy/Tracy.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef WEASEL_BUILD_EDITOR
#include "wsl/comp/singl/editor_context.hpp"
#endif
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/camera.hpp"
#include "../comp/camera_2d.hpp"
#include "../comp/hierarchy.hpp"
#include "../comp/subviewport.hpp"
#include "../comp/transform_2d.hpp"
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
core_systems::register_factory_types (comp::singl::runtime_context &rtc)
{
  auto &factory = rtc.system_factory_registry;
  factory.register_system_type<transform_system> ({ "Transform" });
  factory.register_system_type<physics_system> ({ "Physics" });
  factory.register_system_type<render_3d_system> ({ "3D Render" });
  factory.register_system_type<render_2d_system> ({ "2D Render" });
  factory.register_system_type<audio_system> ({ "Audio" });
  factory.register_system_type<lighting_system> ({ "Lighting" });
  factory.register_system_type<skybox_system> ({ "Skybox" });
  factory.register_system_type<shadow_system> ({ "Shadow" });
  factory.register_system_type<render_ui_system> ({ "UI" });
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
  if (!render_2d_sys) {
    render_2d_sys = std::make_unique<render_2d_system> ("2D Render System");
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

  if (m_runtime_ctx) {
    register_factory_types (*m_runtime_ctx);
  }

  register_debug_metadata ();

  // Build the cached system list + sorted id set now that all unique_ptr
  // members have been constructed. This is the only place the cache needs
  // to be built at runtime — the system membership is fixed after init.
  rebuild_system_cache ();

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

  for (sys::ecs_system *sys : to_vec ()) {
    if (sys == nullptr) {
      continue;
    }

    sys->editor_update (&registry, dt);
  }

  if (scene != nullptr) {
    for (auto &sys : scene->systems) {
      sys->editor_update (&registry, dt);
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

void
core_systems::rebuild_system_cache ()
{
  m_cached_systems.clear ();
  m_cached_systems.reserve (10);

  const auto push = [this] (sys::ecs_system *sys) {
    if (sys) {
      m_cached_systems.push_back (sys);
    }
  };

  push (audio_sys.get ());
  push (physics_sys.get ());
  push (transform_sys.get ());
  push (shadow_sys.get ());
  push (lighting_sys.get ());
  push (skybox_sys.get ());
  push (render_3d_sys.get ());
  push (render_2d_sys.get ());
  push (render_ui_sys.get ());

  m_cached_core_ids.clear ();
  m_cached_core_ids.reserve (m_cached_systems.size ());
  for (sys::ecs_system *sys : m_cached_systems) {
    m_cached_core_ids.push_back (sys->get_type_id ());
  }
  std::sort (m_cached_core_ids.begin (), m_cached_core_ids.end ());
}

const std::vector<sys::ecs_system *> &
core_systems::to_vec () const
{
  return m_cached_systems;
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
  ZoneScoped;
  if (m_runtime_ctx == nullptr) {
    return;
  }

  rsc::scene *scene = nullptr;
  entt::registry *registry_ptr = nullptr;
  {
    ZoneScopedN ("render_impl::setup");
    sync_activation ();

    scene = m_runtime_ctx->scene_manager.get_active ();
    registry_ptr
        = (scene != nullptr) ? &scene->get_registry () : &m_dummy_registry;
  }
  entt::registry &registry = *registry_ptr;

  {
    ZoneScopedN ("render_impl::build_draw_data");
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
  }

  {
    ZoneScopedN ("render_impl::begin_cmd");
    m_runtime_ctx->render_ctx.begin_cmd ();
  }
  if (!m_runtime_ctx->render_ctx.has_active_frame ()) {
    return;
  }

  {
    ZoneScopedN ("render_impl::acquire_swapchain");
    window.new_swapchain ();
  }

  // `core_ids` is the sorted set of type_ids for the built-in core
  // systems. It is built once in `init()` (see `rebuild_system_cache`) and
  // is used below to skip scene-system duplicates that would otherwise
  // double-render. The previous version of this code rebuilt and sorted
  // `core_ids` every frame, which became a measurable hot spot.
  const std::vector<entt::id_type> &core_ids = m_cached_core_ids;

  {
    ZoneScopedN ("render_impl::prepare_gpu_rsc");
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
        if (std::binary_search (core_ids.begin (), core_ids.end (),
                                sys->get_type_id ())) {
          continue;
        }
        sys->render_prepare_gpu_rsc (&registry);
      }
    }
  }

  gfx::scene_renderer *renderer = nullptr;
  sys::render_submission submission{};
  entt::entity main_viewport = entt::null;

  if (scene != nullptr) {
    {
      ZoneScopedN ("render_impl::apply_rendering_manager");
      apply_rendering_manager (registry, *m_runtime_ctx);
    }
    {
      ZoneScopedN ("render_impl::resolve_scene");
      renderer = &m_runtime_ctx->get_active_scene_renderer ();

      auto *rendering = m_runtime_ctx->get_active_rendering_manager ();
      main_viewport
          = (rendering != nullptr) ? rendering->render_viewport : entt::null;

      // Defensive: if render_viewport points to a non-subviewport entity
      // (e.g. stale code writing to the old main_camera field), treat as
      // root.
      if (main_viewport != entt::null
          && !registry.all_of<comp::subviewport> (main_viewport)) {
        main_viewport = entt::null;
      }

#ifdef WEASEL_BUILD_EDITOR
      if (m_editor_ctx != nullptr) {
        if (m_editor_ctx->game_view_selected_viewport != entt::null
            && !m_runtime_ctx->is_running) {
          main_viewport = m_editor_ctx->game_view_selected_viewport;
        }
      }
#endif
    }

    {
      ZoneScopedN ("render_impl::build_render_frame");
      if (sys::build_render_frame (registry, *m_runtime_ctx, submission,
                                   main_viewport)) {

        {
          ZoneScopedN ("render_impl::set_environment");
          renderer->set_environment (submission.environment);
        }

        // Shadow and lighting passes (global)
        {
          ZoneScopedN ("render_impl::global_shadow_and_light");
          renderer->begin_frame (submission.view);
          if (shadow_sys) {
            shadow_sys->render_record_draw_cmd (&registry);
          }
          if (lighting_sys) {
            lighting_sys->render_record_draw_cmd (&registry);
          }
        }

        // Helper to render a viewport (3D + 2D)
        auto render_viewport_full = [&] (entt::entity vp_entity,
                                         const sys::render_submission &sub) {
          ZoneScopedN ("render_viewport_full");

          // Set context for systems that need to know which viewport they
          // are in (e.g. render_2d)
          registry.ctx ().insert_or_assign (vp_entity);

          {
            ZoneScopedN ("render_viewport_full::begin_3d_pass");
            window.begin_3d_pass (true,
                                  true); // Use clearing from viewport settings?
          }

          {
            ZoneScopedN ("render_viewport_full::apply_viewport");
            if (vp_entity != entt::null) {
              if (auto *sv = registry.try_get<comp::subviewport> (vp_entity)) {
                gfx::viewport vp{};
                vp.x = sv->x;
                vp.y = sv->y;
                vp.width = sv->width;
                vp.height = sv->height;
                vp.clear_color = sv->clear_color;
                vp.clear_depth = sv->clear_depth;
                vp.clear_color_value.r = sv->clear_r;
                vp.clear_color_value.g = sv->clear_g;
                vp.clear_color_value.b = sv->clear_b;
                vp.clear_color_value.a = sv->clear_a;
                window.apply_viewport (vp);
              }
            }
          }

          {
            ZoneScopedN ("render_viewport_full::begin_frame");
            // Re-bind draws for this viewport
            renderer->set_visible_draws (sub.draw_commands);
            renderer->begin_frame (sub.view);
          }

          if (!sub.is_2d_view) {
            if (skybox_sys) {
              ZoneScopedN ("render_viewport_full::skybox");
              skybox_sys->render_record_draw_cmd (&registry);
            }
            if (render_3d_sys) {
              ZoneScopedN ("render_viewport_full::render_3d");
              render_3d_sys->render_record_draw_cmd (&registry);
            }
            if (physics_sys) {
              ZoneScopedN ("render_viewport_full::physics");
              physics_sys->render_record_draw_cmd (&registry);
            }
          }

          if (scene != nullptr) {
            ZoneScopedN ("render_viewport_full::scene_systems");
            for (auto &sys : scene->systems) {
              if (sys == nullptr)
                continue;
              if (std::binary_search (core_ids.begin (), core_ids.end (),
                                      sys->get_type_id ()))
                continue;
              sys->render_record_draw_cmd (&registry);
            }
          }
          {
            ZoneScopedN ("render_viewport_full::end_3d_pass");
            window.end_3d_pass ();
          }

          // 2D Pass
          if (render_2d_sys) {
            ZoneScopedN ("render_viewport_full::render_2d");
            auto &r2d_ref
                = m_runtime_ctx->get_active_rendering_manager ()
                      ->ensure_renderer_2d (window, m_runtime_ctx->render_ctx,
                                            &m_runtime_ctx->resource_manager);
            auto *r2d = &r2d_ref;

            {
              ZoneScopedN ("render_viewport_full::2d_setup");
              // Set 2D projection from the already-resolved camera matrix
              r2d->set_projection (sub.view.proj);

              // Build sprite queue (no GPU work yet)
              render_2d_sys->render_record_draw_cmd (&registry);

              // Upload vertices OUTSIDE the render pass (copy pass is
              // illegal inside a render pass)
              r2d->build_and_upload ();
            }

            {
              ZoneScopedN ("render_viewport_full::2d_begin_pass");
              window.begin_3d_pass (false, false);
            }

            {
              ZoneScopedN ("render_viewport_full::2d_apply_viewport");
              // Apply viewport
              if (vp_entity != entt::null) {
                if (auto *sv
                    = registry.try_get<comp::subviewport> (vp_entity)) {
                  gfx::viewport vp{};
                  vp.x = sv->x;
                  vp.y = sv->y;
                  vp.width = sv->width;
                  vp.height = sv->height;
                  window.apply_viewport (vp);
                }
              } else {
                window.apply_viewport ({}); // Fullscreen
              }
            }

            {
              ZoneScopedN ("render_viewport_full::2d_draw");
              // Draw already-uploaded batches INSIDE the render pass
              r2d->draw ();
            }
            {
              ZoneScopedN ("render_viewport_full::2d_end_pass");
              window.end_3d_pass ();
            }
          }
        };

        // 1. Render main viewport
        {
          ZoneScopedN ("render_impl::main_viewport");
          render_viewport_full (main_viewport, submission);
        }

        // 2. Render child viewports
        {
          ZoneScopedN ("render_impl::child_viewports");
          auto sv_view = registry.view<comp::subviewport> ();
          for (auto const e : sv_view) {
            if (comp::find_nearest_viewport (registry, e) == main_viewport) {
              sys::render_submission child_sub{};
              {
                ZoneScopedN ("render_impl::child_viewport_build");
                if (sys::build_render_frame (registry, *m_runtime_ctx,
                                             child_sub, e)) {
                  render_viewport_full (e, child_sub);
                }
              }
            }
          }
        }

        {
          ZoneScopedN ("render_impl::renderer_end_frame");
          renderer->end_frame ();
        }
        registry.ctx ().erase<entt::entity> (); // clear viewport context
      }
    }
  }

  {
    ZoneScopedN ("render_impl::ui_pass");
    if (render_ui_sys) {
      render_ui_sys->render_record_draw_cmd (&registry);
    }

    window.begin_ui_pass ();
    if (callbacks.record_ui_draw_cmd) {
      callbacks.record_ui_draw_cmd (registry);
    }
    window.end_ui_pass ();
  }

  {
    ZoneScopedN ("render_impl::end_cmd");
    m_runtime_ctx->render_ctx.end_cmd ();
  }
}

} // namespace sys

} // namespace wsl
