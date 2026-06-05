#include "runtime_context.hpp"

#include "../../rsc/resource_manager.hpp"
#include "../../rsc/scene.hpp"
#include "../../rsc/scene_snapshot_serializer.hpp"
#include "../camera.hpp"
#include "comp/component_meta.hpp"
#include "comp/singl/physics_manager.hpp"
#include "comp/singl/rendering_manager.hpp"
#include "editor_context.hpp"
#include "events.hpp"
#include "gfx/scene_renderer.hpp"
#include "phys/physics_engine.hpp"
#include "rsc/resource_ids.hpp"
#include "sys/system.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <cassert>
#include <entt/core/fwd.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <imgui.h>
#include <memory>
#include <utility>

namespace wsl
{

namespace
{

rsc::scene_id
find_scene_id_for_instance (comp::singl::runtime_context &runtime_ctx,
                            rsc::scene *scene)
{
  if (scene == nullptr) {
    return rsc::scene_id{ entt::null };
  }

  for (const auto &scene_info : runtime_ctx.resource_manager.list_scenes ()) {
    const rsc::scene_id candidate{ scene_info.id };
    if (runtime_ctx.resource_manager.find_loaded_scene (candidate) == scene) {
      return candidate;
    }
  }

  return rsc::scene_id{ entt::null };
}

bool
scene_belongs_to_world (const comp::singl::runtime_context &runtime_ctx,
                        const rsc::scene *scene)
{
  if (scene == nullptr) {
    return false;
  }

  for (const auto &candidate : runtime_ctx.world.get_scenes ()) {
    if (candidate.get () == scene) {
      return true;
    }
  }

  return false;
}

} // namespace

void
comp::singl::runtime_context::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::singl::runtime_context> ()
      .type (entt::type_hash<comp::singl::runtime_context>::value ())
      .custom<comp::meta_info> (comp::meta_info{
          "Runtime Context", "Global state for the running game application.",
          "" })
      .func<&comp::singl::runtime_context::custom_inspect> (
          "custom_inspect"_hs);
}

