#include "area3d.hpp"

#include "phys/physics_engine.hpp"
#include "singl/runtime_context.hpp"

#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace wsl
{

namespace comp
{

void
area::sync_applied_cache ()
{
  applied_shape = shape;
  applied_half_extents = half_extents;
  applied_radius = radius;
  applied_position = position;
  applied_rotation = rotation;
}

bool
area::has_structural_change () const
{
  return shape != applied_shape
         || half_extents.x () != applied_half_extents.x ()
         || half_extents.y () != applied_half_extents.y ()
         || half_extents.z () != applied_half_extents.z ()
         || radius != applied_radius;
}

bool
area::has_transform_change () const
{
  return position.x () != applied_position.x ()
         || position.y () != applied_position.y ()
         || position.z () != applied_position.z ()
         || rotation.x () != applied_rotation.x ()
         || rotation.y () != applied_rotation.y ()
         || rotation.z () != applied_rotation.z ()
         || rotation.w () != applied_rotation.w ();
}

void
area::destroy_body (phys::engine &engine)
{
  if (!phys::is_valid_body_id (body_id)) {
    return;
  }

  engine.unregister_sensor (body_id);
  engine.on_remove_body (body_id);
  body_id = phys::null_body_id;
}

void
area::rebuild_body (phys::engine &engine, const glm::vec3 &world_pos,
                    const glm::quat &world_rot, const glm::vec3 &scale)
{
  destroy_body (engine);
  create_body (engine, world_pos, world_rot, scale);
}

void
area::create_body (phys::engine &engine, const glm::vec3 &world_pos,
                   const glm::quat &world_rot, const glm::vec3 &scale)
{
  if (phys::is_valid_body_id (body_id)) {
    destroy_body (engine);
  }

  phys::body_desc desc;
  desc.motion = phys::motion_type::Kinematic;
  desc.shape = shape == shape_type::box ? phys::shape_type::box
                                        : phys::shape_type::sphere;
  desc.position = { world_pos.x, world_pos.y, world_pos.z };
  desc.rotation = { world_rot.x, world_rot.y, world_rot.z, world_rot.w };
  desc.half_extents = { half_extents.x () * scale.x,
                        half_extents.y () * scale.y,
                        half_extents.z () * scale.z };
  desc.radius = radius * ((scale.x + scale.y + scale.z) / 3.0F);
  desc.sensor = true;
  body_id = engine.create_body (desc);
}

void
area::apply_transform_to_body (phys::engine &engine) const
{
  if (!phys::is_valid_body_id (body_id)) {
    return;
  }

  // Read current body world position/rotation
  const phys::vector3 body = engine.get_body_position (body_id);
  const phys::quaternion rotation_value = engine.get_body_rotation (body_id);
  glm::vec3 const body_pos (body.x, body.y, body.z);
  glm::quat const body_rot (rotation_value.w, rotation_value.x,
                            rotation_value.y, rotation_value.z);

  // Undo old offset to recover the transform's world rotation
  glm::quat const old_off_rot = (glm::quat)applied_rotation;
  glm::quat const xform_rot = body_rot * glm::inverse (old_off_rot);

  // Apply new offset: body = transform * offset
  glm::quat const new_off_rot = (glm::quat)rotation;
  glm::vec3 const offset_delta
      = (glm::vec3)position - (glm::vec3)applied_position;
  glm::vec3 const new_body_pos = body_pos + xform_rot * offset_delta;
  glm::quat const new_body_rot = xform_rot * new_off_rot;

  engine.set_body_transform (
      body_id, { new_body_pos.x, new_body_pos.y, new_body_pos.z },
      { new_body_rot.x, new_body_rot.y, new_body_rot.z, new_body_rot.w });
}

void
area::on_inspector_changed (comp::singl::runtime_context *runtime,
                            const glm::vec3 &scale)
{
  phys::engine *engine = (runtime != nullptr)
                             ? runtime->try_get_active_physics_engine ()
                             : nullptr;
  if (engine == nullptr) {
    return;
  }

  // Sanitize dimensions
  sanitize_dimensions ();

  const bool structural_change = has_structural_change ();
  const bool xform_change = has_transform_change ();

  if (!phys::is_valid_body_id (body_id)) {
    sync_applied_cache ();
    return;
  }

  if (structural_change) {
    // Read current body position from physics engine to preserve world placement
    const phys::vector3 current = engine->get_body_position (body_id);
    const phys::quaternion current_rotation
        = engine->get_body_rotation (body_id);
    glm::vec3 const current_pos (current.x, current.y, current.z);
    glm::quat const current_rot (current_rotation.w, current_rotation.x,
                                 current_rotation.y, current_rotation.z);
    rebuild_body (*engine, current_pos, current_rot, scale);
  } else {
    if (xform_change) {
      apply_transform_to_body (*engine);
    }
  }

  sync_applied_cache ();
}

void
area::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::area::shape_type> ()
      .type (entt::type_hash<comp::area::shape_type>::value ())
      .conv<int> ()
      .data<comp::area::shape_type::box> ("box"_hs)
      .custom<const char *> ("Box")
      .data<comp::area::shape_type::sphere> ("sphere"_hs)
      .custom<const char *> ("Sphere");

  entt::meta_factory<comp::area> ()
      .type (entt::type_hash<comp::area>::value ())
      .custom<comp::meta_info> (meta_info{
          "Area3D", "Sensor (trigger) to detect bodies entering/exiting",
          "" })
      .func<&comp::area::on_inspector_changed> ("on_inspector_changed"_hs)

      .data<&comp::area::shape> ("shape"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Shape", "Sensor collision shape", "" })

      .data<&comp::area::position> ("position"_hs)
      .custom<comp::meta_info> (meta_info{
          "Position",
          "Local position offset relative to the entity's Transform position",
          "" })

      .data<&comp::area::rotation> ("rotation"_hs)
      .custom<comp::meta_info> (meta_info{
          "Rotation",
          "Local rotation offset relative to the entity's Transform rotation",
          "" })

      .data<&comp::area::half_extents> ("half_extents"_hs)
      .custom<comp::meta_info> (meta_info{
          "Half Extents",
          "Box half size (scaled by the entity's Transform scale)", "" })

      .data<&comp::area::radius> ("radius"_hs)
      .custom<comp::meta_info> (meta_info{
          "Radius", "Sphere radius (scaled by the entity's Transform scale)",
          "" });
}

} // namespace comp
} // namespace wsl
