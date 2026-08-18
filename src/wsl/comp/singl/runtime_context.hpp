#pragma once

#include "../../rsc/resource_manager.hpp"
#include "../../rsc/scene_manager.hpp"
#include "../../rsc/world.hpp"
#include "../../reg/component_registry.hpp"
#include "../../reg/singleton_registry.hpp"
#include "../../reg/system_factory_registry.hpp"
#include "../../reg/runtime_project_module.hpp"
#include "../../reg/registry_queries.hpp"

#include "../../sys/core_systems.hpp"
#include "../../sys/audio_system.hpp"
#include "../../gfx/render_window.hpp"
#include "../../gfx/scene_renderer.hpp"
#include "../../phys/physics_engine.hpp"
#include "../../events.hpp"
#include "../../input.hpp"
#include "../../reg/sig/signal_hub.hpp"
#include "rendering_manager.hpp"
#include "ui_manager.hpp"
#include "physics_manager.hpp"

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace wsl
{

namespace comp::singl
{

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
    return m_app_input_map;
  }

  /** Returns the application input map. */
  const wsl::input::action_map &
  get_app_input_map () const
  {
    return m_app_input_map;
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
    return m_world;
  }
  rsc::world &
  world ()
  {
    return m_world;
  }
  rsc::scene_manager const &
  scene_manager () const
  {
    return m_scene_manager;
  }
  rsc::scene_manager &
  scene_manager ()
  {
    return m_scene_manager;
  }
  reg::component_registry const &
  component_registry () const
  {
    return m_component_registry;
  }
  reg::component_registry &
  component_registry ()
  {
    return m_component_registry;
  }
  reg::singleton_registry const &
  singleton_registry () const
  {
    return m_singleton_registry;
  }
  reg::singleton_registry &
  singleton_registry ()
  {
    return m_singleton_registry;
  }
  reg::system_factory_registry const &
  system_factory_registry () const
  {
    return m_system_factory_registry;
  }
  reg::system_factory_registry &
  system_factory_registry ()
  {
    return m_system_factory_registry;
  }
  reg::sig::signal_debug_db const &
  signal_db () const
  {
    return m_signal_db;
  }
  reg::sig::signal_debug_db &
  signal_db ()
  {
    return m_signal_db;
  }
  entt::dispatcher const &
  dispatcher () const
  {
    return m_dispatcher;
  }
  entt::dispatcher &
  dispatcher ()
  {
    return m_dispatcher;
  }
  reg::sig::signal_hub const &
  signal_hub () const
  {
    return m_signal_hub;
  }
  reg::sig::signal_hub &
  signal_hub ()
  {
    return m_signal_hub;
  }
  reg::registry_queries const &
  reg_queries () const
  {
    return m_reg_queries;
  }
  reg::registry_queries &
  reg_queries ()
  {
    return m_reg_queries;
  }
  reg::runtime::runtime_project_module const &
  runtime_project_module () const
  {
    return m_runtime_project_module;
  }
  reg::runtime::runtime_project_module &
  runtime_project_module ()
  {
    return m_runtime_project_module;
  }

  gfx::render_context const &
  render_ctx () const
  {
    return m_render_ctx;
  }
  gfx::render_context &
  render_ctx ()
  {
    return m_render_ctx;
  }
  rsc::resource_manager const &
  resource_manager () const
  {
    return m_resource_manager;
  }
  rsc::resource_manager &
  resource_manager ()
  {
    return m_resource_manager;
  }
  rsc::resource_manager_view const &
  resource_manager_view () const
  {
    return m_resource_manager_view;
  }
  rsc::resource_manager_view &
  resource_manager_view ()
  {
    return m_resource_manager_view;
  }
  gfx::render_window const &
  window () const
  {
    return m_window;
  }
  gfx::render_window &
  window ()
  {
    return m_window;
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

  rsc::world m_world;
  rsc::scene_manager m_scene_manager;
  reg::component_registry m_component_registry;
  reg::singleton_registry m_singleton_registry;
  reg::system_factory_registry m_system_factory_registry;
  reg::sig::signal_debug_db m_signal_db;
  entt::dispatcher m_dispatcher;
  reg::sig::signal_hub m_signal_hub;
  reg::registry_queries m_reg_queries;
  reg::runtime::runtime_project_module m_runtime_project_module;

  gfx::render_context m_render_ctx;
  rsc::resource_manager m_resource_manager;
  rsc::resource_manager_view m_resource_manager_view;
  gfx::render_window m_window;
  comp::singl::ui_manager m_ui_manager;
  bool m_headless = false;
  std::unique_ptr<sys::core_systems> m_core_systems;
  bool m_is_running = false;
  bool m_in_play_session = false;
  class editor_context *m_editor_ctx = nullptr;
  std::unordered_map<entt::id_type, std::string> m_scene_save_states;

  wsl::input::action_map m_app_input_map;
  wsl::input::action_map *m_current_input_map = nullptr;

  rsc::scene *m_play_session_origin_scene = nullptr;
  rsc::scene_id m_play_session_origin_scene_id{ entt::null };

  bool m_needs_save_active_scene = false;
};

} // namespace comp::singl

} // namespace wsl