bool
comp::singl::runtime_context::custom_inspect (
    const char *label, comp::singl::runtime_context *runtime_ctx_ptr)
{
  (void)label;
  comp::singl::runtime_context &ctx
      = (runtime_ctx_ptr != nullptr) ? *runtime_ctx_ptr : *this;

  rsc::scene *scene = ctx.scene_manager.get_active ();
  if (scene == nullptr) {
    ImGui::TextDisabled ("No active scene.");
    return false;
  }

  bool changed = false;
  entt::registry &registry = scene->get_registry ();

  // If scene->camera changes, we sync here if they were different,
  // but let's just use scene->camera as the source of truth for now
  // or add game_camera as its own thing.
  // The user asked for "game camera" UI field in runtime context.

  entt::entity const current_cam = ctx.game_camera;
  const char *preview = "None";
  if (current_cam != entt::null) {
    preview = scene->get_entity_name (current_cam).c_str ();
  }

  if (ImGui::BeginCombo ("Game Camera", preview)) {
    if (ImGui::Selectable ("None", current_cam == entt::null)) {
      ctx.game_camera = entt::null;
      scene->camera = entt::null;
      changed = true;
    }

    auto view = registry.view<comp::camera> ();
    for (entt::entity const e : view) {
      comp::camera const &cam = view.get<comp::camera> (e);
      if (cam.only_for_editor) {
        continue;
      }

      const std::string &name = scene->get_entity_name (e);
      bool const selected = (e == current_cam);

      if (ImGui::Selectable (name.c_str (), selected)) {
        ctx.game_camera = e;
        scene->camera = e; // Sync with scene camera
        changed = true;
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }

  // Also sync from scene back to context if they diverge
  if (scene->camera != ctx.game_camera) {
    ctx.game_camera = scene->camera;
  }

  ImGui::Separator ();
  ImGui::Value ("Is Running", ctx.is_running);
  ImGui::Value ("In Play Session", ctx.in_play_session);
  ImGui::Value ("Saved Scene Count", (int)ctx.scene_save_states.size ());

  return changed;
}

comp::singl::runtime_context::sdl_init_guard::sdl_init_guard (bool headless)
    : is_headless (headless)
{
  if (headless) {
    wsl::log::core ()->trace ("Headless mode, skipping SDL initialization");
    return;
  }

  if (!SDL_Init (SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
    wsl::log::core ()->critical ("Failed to initialize SDL: {}",
                                 SDL_GetError ());
  } else {
    wsl::log::core ()->trace ("SDL initialized successfully");
  }
}

comp::singl::runtime_context::sdl_init_guard::~sdl_init_guard ()
{
  if (!is_headless)
    SDL_Quit ();
}

comp::singl::runtime_context::runtime_context (
    const char *name, int width, int height, const std::string &engine_res_path,
    bool headless)
    : world (this), scene_manager (world), signal_hub (dispatcher, signal_db),
      reg_queries (component_registry, system_factory_registry, signal_hub),
      runtime_project_module (this), sdl_init_guard_ (headless),
      render_ctx (headless), resource_manager (this, engine_res_path),
      resource_manager_view (&resource_manager),
      window (name, width, height, &render_ctx, &resource_manager, headless),
      ui_manager (render_ctx, window, &resource_manager), m_headless (headless)
{
  system_factory_registry.set_signal_hub (&signal_hub);
  if (!headless)
    wsl::log::core ()->trace ("GPU device status: {}",
                              (void *)render_ctx.gpu_device);
  current_input_map = &app_input_map;

  signal_hub.resolve_active_registry = [this] () -> entt::registry * {
    auto *scene = scene_manager.get_active ();
    return scene ? &scene->get_registry () : nullptr;
  };

  signal_hub.resolve_system_by_type
      = [this] (entt::id_type system_type_id) -> sys::ecs_system * {
    if (auto *scene = scene_manager.get_active ()) {
      for (sys::ecs_system *system : scene->get_systems ()) {
        if (system && system->get_type_id () == system_type_id) {
          return system;
        }
      }
    }

    if (core_systems) {
      for (sys::ecs_system *system : core_systems->to_vec ()) {
        if (system && system->get_type_id () == system_type_id) {
          return system;
        }
      }
    }

    return nullptr;
  };

  dispatcher.sink<wsl::event::scene_changed> ()
      .connect<&runtime_context::on_scene_changed> (this);

  // Register core system factories so CLI can discover them via `sys avail`,
  // even in headless mode.  The actual system instances are only created
  // when a full rendering context is available.
  sys::core_systems::register_factory_types (*this);

  if (!headless) {
    core_systems = std::make_unique<sys::core_systems> ();
    core_systems->init (this, nullptr);
  }

  wsl::log::core ()->debug (
      "Runtime context initialized (window={}x{}, engine_res_path='{}')", width,
      height, engine_res_path);
}

comp::singl::runtime_context::~runtime_context ()
{
  wsl::log::core ()->debug ("Shutting down runtime context");
  // The main resource manager depends on the runtime world, core systems, and
  // GPU device still being alive. Shut it down explicitly before member
  // destruction starts.
  resource_manager.shutdown ();
}

void
comp::singl::runtime_context::set_editor_ctx (
    comp::singl::editor_context *editor_ctx)
{
  this->editor_ctx = editor_ctx;
  if (editor_ctx != nullptr) {
    current_input_map = &editor_ctx->editor_input_map;
  }
  world.set_editor_context (editor_ctx);
  resource_manager.set_editor_context (editor_ctx);
}

void
comp::singl::runtime_context::save_scene_state (rsc::scene *scene)
{
  if (!in_play_session || (scene == nullptr)) {
    return;
  }

  const rsc::scene_id sid = find_scene_id_for_instance (*this, scene);
  if (sid.value == entt::null || scene_save_states.contains (sid.value)) {
    return;
  }

  rsc::io::scene_snapshot_serializer const serializer (this, *scene);
  std::string snapshot;
  serializer.save_to_binary_string (snapshot);
  scene_save_states[sid.value] = std::move (snapshot);
}

void
comp::singl::runtime_context::save_active_scene_state ()
{
  save_scene_state (scene_manager.get_active ());
}

void
comp::singl::runtime_context::sync ()
{
  if (m_needs_save_active_scene) {
    save_active_scene_state ();
    m_needs_save_active_scene = false;
  }
}

void
comp::singl::runtime_context::on_scene_changed (
    const wsl::event::scene_changed &event)
{
  if (!in_play_session) {
    return;
  }

  // Save both sides of the transition once so stop() can restore every scene
  // touched during the current play session.
  save_scene_state (event.old_scene);
  save_scene_state (event.new_scene);
  m_needs_save_active_scene = false;
}

void
comp::singl::runtime_context::set_running (bool value)
{
  if (is_running == value) {
    return;
  }

  // If starting play for the first time in a session, save state
  if (value && !in_play_session) {
    in_play_session = true;
    save_active_scene_state ();
    if (auto *scene = scene_manager.get_active ()) {
      play_session_origin_scene = scene;
      play_session_origin_scene_id = find_scene_id_for_instance (*this, scene);
    }
  }

  is_running = value;

  wsl::log::core ()->debug ("Runtime {}",
                            value ? "started (play)" : "stopped (pause)");

  if (auto *scene = scene_manager.get_active ()) {
    if (is_running) {
      scene->resume ();
    } else {
      scene->pause ();
    }
  }

  if (core_systems) {
    core_systems->sync_activation ();
  }
}

void
comp::singl::runtime_context::stop ()
{
  if (!in_play_session) {
    return;
  }

  // Pause everything first
  set_running (false);
  in_play_session = false;

  // Ensure GPU is idle before we start destroying renderers and restoring
  // states. This prevents VRAM exhaustion from deferred releases during rapid
  // play/stop cycles.
  if (render_ctx.gpu_device != nullptr) {
    SDL_WaitForGPUIdle (render_ctx.gpu_device);
  }

  // Restore ALL scenes state
  for (auto &[sid_val, snapshot] : scene_save_states) {
    const rsc::scene_id sid{ sid_val };
    rsc::scene *scene = resource_manager.find_loaded_scene (sid);
    if (scene != nullptr) {
      rsc::io::scene_snapshot_serializer serializer (this, *scene);
      serializer.load_from_binary_string (snapshot);
    }
  }

  // Then restore the original active scene
  bool restored_origin_scene = false;
  if (scene_belongs_to_world (*this, play_session_origin_scene)) {
    scene_manager.set_active (play_session_origin_scene);
    restored_origin_scene = true;
  }

  if (!restored_origin_scene
      && play_session_origin_scene_id.value != entt::null) {
    if (resource_manager.activate_scene (play_session_origin_scene_id)) {
      restored_origin_scene = true;
    } else if (rsc::scene *loaded_scene = resource_manager.find_loaded_scene (
                   play_session_origin_scene_id)) {
      scene_manager.set_active (loaded_scene);
      restored_origin_scene = true;
    }
  }

  scene_save_states.clear ();
  play_session_origin_scene_id = rsc::scene_id{ entt::null };
  play_session_origin_scene = nullptr;

  wsl::log::core ()->debug ("Play session stopped");
}

comp::singl::rendering_manager *
comp::singl::runtime_context::get_active_rendering_manager () const
{
  auto *scene = scene_manager.get_active ();
  if (scene == nullptr) {
    return nullptr;
  }

  auto &ctx = scene->get_registry ().ctx ();
  if (!ctx.contains<comp::singl::rendering_manager> ()) {
    return nullptr;
  }

  return &ctx.get<comp::singl::rendering_manager> ();
}

gfx::scene_renderer *
comp::singl::runtime_context::try_get_active_scene_renderer ()
{
  if (auto *rendering = get_active_rendering_manager ()) {
    return rendering->try_renderer ();
  }

  return nullptr;
}

gfx::scene_renderer &
comp::singl::runtime_context::get_active_scene_renderer ()
{
  comp::singl::rendering_manager *rendering = get_active_rendering_manager ();
  assert (rendering && "Active scene is missing its rendering manager.");
  return rendering->ensure_renderer (window, render_ctx, &resource_manager);
}

comp::singl::physics_manager *
comp::singl::runtime_context::get_active_physics_manager () const
{
  auto *scene = scene_manager.get_active ();
  if (scene == nullptr) {
    return nullptr;
  }

  auto &ctx = scene->get_registry ().ctx ();
  if (!ctx.contains<comp::singl::physics_manager> ()) {
    return nullptr;
  }

  return &ctx.get<comp::singl::physics_manager> ();
}

phys::engine *
comp::singl::runtime_context::try_get_active_physics_engine ()
{
  if (auto *physics = get_active_physics_manager ()) {
    return physics->try_engine ();
  }

  return nullptr;
}

phys::engine &
comp::singl::runtime_context::get_active_physics_engine ()
{
  comp::singl::physics_manager *physics = get_active_physics_manager ();
  assert (physics && "Active scene is missing its physics manager.");
  return physics->ensure_engine ();
}

} // namespace wsl
