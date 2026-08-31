#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "../math/vector.hpp"
#endif
#include "component_meta.hpp"

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif

namespace wsl::comp
{

/**
 * 2D orthographic camera component.
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

};

} // namespace wsl::comp
