#pragma once

#include "../comp/component_meta.hpp"
#include "world.hpp"

#include <entt/entity/entity.hpp>

namespace wsl
{

namespace rsc
{

/**
 * Tracks and updates the currently active scene in a world.
 *
 * The scene manager owns no scene storage itself. It selects one scene from
 * the associated world, ensures it is initialized when activated, and forwards
 * per-frame updates and events to it.
 */
class scene_manager : public comp::singleton_component
{
public:
  /**
 * Creates a manager bound to a world instance.
 * :param world: World that owns the scenes managed by this object.
 */
  explicit scene_manager (world &world) : m_main_world (world) {}
  static void register_meta ();

  /**
 * Sets the active scene.
 * :param scene_ptr: Scene to activate, or `nullptr` to clear the active scene.
 *
 * Activating a new scene initializes it on first use and emits the
 * `wsl::event::scene_changed` event through the runtime dispatcher.
 */
  void set_active (scene *scene_ptr);
  scene &create_scene (const std::string &name, bool make_active = false);
  scene &create_default_scene (const std::string &name, bool make_active);
  void destroy_scene (scene *scene_ptr);

  /**
 * Returns the currently active scene.
 * :return: Active scene pointer, or `nullptr` when no scene is active.
 */
  scene *get_active () const;
  world &get_world ();
  const world &get_world () const;

  /**
 * Updates the active scene.
 * :param dt: Frame delta time in seconds.
 */
  void update (double dt);

  /**
 * Forwards an engine event to the active scene.
 * :param event: Event to dispatch.
 */
  void handle_events (const wsl::engine_event &event);
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx);

private:
  world &m_main_world;
  scene *m_active_scene = nullptr;
};

} // namespace rsc

} // namespace wsl
