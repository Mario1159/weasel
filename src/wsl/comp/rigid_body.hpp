#pragma once

#include "../math/vector.hpp"         // math::vec3f, math::quatf
#include "../phys/physics_engine.hpp" // phys::engine
#include "singl/runtime_context.hpp"
#include <cereal/cereal.hpp>
#include <exception>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <entt/entt.hpp>
#include <type_traits>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>

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
    phys::allowed_do_fs value = phys::allowed_do_fs::All;

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

  // kept for authored/debug state, but runtime layer is derived from
  // motion_type
  bool dynamic = true;

  // UI wrappers
  motion_type_ui motion_type{};
  allowed_dofs_ui allowed_dofs{};
  collision_layer_ui collision_layer{};
  collision_mask_ui collision_mask{};

  // Jolt surface response parameters
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
  phys::allowed_do_fs applied_dofs = phys::allowed_do_fs::All;
  phys::layers::layer_index_t applied_collision_layer = 0;
  phys::layers::layer_mask_t applied_collision_mask
      = phys::layers::all_collision_layers;
  float applied_friction = 0.2F;
  float applied_restitution = 0.0F;
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
                                        phys::allowed_do_fs dofs);

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
  JPH::ObjectLayer object_layer () const;
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

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    rigid_body def{};
    int shape_i = (int)shape;
    int motion_i = (int)motion_type.value;
    int dofs_i = (int)allowed_dofs.value;
    int collision_layer_i = (int)collision_layer.value;
    int collision_mask_i = (int)collision_mask.value;

    if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
      if (shape_i != (int)def.shape)
        ar (cereal::make_nvp ("shape", shape_i));
      serialize_field_if_diff (ar, "position", position, def.position);
      serialize_field_if_diff (ar, "rotation", rotation, def.rotation);
      serialize_field_if_diff (ar, "half_extents", half_extents,
                               def.half_extents);
      serialize_field_if_diff (ar, "radius", radius, def.radius);
      serialize_field_if_diff (ar, "dynamic", dynamic, def.dynamic);
      if (motion_i != (int)def.motion_type.value)
        ar (cereal::make_nvp ("motion_type", motion_i));
      if (dofs_i != (int)def.allowed_dofs.value)
        ar (cereal::make_nvp ("allowed_dofs", dofs_i));
      if (collision_layer_i != (int)def.collision_layer.value)
        ar (cereal::make_nvp ("collision_layer", collision_layer_i));
      if (collision_mask_i != (int)def.collision_mask.value)
        ar (cereal::make_nvp ("collision_mask", collision_mask_i));
      serialize_field_if_diff (ar, "friction", friction, def.friction);
      serialize_field_if_diff (ar, "restitution", restitution, def.restitution);
    } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
      shape_i = (int)def.shape;
      position = def.position;
      rotation = def.rotation;
      half_extents = def.half_extents;
      radius = def.radius;
      dynamic = def.dynamic;
      motion_i = (int)def.motion_type.value;
      dofs_i = (int)def.allowed_dofs.value;
      collision_layer_i = (int)def.collision_layer.value;
      collision_mask_i = (int)def.collision_mask.value;
      friction = def.friction;
      restitution = def.restitution;

      try {
        ar (cereal::make_nvp ("shape", shape_i));
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "position", position, def.position);
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "rotation", rotation, def.rotation);
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "half_extents", half_extents,
                                 def.half_extents);
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "radius", radius, def.radius);
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "dynamic", dynamic, def.dynamic);
      } catch (...) {
      }
      try {
        ar (cereal::make_nvp ("motion_type", motion_i));
      } catch (...) {
      }
      try {
        ar (cereal::make_nvp ("allowed_dofs", dofs_i));
      } catch (...) {
      }
      try {
        ar (cereal::make_nvp ("collision_layer", collision_layer_i));
      } catch (...) {
      }
      try {
        ar (cereal::make_nvp ("collision_mask", collision_mask_i));
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "friction", friction, def.friction);
      } catch (...) {
      }
      try {
        serialize_field_if_diff (ar, "restitution", restitution,
                                 def.restitution);
      } catch (...) {
      }

      shape = (shape_type)shape_i;
      motion_type.value = (JPH::EMotionType)motion_i;
      allowed_dofs.value = (JPH::EAllowedDOFs)dofs_i;
      collision_layer.value = phys::layers::clamp_layer_index (
          static_cast<phys::layers::layer_index_t> (collision_layer_i));
      collision_mask.value = phys::layers::clamp_layer_mask (
          static_cast<phys::layers::layer_mask_t> (collision_mask_i));

      sanitize_dimensions ();
      sanitize_surface_properties ();

      body_id = JPH::BodyID{};

      sync_applied_cache ();
    } else {
      ar (cereal::make_nvp ("shape", shape_i),
          cereal::make_nvp ("position", position),
          cereal::make_nvp ("rotation", rotation),
          cereal::make_nvp ("half_extents", half_extents),
          cereal::make_nvp ("radius", radius),
          cereal::make_nvp ("dynamic", dynamic),
          cereal::make_nvp ("motion_type", motion_i),
          cereal::make_nvp ("allowed_dofs", dofs_i),
          cereal::make_nvp ("collision_layer", collision_layer_i),
          cereal::make_nvp ("collision_mask", collision_mask_i),
          cereal::make_nvp ("friction", friction),
          cereal::make_nvp ("restitution", restitution));
      if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase,
                                      Archive>) {
        shape = (shape_type)shape_i;
        motion_type.value = (JPH::EMotionType)motion_i;
        allowed_dofs.value = (JPH::EAllowedDOFs)dofs_i;
        collision_layer.value = phys::layers::clamp_layer_index (
            static_cast<phys::layers::layer_index_t> (collision_layer_i));
        collision_mask.value = phys::layers::clamp_layer_mask (
            static_cast<phys::layers::layer_mask_t> (collision_mask_i));

        sanitize_dimensions ();
        sanitize_surface_properties ();

        body_id = JPH::BodyID{};

        sync_applied_cache ();
      }
    }
  }
};

} // namespace comp

} // namespace wsl
