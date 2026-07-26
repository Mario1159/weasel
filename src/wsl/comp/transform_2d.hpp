#pragma once

#include "../math/vector.hpp"
#include "component_meta.hpp"

#include <entt/entt.hpp>

namespace wsl::comp
{

/**
 * 2D local transform (position, rotation, scale).
 *
 * Entities with this component are skipped by the 3D transform_system.
 * The sprite renderer uses this directly for 2D positioning.
 */
struct transform_2d : world_component
{
  math::vec2f position{ 0.0F, 0.0F };
  float rotation = 0.0F; // Z rotation in degrees
  math::vec2f scale{ 1.0F, 1.0F };
  math::vec2f pivot{ 0.5F, 0.5F }; // Normalized pivot (0.5 = center)

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    transform_2d def{};
    serialize_field_if_diff (archive, "position", position, def.position);
    serialize_field_if_diff (archive, "rotation", rotation, def.rotation);
    serialize_field_if_diff (archive, "scale", scale, def.scale);
    serialize_field_if_diff (archive, "pivot", pivot, def.pivot);
  }
};

} // namespace wsl::comp
