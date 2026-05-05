#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

#include "resource_ids.hpp"
#include "resource_ref.hpp"
#include "../sys/system.hpp"


namespace wsl
{

namespace comp
{
namespace singl
{
class runtime_context;
class editor_context;
} // namespace singl
} // namespace comp

namespace rsc
{

/*!
 * \brief Represents a collection of entities and systems that form a unit of gameplay or world space.
 *
 * The scene manages an EnTT registry and a list of systems that operate on that registry.
 * It also handles entity naming, resource tracking for asynchronous loading, and entity copying.
 */
class scene
{
public:
  /*!
   * \brief Constructs a scene with a runtime and optional editor context.
   * \param runtime_ctx Pointer to the runtime context.
   * \param editor_ctx Pointer to the editor context.
   * \param name The name of the scene.
   */
  explicit scene (comp::singl::runtime_context *runtime_ctx,
                  comp::singl::editor_context *editor_ctx,
                  const std::string &name);
  ~scene () = default;

  scene (scene &&) = default;
  scene &operator= (scene &&) = default;

  scene (const scene &) = delete;
  scene &operator= (const scene &) = delete;

  /*! \brief Returns the EnTT registry owned by this scene. */
  entt::registry &get_registry ();

  /*! \brief Returns the scene name. */
  const std::string &get_name () const;

  /*! \brief Sets the scene name. */
  void set_name (std::string name);

  /*!
   * \brief Adds a system to the scene.
   * \tparam T The type of the system.
   * \param args Arguments to pass to the system's constructor.
   * \return Reference to the created system.
   */
  template <typename T, typename... Args> T &add_system (Args &&...args);

  /*!
   * \brief Adds a system instance to the scene.
   * \param sys Unique pointer to the system instance.
   * \param initialize_if_running Whether to initialize the system if the scene is already running.
   * \return Reference to the added system.
   */
  sys::ecs_system &add_system_instance (std::unique_ptr<sys::ecs_system> sys,
                                        bool initialize_if_running = true);

  /*! \brief Removes a system from the scene. */
  void remove_system (sys::ecs_system *system);

  /*! \brief Initializes the scene and its systems. */
  void init ();

  /*! \brief Pauses the scene and its systems. */
  void pause ();

  /*! \brief Resumes the scene and its systems. */
  void resume ();

  /*! \brief Updates all active systems in the scene. */
  void update (double dt);

  /*! \brief Forwards an SDL event to all active systems in the scene. */
  void handle_events (const SDL_Event &e);

  /*! \brief Shuts down all systems and clears the registry. */
  void stop_and_clear ();

  /*! \brief Shuts down all systems and clears their storage. */
  void clear ();

  /*! \brief Clears all entities and their names from the registry. */
  void clear_registry ();

  /*! \brief Adds a resource to the scene's load list. */
  void add_resource (io::resource_type type, entt::id_type id);

  /*! \brief Removes a resource from the scene's load list. */
  void remove_resource (io::resource_type type, entt::id_type id);

  /*! \brief Checks if a resource is in the scene's load list. */
  bool has_resource (io::resource_type type, entt::id_type id) const;

  /*! \brief Returns the list of resources to be loaded for this scene. */
  const std::vector<io::resource_ref> &get_load_list () const;

  /*! \brief Sets the name of an entity. */
  void set_entity_name (entt::entity e, std::string name);

  /*! \brief Gets the name of an entity. */
  const std::string &get_entity_name (entt::entity e) const;

  /*! \brief Removes an entity name entry. */
  void remove_entity_name (entt::entity e);

  /*! \brief Returns the internal mapping of entities to names. */
  std::unordered_map<entt::entity, std::string> &get_entity_names ();

  /*! \brief Returns the internal mapping of entities to names (const). */
  const std::unordered_map<entt::entity, std::string> &
  get_entity_names () const;

  /*!
   * \brief Copies an entity and its components from another scene to this one.
   * \param src_scene The source scene.
   * \param src_entity The source entity to copy.
   * \param dst_parent Optional parent entity in this scene.
   * \param is_instantiating_prefab Whether the copy is part of a prefab instantiation.
   * \param prefab_id The ID of the prefab if applicable.
   * \return The newly created entity in this scene.
   */
  entt::entity copy_entity (scene &src_scene, entt::entity src_entity,
                            entt::entity dst_parent = entt::null,
                            bool is_instantiating_prefab = false,
                            rsc::scene_id prefab_id = {});

  /*! \brief Returns a list of raw pointers to systems in this scene. */
  std::vector<sys::ecs_system *> get_systems ();

  //! The systems associated with this scene.
  std::vector<std::unique_ptr<sys::ecs_system>> systems;

  //! The active camera entity in this scene.
  entt::entity camera{ entt::null };

protected:
  bool m_running = false;

private:
  friend class scene_manager;
  void on_system_added (sys::ecs_system &system);
  void ensure_context_bindings ();
  void reset_scene_context ();
  void refresh_system_states ();
  void shutdown_systems ();
  bool is_playing () const;

  std::string m_name;
  std::unordered_map<entt::entity, std::string> m_entity_names;

  entt::registry m_registry;
  std::vector<io::resource_ref> m_load_list;

  bool m_initialized = false;

  comp::singl::runtime_context *m_runtime_ctx;
  comp::singl::editor_context *m_editor_ctx;
};

template <typename T, typename... Args>
inline T &
scene::add_system (Args &&...args)
{
  static_assert (std::is_base_of_v<sys::ecs_system, T>);
  auto sys = std::make_unique<T> (std::forward<Args> (args)...);
  return static_cast<T &> (add_system_instance (std::move (sys)));
}

} // namespace rsc

} // namespace wsl
