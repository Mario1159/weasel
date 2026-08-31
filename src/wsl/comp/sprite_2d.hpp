#pragma once

#include "component_meta.hpp"
#if !defined(WSL_MODULE_BUILD)
#include "../math/vector.hpp"
#endif
#include "wsl/rsc/resource_ids.hpp"
#if !defined(WSL_MODULE_BUILD)
#include <glm/glm.hpp>
#endif

namespace wsl::comp
{

/** Component for rendering a 2D sprite. */
struct sprite_2d : world_component
{
  /** Image resource to display. */
  wsl::rsc::image_id image{};

  /** Size of the sprite in pixels (before transform scaling). */
  math::vec2f size{ 100.0F, 100.0F };

  /** Tint color applied to the sprite. */
  math::vec4f color{ 1.0F, 1.0F, 1.0F, 1.0F };

  /** Texture coordinate offset and scale. */
  math::vec2f uv_offset{ 0.0F, 0.0F };
  math::vec2f uv_scale{ 1.0F, 1.0F };

  /** Vertical flip. */
  bool flip_v = false;
  /** Horizontal flip. */
  bool flip_h = false;

  /** Z-index for sorting (higher values are rendered on top). */
  int z_index = 0;

  static void register_meta ();

};

} // namespace wsl::comp
