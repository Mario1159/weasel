#pragma once

#include "component_meta.hpp"
#include "../math/vector.hpp"
#include "wsl/rsc/resource_ids.hpp"
#include <glm/glm.hpp>

namespace wsl::comp
{

/** Component for rendering a 2D sprite. */
struct sprite_2d : world_component
{
  /** Image resource to display. */
  wsl::rsc::image_id image = wsl::rsc::image_id{ 0 };

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

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    sprite_2d const def{};
    serialize_field_if_diff (archive, "image", image.value,
                             def.image.value);
    serialize_field_if_diff (archive, "size", size, def.size);
    serialize_field_if_diff (archive, "color", color, def.color);
    serialize_field_if_diff (archive, "uv_offset", uv_offset, def.uv_offset);
    serialize_field_if_diff (archive, "uv_scale", uv_scale, def.uv_scale);
    serialize_field_if_diff (archive, "flip_v", flip_v, def.flip_v);
    serialize_field_if_diff (archive, "flip_h", flip_h, def.flip_h);
    serialize_field_if_diff (archive, "z_index", z_index, def.z_index);
  }
};

} // namespace wsl::comp
