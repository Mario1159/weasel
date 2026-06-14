#pragma once

#include "../math/vector.hpp"
#include "component_meta.hpp"

#include <entt/entt.hpp>

namespace wsl::comp
{

/*!
 * \brief 2D orthographic camera component.
 *
 * An entity with this component should also have a transform_2d component that
 * defines the camera centre.  The projection is always orthographic.
 */
struct camera_2d : world_component
{
  float zoom = 1.0F;
  bool use_window_as_viewport = true;
  math::vec2f viewport_offset{ 0.0F, 0.0F };
  math::vec2f viewport_size{ 0.0F, 0.0F };
  int layer = 0;
  bool only_for_editor = false;

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    camera_2d def{};
    serialize_field_if_diff (archive, "zoom", zoom, def.zoom);
    serialize_field_if_diff (archive, "use_window_as_viewport",
                             use_window_as_viewport,
                             def.use_window_as_viewport);
    serialize_field_if_diff (archive, "viewport_offset", viewport_offset,
                             def.viewport_offset);
    serialize_field_if_diff (archive, "viewport_size", viewport_size,
                             def.viewport_size);
    serialize_field_if_diff (archive, "layer", layer, def.layer);
    serialize_field_if_diff (archive, "only_for_editor", only_for_editor,
                             def.only_for_editor);
  }
};

} // namespace wsl::comp
