#pragma once

#ifndef IN_MODULE_INTERFACE
#include "../math/vector.hpp" // math::vec3f, math::quatf
#endif
#ifndef IN_MODULE_INTERFACE
#include "../phys/physics_engine.hpp" // phys::engine
#include "../phys/layers.hpp"
#endif
namespace wsl::comp::singl
{
class runtime_context;
}
#ifndef IN_MODULE_INTERFACE
#include <exception>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <algorithm>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstdint>
#endif
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <type_traits>
#endif

namespace wsl
{

namespace comp
{

struct rigid_body : world_component
{
  struct motion_type_ui
  {
    phys::motion_type value = phys::motion_type::Dynamic;

    bool custom_inspect (const char *label,
                         comp::singl::runtime_context *runtime);
    static void register_meta ();
  };

  struct allowed_dofs_ui
  {
    phys::allowed_dofs value = phys::allowed_dofs::All;

    bool custom_inspect (const char *label,
                         comp::singl::runtime_context *runtime);
    static void register_meta ();
  };

  struct collision_layer_ui
  {
    phys::layers::layer_index_t value = 0;

    bool custom_inspect (const char *label,
                         comp::singl::runtime_context *runtime);
    static void register_meta ();
  };

  struct collision_mask_ui
  {
    phys::layers::layer_mask_t value = phys::layers::all_collision_layers;

    bool custom_inspect (const char *label,
                         comp::singl::runtime_context *runtime);
    static void register_meta ();
  };

  enum class shape_type
  {
    box = 0,
    sphere = 1
  };

  // --- authored state ---
  shape_type shape = shape_type::box;

  // Local offset relative to the entity's Transform component.
  // The body's world position/rotation is computed as:
  //   body_world = transform_world * offset (rotation) + offset (translation)
  math::vec3f position{ 0, 0, 0 };
  math::quatf rotation{ 0, 0, 0, 1 };

  math::vec3f half_extents{ 0.5F, 0.5F, 0.5F };
  float radius = 0.5F;

  // Material density (kg/m^3) used to derive the body mass from its shape
  // volume. Defaults to water (1000), matching the physics engine's default so existing
  // scenes keep their current mass.
  float density = 1000.0F;
  static constexpr float default_density = 1000.0F;

  // kept for authored/debug state, but runtime layer is derived from
  // motion_type
  bool dynamic = true;

  // UI wrappers
  motion_type_ui motion_type{};
  allowed_dofs_ui allowed_dofs{};
  collision_layer_ui collision_layer{};
  collision_mask_ui collision_mask{};

  // Physics surface response parameters
  float friction = 0.2F;
  float restitution = 0.0F;

  // runtime handle (NOT serialized)
  phys::body_id body_id;

  // --- runtime cache to detect structural edits (editor-only, not serialized)
  shape_type applied_shape = shape_type::box;
  math::vec3f applied_half_extents{ 0.5F, 0.5F, 0.5F };
  float applied_radius = 0.5F;
  bool applied_dynamic = true;
  phys::motion_type applied_motion = phys::motion_type::Dynamic;
  phys::allowed_dofs applied_dofs = phys::allowed_dofs::All;
  phys::layers::layer_index_t applied_collision_layer = 0;
  phys::layers::layer_mask_t applied_collision_mask
      = phys::layers::all_collision_layers;
  float applied_friction = 0.2F;
  float applied_restitution = 0.0F;
  float applied_density = 1000.0F;
  math::vec3f applied_position{ 0, 0, 0 };
  math::quatf applied_rotation{ 0, 0, 0, 1 };
  math::vec3f applied_scale{ 1, 1, 1 };

  // --- creation helpers ---
  static rigid_body create_box_body (phys::engine &engine, const glm::vec3 &pos,
                                     const glm::quat &rot, const glm::vec3 &he,
                                     bool dyn);

  static rigid_body create_sphere_body (phys::engine &engine,
                                        const glm::vec3 &pos, float r,
                                        phys::motion_type motion,
                                        phys::allowed_dofs dofs);

  // runtime ops
  // world_pos and world_rot must be the entity's world-space position/rotation
  // (derived from its transform component).
  void create_body (phys::engine &engine, const glm::vec3 &world_pos,
                    const glm::quat &world_rot,
                    const glm::vec3 &scale = { 1, 1, 1 });
  void destroy_body (phys::engine &engine);
  void rebuild_body (phys::engine &engine, const glm::vec3 &world_pos,
                     const glm::quat &world_rot,
                     const glm::vec3 &scale = { 1, 1, 1 });

  // only non-structural live update (safe + exists)
  void apply_transform_to_body (phys::engine &engine) const;
  void apply_surface_properties_to_body (phys::engine &engine) const;

  // called by inspector after any field edit
  void on_inspector_changed (comp::singl::runtime_context *runtime,
                             const glm::vec3 &scale = { 1, 1, 1 });

  // sync current authored values into applied_* cache
  void sync_applied_cache ();
  phys::object_layer object_layer () const;

  // Derive the body mass (kg) from density and shape volume. This mirrors the
  // mass the physics engine assigns at body creation given the configured density.
  float
  mass () const
  {
    if (shape == shape_type::sphere) {
      const float volume
          = (4.0F / 3.0F) * 3.14159265358979323846F * radius * radius * radius;
      return density * volume;
    }
    const float volume
        = half_extents.x () * half_extents.y () * half_extents.z () * 8.0F;
    return density * volume;
  }

  bool has_structural_change () const;
  bool has_surface_change () const;
  bool has_transform_change () const;
  bool has_scale_change (const math::vec3f &scale) const;

  void
  sanitize_dimensions ()
  {
    half_extents.x () = std::max (half_extents.x (), 1e-3F);
    half_extents.y () = std::max (half_extents.y (), 1e-3F);
    half_extents.z () = std::max (half_extents.z (), 1e-3F);
    radius = std::max (radius, 1e-3F);
  }

  void
  sanitize_surface_properties ()
  {
    friction = std::clamp (friction, 0.0F, 1.0F);
    restitution = std::clamp (restitution, 0.0F, 1.0F);
    collision_layer.value
        = phys::layers::clamp_layer_index (collision_layer.value);
    collision_mask.value
        = phys::layers::clamp_layer_mask (collision_mask.value);
  }

  static void register_meta ();
};

} // namespace comp

} // namespace wsl
