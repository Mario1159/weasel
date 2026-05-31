#pragma once

#include "../../rsc/resource_manager.hpp"
#include "../../rsc/scene_manager.hpp"
#include "../../rsc/world.hpp"
#include "../../reg/component_registry.hpp"
#include "../../reg/singleton_registry.hpp"
#include "../../reg/system_factory_registry.hpp"
#include "../../reg/runtime_project_module_api.hpp"
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

/*!
 * \brief Core shared state for a Weasel runtime instance.
 *
 * This singleton aggregates all major subsystems (resource manager, scene
 * manager, world, dispatcher) and provides a central point of access for
 * runtime logic.
 */
class runtime_context : public comp::singleton_component
{
public:
  /*!
   * \brief Constructs the runtime context.
   * \param name Window title.
   * \param width Window width.
   * \param height Window height.
   * \param engine_res_path Base path for engine resources.
   */
  explicit runtime_context (const char *name, int width, int height,
                            const std::string &engine_res_path,
                            bool headless = false);

  ~runtime_context ();

  /*! \brief Register reflection metadata for this class. */
  static void register_meta ();

  /*! \brief ImGui inspector for runtime context state. */
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx_ptr);

  /*! \brief Returns the active rendering manager from the current scene. */
  rendering_manager *get_active_rendering_manager () const;

  /*! \brief Attempts to get the active scene renderer, may return nullptr. */
  gfx::scene_renderer *try_get_active_scene_renderer ();

  /*! \brief Gets the active scene renderer, ensuring it exists. */
  gfx::scene_renderer &get_active_scene_renderer ();

  /*! \brief Returns the active physics manager from the current scene. */
  physics_manager *get_active_physics_manager () const;

  /*! \brief Attempts to get the active physics engine, may return nullptr. */
  phys::engine *try_get_active_physics_engine ();

  /*! \brief Gets the active physics engine, ensuring it exists. */
  phys::engine &get_active_physics_engine ();

  /*! \brief Sets the simulation running state. */
  void set_running (bool value);

  /*! \brief Stops the current play session and restores scene states. */
  void stop ();

  /*! \brief Synchronizes deferred state changes. */
  void sync ();

  /*! \brief Saves the state of a specific scene for later restoration. */
  void save_scene_state (rsc::scene *scene);

  /*! \brief Saves the state of the currently active scene. */
  void save_active_scene_state ();

  /*! \brief Callback for scene change events. */
  void on_scene_changed (const wsl::event::scene_changed &event);

  /*! \brief Assigns an editor context for tool-specific behaviors. */
  void set_editor_ctx (class editor_context *editor_ctx);

  /*! \brief Returns the application input map. */
  wsl::input::action_map &
  get_app_input_map ()
  {
    return app_input_map;
  }

  /*! \brief Returns the application input map. */
  const wsl::input::action_map &
  get_app_input_map () const
  {
    return app_input_map;
  }

  /*! \brief Returns current input map (may be null). */
  wsl::input::action_map *
  get_current_input_map () const
  {
    return current_input_map;
  }

  /*! \brief Returns whether this context was created in headless mode. */
  bool
  is_headless () const
  {
    return m_headless;
  }

  rsc::world world;
  rsc::scene_manager scene_manager;

  reg::component_registry component_registry;
  reg::singleton_registry singleton_registry;
  reg::system_factory_registry system_factory_registry;
  reg::sig::signal_debug_db signal_db;
  entt::dispatcher dispatcher;
  reg::sig::signal_hub signal_hub;
  reg::registry_queries reg_queries;
  reg::runtime::runtime_project_module runtime_project_module;

private:
  struct sdl_init_guard
  {
    sdl_init_guard (bool headless = false);
    ~sdl_init_guard ();

    bool is_headless = false;
  };
  sdl_init_guard sdl_init_guard_;

public:
  gfx::render_context render_ctx;
  rsc::resource_manager resource_manager;
  rsc::resource_manager_view resource_manager_view;
  gfx::render_window window;

  comp::singl::ui_manager ui_manager;

  bool m_headless = false;
  std::unique_ptr<sys::core_systems> core_systems;

  entt::entity game_camera = entt::null;
  bool is_running = false;
  bool in_play_session = false;

  class editor_context *editor_ctx = nullptr;
  std::unordered_map<entt::id_type, std::string> scene_save_states;

private:
  wsl::input::action_map app_input_map;
  wsl::input::action_map *current_input_map = nullptr;

  rsc::scene *play_session_origin_scene = nullptr;
  rsc::scene_id play_session_origin_scene_id{ entt::null };

  bool m_needs_save_active_scene = false;
};

} // namespace comp::singl

} // namespace wsl
