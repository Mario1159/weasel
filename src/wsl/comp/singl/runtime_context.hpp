#pragma once

// Phase 2 (C++20 modules): the subsystems aggregated by runtime_context live in
// other namespaces (rsc/reg/event/gfx/phys/input/sys). To keep this header from
// textually including those namespaces (which would make the header graph
// cyclic and block `export import`), their types are only *forward-declared*
// here; the real definitions are pulled in by runtime_context.cpp. The members
// are owning `unique_ptr`s so an incomplete type is sufficient in this header.

#include "rendering_manager.hpp"
#include "ui_manager.hpp"
#include "physics_manager.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace wsl
{

// Forward declarations of the aggregated subsystem types (defined in their own
// namespaces / headers, included by runtime_context.cpp).
namespace rsc
{
class world;
class scene_manager;
class resource_manager;
class resource_manager_view;
class scene;
struct scene_id;
} // namespace rsc

namespace reg
{
class component_registry;
class singleton_registry;
class system_factory_registry;
class registry_queries;
namespace runtime
{
class runtime_project_module;
}
} // namespace reg

#if !defined(WSL_MODULE_BUILD)
namespace event
{
class event_debug_db;
class event_hub;
class message_bus;
} // namespace event
#endif
namespace event
{
struct scene_changed;
} // namespace event (core-owned)

namespace gfx
{
class render_context;
class render_window;
class scene_renderer;
} // namespace gfx

#if !defined(WSL_MODULE_BUILD)
namespace phys
{
class engine;
}
#endif

namespace sys
{
class core_systems;
class ecs_system;
} // namespace sys

namespace input
{
class action_map;
}

namespace comp::singl
{

/** Tunable parallel-systems execution policy. */
struct system_parallelism_settings
{
  bool parallel_systems_enabled = true;
  bool parallel_render_build_enabled = true;
  std::size_t max_system_worker_threads = 0; // 0 = hardware concurrency
  bool stage_fences_enabled = false;
  bool strict_system_ordering = false;
  bool warn_undeclared_cross_tier = true;
  bool deterministic_system_order = false;
};

/**
 * Core shared state for a Weasel runtime instance.
 *
 * This singleton aggregates all major subsystems (resource manager, scene
 * manager, world, dispatcher) and provides a central point of access for
 * runtime logic.
 */
class runtime_context : public comp::singleton_component
{
public:
  /**
   * Constructs the runtime context.
   * :param name: Window title.
   * :param width: Window width.
   * :param height: Window height.
   * :param engine_res_path: Base path for engine resources.
   */
  explicit runtime_context (const char *name, int width, int height,
                            const std::string &engine_res_path,
                            bool headless = false);

  ~runtime_context ();

  /** Register reflection metadata for this class. */
  static void register_meta ();

  /** Mutable parallel-systems execution policy. */
  system_parallelism_settings &
  parallel_settings ()
  {
    return m_parallel_settings;
  }

  const system_parallelism_settings &
  parallel_settings () const
  {
    return m_parallel_settings;
  }

  /** Returns the active rendering manager from the current scene. */
  rendering_manager *get_active_rendering_manager () const;

  /** Attempts to get the active scene renderer, may return nullptr. */
  gfx::scene_renderer *try_get_active_scene_renderer ();

  /** Gets the active scene renderer, ensuring it exists. */
  gfx::scene_renderer &get_active_scene_renderer ();

  /** Returns the active physics manager from the current scene. */
  physics_manager *get_active_physics_manager () const;

  /** Attempts to get the active physics engine, may return nullptr. */
  phys::engine *try_get_active_physics_engine ();

  /** Gets the active physics engine, ensuring it exists. */
  phys::engine &get_active_physics_engine ();

  /** Sets the simulation running state. */
  void set_running (bool value);

  /** Stops the current play session and restores scene states. */
  void stop ();

  /** Synchronizes deferred state changes. */
  void sync ();

  /** Saves the state of a specific scene for later restoration. */
  void save_scene_state (rsc::scene *scene);

  /** Saves the state of the currently active scene. */
  void save_active_scene_state ();

  /** Callback for scene change events. */
  void on_scene_changed (const wsl::event::scene_changed &event);

  /** Assigns an editor context for tool-specific behaviors. */
  void set_editor_ctx (class editor_context *editor_ctx);

  /** Returns the application input map. */
  wsl::input::action_map &
  get_app_input_map ()
  {
    return *m_app_input_map;
  }

  /** Returns the application input map. */
  const wsl::input::action_map &
  get_app_input_map () const
  {
    return *m_app_input_map;
  }

  /** Returns current input map (may be null). */
  wsl::input::action_map *
  get_current_input_map () const
  {
    return m_current_input_map;
  }

  /** Returns whether this context was created in headless mode. */
  bool
  is_headless () const
  {
    return m_headless;
  }

  rsc::world const &
  world () const
  {
    return *m_world;
  }
  rsc::world &
  world ()
  {
    return *m_world;
  }
  rsc::scene_manager const &
  scene_manager () const
  {
    return *m_scene_manager;
  }
  rsc::scene_manager &
  scene_manager ()
  {
    return *m_scene_manager;
  }
  reg::component_registry const &
  component_registry () const
  {
    return *m_component_registry;
  }
  reg::component_registry &
  component_registry ()
  {
    return *m_component_registry;
  }
  reg::singleton_registry const &
  singleton_registry () const
  {
    return *m_singleton_registry;
  }
  reg::singleton_registry &
  singleton_registry ()
  {
    return *m_singleton_registry;
  }
  reg::system_factory_registry const &
  system_factory_registry () const
  {
    return *m_system_factory_registry;
  }
  reg::system_factory_registry &
  system_factory_registry ()
  {
    return *m_system_factory_registry;
  }
  event::event_debug_db const &
  event_db () const
  {
    return *m_event_db;
  }
  event::event_debug_db &
  event_db ()
  {
    return *m_event_db;
  }
  event::event_hub const &
  event_hub () const
  {
    return *m_event_hub;
  }
  event::event_hub &
  event_hub ()
  {
    return *m_event_hub;
  }
  event::message_bus const &
  message_bus () const
  {
    return *m_message_bus;
  }
  event::message_bus &
  message_bus ()
  {
    return *m_message_bus;
  }
  reg::registry_queries const &
  reg_queries () const
  {
    return *m_reg_queries;
  }
  reg::registry_queries &
  reg_queries ()
  {
    return *m_reg_queries;
  }
  reg::runtime::runtime_project_module const &
  runtime_project_module () const
  {
    return *m_runtime_project_module;
  }
  reg::runtime::runtime_project_module &
  runtime_project_module ()
  {
    return *m_runtime_project_module;
  }

  gfx::render_context const &
  render_ctx () const
  {
    return *m_render_ctx;
  }
  gfx::render_context &
  render_ctx ()
  {
    return *m_render_ctx;
  }
  rsc::resource_manager const &
  resource_manager () const
  {
    return *m_resource_manager;
  }
  rsc::resource_manager &
  resource_manager ()
  {
    return *m_resource_manager;
  }
  rsc::resource_manager_view const &
  resource_manager_view () const
  {
    return *m_resource_manager_view;
  }
  rsc::resource_manager_view &
  resource_manager_view ()
  {
    return *m_resource_manager_view;
  }
  gfx::render_window const &
  window () const
  {
    return *m_window;
  }
  gfx::render_window &
  window ()
  {
    return *m_window;
  }
  comp::singl::ui_manager const &
  ui_manager () const
  {
    return m_ui_manager;
  }
  comp::singl::ui_manager &
  ui_manager ()
  {
    return m_ui_manager;
  }

  std::unique_ptr<sys::core_systems> const &
  core_systems () const
  {
    return m_core_systems;
  }
  std::unique_ptr<sys::core_systems> &
  core_systems ()
  {
    return m_core_systems;
  }

  bool
  is_running () const
  {
    return m_is_running;
  }
  bool
  in_play_session () const
  {
    return m_in_play_session;
  }
  void
  set_in_play_session (bool v)
  {
    m_in_play_session = v;
  }

  class editor_context *
  editor_ctx () const
  {
    return m_editor_ctx;
  }

  std::unordered_map<entt::id_type, std::string> const &
  scene_save_states () const
  {
    return m_scene_save_states;
  }
  std::unordered_map<entt::id_type, std::string> &
  scene_save_states ()
  {
    return m_scene_save_states;
  }

private:
  struct sdl_init_guard
  {
    sdl_init_guard (bool headless = false);
    ~sdl_init_guard ();

    bool is_headless = false;
  };
  sdl_init_guard sdl_init_guard_;

  std::unique_ptr<rsc::world> m_world;
  std::unique_ptr<rsc::scene_manager> m_scene_manager;
  std::unique_ptr<reg::component_registry> m_component_registry;
  std::unique_ptr<reg::singleton_registry> m_singleton_registry;
  std::unique_ptr<reg::system_factory_registry> m_system_factory_registry;
  std::unique_ptr<event::event_debug_db> m_event_db;
  std::unique_ptr<event::event_hub> m_event_hub;
  std::unique_ptr<event::message_bus> m_message_bus;
  std::unique_ptr<reg::registry_queries> m_reg_queries;
  std::unique_ptr<reg::runtime::runtime_project_module>
      m_runtime_project_module;

  std::unique_ptr<gfx::render_context> m_render_ctx;
  std::unique_ptr<rsc::resource_manager> m_resource_manager;
  std::unique_ptr<rsc::resource_manager_view> m_resource_manager_view;
  std::unique_ptr<gfx::render_window> m_window;
  comp::singl::ui_manager m_ui_manager;
  bool m_headless = false;
  std::unique_ptr<sys::core_systems> m_core_systems;
  bool m_is_running = false;
  bool m_in_play_session = false;
  class editor_context *m_editor_ctx = nullptr;
  std::unordered_map<entt::id_type, std::string> m_scene_save_states;

  std::unique_ptr<wsl::input::action_map> m_app_input_map;
  wsl::input::action_map *m_current_input_map = nullptr;

  rsc::scene *m_play_session_origin_scene = nullptr;
  std::unique_ptr<rsc::scene_id> m_play_session_origin_scene_id;

  bool m_needs_save_active_scene = false;

  system_parallelism_settings m_parallel_settings;
};

} // namespace comp::singl

} // namespace wsl
