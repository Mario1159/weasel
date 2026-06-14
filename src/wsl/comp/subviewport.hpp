#pragma once

#include "component_meta.hpp"

#include <entt/entt.hpp>

namespace wsl::comp
{

/*!
 * \brief Defines a viewport sub-region for rendering.
 *
 * Placed on an entity in the hierarchy. Cameras find their containing viewport
 * by walking up to the nearest ancestor with this component. If no ancestor
 * has one, the camera belongs to the root (fullscreen) viewport.
 */
struct subviewport : world_component
{
  //! Normalized left edge (0 = left, 1 = right edge of parent).
  float x = 0.0F;
  //! Normalized top edge (0 = top, 1 = bottom edge of parent).
  float y = 0.0F;
  //! Normalized width (1.0 = full width of parent).
  float width = 1.0F;
  //! Normalized height (1.0 = full height of parent).
  float height = 1.0F;

  //! Whether to clear the color target before this viewport.
  bool clear_color = false;
  //! Whether to clear the depth target before this viewport.
  bool clear_depth = false;

  //! RGBA clear colour (used when clear_color is true).
  float clear_r = 0.0F;
  float clear_g = 0.0F;
  float clear_b = 0.0F;
  float clear_a = 1.0F;

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    subviewport def{};
    serialize_field_if_diff (archive, "x", x, def.x);
    serialize_field_if_diff (archive, "y", y, def.y);
    serialize_field_if_diff (archive, "width", width, def.width);
    serialize_field_if_diff (archive, "height", height, def.height);
    serialize_field_if_diff (archive, "clear_color", clear_color,
                             def.clear_color);
    serialize_field_if_diff (archive, "clear_depth", clear_depth,
                             def.clear_depth);
    serialize_field_if_diff (archive, "clear_r", clear_r, def.clear_r);
    serialize_field_if_diff (archive, "clear_g", clear_g, def.clear_g);
    serialize_field_if_diff (archive, "clear_b", clear_b, def.clear_b);
    serialize_field_if_diff (archive, "clear_a", clear_a, def.clear_a);
  }
};

} // namespace wsl::comp
