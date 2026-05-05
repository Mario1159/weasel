#pragma once

#include "scene.hpp"

#include <memory>
#include <vector>

#include <entt/entt.hpp>

namespace wsl
{

namespace comp::singl
{
class runtime_context;
class editor_context;
}

namespace rsc
{

/*!
 * \brief Owns the set of scenes available to a runtime context.
 *
 * The world is responsible for creating and destroying scenes and for wiring
 * newly created scenes to the current runtime and editor contexts.
 */
class world
{
public:
  /*!
   * \brief Creates a world bound to a runtime context.
   * \param runtime_ctx Runtime context shared by all scenes in the world.
   */
  explicit world (comp::singl::runtime_context *runtime_ctx);

  /*!
   * \brief Creates and stores a new scene.
   * \param name Human-readable scene name.
   * \return Reference to the newly created scene owned by the world.
   */
  scene &create_scene (const std::string &name);
  scene &add_scene (scene &&ready_scene);

  /*!
   * \brief Destroys a stored scene if it exists.
   * \param target Scene to remove. Passing `nullptr` has no effect.
   */
  void destroy_scene (scene *target);

  /*!
   * \brief Returns the owned scenes.
   * \return Mutable scene storage.
   */
  std::vector<std::unique_ptr<scene>> &get_scenes ();

  /*!
   * \brief Returns the owned scenes.
   * \return Read-only scene storage.
   */
  const std::vector<std::unique_ptr<scene>> &get_scenes () const;

  /*!
   * \brief Updates the editor context used for future scene creation.
   * \param editor_ctx Editor context shared with newly created scenes.
   */
  void set_editor_context (comp::singl::editor_context *editor_ctx);

  /*!
   * \brief Returns the runtime context backing this world.
   * \return Runtime context pointer provided at construction time.
   */
  comp::singl::runtime_context *get_runtime_context () const;

  /*!
   * \brief Removes all scenes from the world.
   */
  void clear ();

private:
  std::vector<std::unique_ptr<scene>> m_scenes;
  comp::singl::runtime_context *m_runtime_ctx;
  comp::singl::editor_context *m_editor_ctx = nullptr;
};

} // namespace rsc

} // namespace wsl
