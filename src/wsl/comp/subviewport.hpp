#pragma once

#include "../math/vector.hpp"
#include "component_meta.hpp"

#include <entt/entt.hpp>

namespace wsl::comp
{

namespace singl
{
class runtime_context;
}

/*!
 * \brief Custom UI type for subviewport camera picker.
 *
 * Wraps entt::entity to provide a custom inspector that only shows
 * cameras that are descendants of the subviewport entity.
 */
struct subviewport_camera_ui
{
  entt::entity value = entt::null;
  //! If true, the inspector only shows descendants with camera_2d.
  //! If false, it only shows descendants with camera (3D).
  bool filter_2d = false;

  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx);

  static void register_meta ();
};

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

  //! 2D camera entity for this viewport.
  subviewport_camera_ui camera_2d{ .filter_2d = true };
  //! 3D camera entity for this viewport.
  subviewport_camera_ui camera_3d{ .filter_2d = false };

  //! Size of the quad when rendered in 3D space.
  math::vec2f world_quad_size{ 1.0F, 1.0F };

  //! Size in pixels for 2D overlay.
  math::vec2f container_size{ 320.0F, 180.0F };

  //! Position in pixels for 2D overlay.
  math::vec2f container_position{ 0.0F, 0.0F };

  //! Internal resolution of the viewport.
  math::vec2f virtual_size{ 1920.0F, 1080.0F };

  //! If true, this viewport is rendered in 2D mode only.
  bool render_2d_only = false;

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
    serialize_field_if_diff (archive, "camera_2d", camera_2d.value,
                             def.camera_2d.value);
    serialize_field_if_diff (archive, "camera_3d", camera_3d.value,
                             def.camera_3d.value);
    // Backward compatibility: old single "camera" field maps to camera_3d
    if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
      if (camera_3d.value == entt::null) {
        try {
          archive (cereal::make_nvp ("camera", camera_3d.value));
        } catch (const std::exception &) {
          /* old field not present */
        }
      }
    }
    serialize_field_if_diff (archive, "world_quad_size", world_quad_size,
                             def.world_quad_size);
    serialize_field_if_diff (archive, "container_size", container_size,
                             def.container_size);
    serialize_field_if_diff (archive, "container_position", container_position,
                             def.container_position);
    serialize_field_if_diff (archive, "virtual_size", virtual_size,
                             def.virtual_size);
    serialize_field_if_diff (archive, "render_2d_only", render_2d_only,
                             def.render_2d_only);
  }
};

/**
 * @brief Walks up the hierarchy from entity to find the nearest ancestor with a
 * subviewport component.
 * @return The nearest subviewport entity, or entt::null if none (Root
 * Viewport).
 */
entt::entity find_nearest_viewport (entt::registry &registry,
                                    entt::entity entity);

/**
 * @brief Finds the viewport that owns an entity through its parent chain.
 *
 * For subviewport entities this returns the containing parent viewport, not the
 * entity itself. This is useful when traversing/rendering viewport nodes.
 */
entt::entity find_parent_viewport (entt::registry &registry,
                                   entt::entity entity);

/**
 * @brief Returns true when an entity belongs to the render scope of a viewport.
 *
 * Root viewport membership is represented by target_viewport == entt::null.
 * Subviewport membership requires a descendant entity; the subviewport entity
 * itself is not part of its own render contents.
 */
bool entity_in_viewport_scope (entt::registry &registry, entt::entity entity,
                               entt::entity target_viewport);

} // namespace wsl::comp
