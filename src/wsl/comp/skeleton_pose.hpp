#pragma once

#include "component_meta.hpp"

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/mat4x4.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstddef>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl
{

namespace comp
{

/**
 * Per-instance model-space joint palette produced by the animation system.
 *
 * This component is intentionally transient: it is rebuilt every animation
 * update and is neither registered as a serializable world component nor
 * exposed in the Add Component list.
 */
struct skeleton_pose : world_component
{
  /** Model-space palette in the owning model skin's joint order. */
  std::vector<glm::mat4> palette;

  /** Returns the number of joints currently present in the palette. */
  [[nodiscard]] std::size_t
  joint_count () const
  {
    return palette.size ();
  }

  /** Resizes the palette, initializing new entries to identity. */
  void
  resize (std::size_t count)
  {
    palette.resize (count, glm::mat4 (1.0F));
  }

  /** Clears the transient pose. */
  void
  clear ()
  {
    palette.clear ();
  }

  /** Metadata hook kept separate from the serializable component registry. */
  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::skeleton_pose> ()
        .type (entt::type_hash<comp::skeleton_pose>::value ())
        .custom<comp::meta_info> (meta_info{
            "Skeleton Pose", "Transient model-space joint palette", "" });
  }
};

} // namespace comp

} // namespace wsl
