#include "area3d.hpp"

#include "../phys/utils.hpp" // to_jolt(...)
#include "phys/physics_engine.hpp"
#include "singl/runtime_context.hpp"

#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
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
  return shape != applied_shape || half_extents.x != applied_half_extents.x
         || half_extents.y != applied_half_extents.y
         || half_extents.z != applied_half_extents.z
         || radius != applied_radius;
}

bool
area::has_transform_change () const
{
  return position.x != applied_position.x || position.y != applied_position.y
         || position.z != applied_position.z || rotation.x != applied_rotation.x
         || rotation.y != applied_rotation.y || rotation.z != applied_rotation.z
         || rotation.w != applied_rotation.w;
}

void
area::destroy_body (phys::engine &engine)
{
  if (body_id.IsInvalid ()) {
    return;
  }

  engine.unregister_sensor (body_id);
  engine.on_remove_body (body_id);
  body_id = JPH::BodyID{};
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
  if (!body_id.IsInvalid ()) {
    destroy_body (engine);
  }

  JPH::ShapeRefC shape_ref;

  if (shape == shape_type::box) {
    JPH::RefConst<JPH::ShapeSettings> const s = new JPH::BoxShapeSettings (
        to_jolt (half_extents) * JPH::Vec3 (scale.x, scale.y, scale.z));
    shape_ref = s->Create ().Get ();
  } else {
    float const avg_scale = (scale.x + scale.y + scale.z) / 3.0F;
    JPH::RefConst<JPH::ShapeSettings> const s
        = new JPH::SphereShapeSettings (radius * avg_scale);
    shape_ref = s->Create ().Get ();
  }

  const JPH::RVec3 pos = to_jolt (world_pos);
  const JPH::Quat rot = to_jolt (world_rot);

  JPH::BodyCreationSettings settings (
      shape_ref, pos, rot, phys::motion_type::Kinematic, (phys::object_layer)0);

  settings.mIsSensor = true;
  settings.mAllowDynamicOrKinematic = true;

  body_id = engine.get_body_interface ().CreateAndAddBody (
      settings, JPH::EActivation::Activate);

  engine.register_sensor (body_id);
}

void
area::apply_transform_to_body (phys::engine &engine) const
{
  if (body_id.IsInvalid ()) {
    return;
  }

  auto &bi = engine.get_body_interface ();

  // Read current body world position/rotation
  glm::vec3 const body_pos = to_glm (bi.GetPosition (body_id));
  glm::quat const body_rot = to_glm (bi.GetRotation (body_id));

  // Undo old offset to recover the transform's world rotation
  glm::quat const old_off_rot = (glm::quat)applied_rotation;
  glm::quat const xform_rot = body_rot * glm::inverse (old_off_rot);

  // Apply new offset: body = transform * offset
  glm::quat const new_off_rot = (glm::quat)rotation;
  glm::vec3 const offset_delta
      = (glm::vec3)position - (glm::vec3)applied_position;
  glm::vec3 const new_body_pos = body_pos + xform_rot * offset_delta;
  glm::quat const new_body_rot = xform_rot * new_off_rot;

  bi.SetPositionAndRotation (body_id, to_jolt (new_body_pos),
                             to_jolt (new_body_rot),
                             JPH::EActivation::Activate);
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

  if (body_id.IsInvalid ()) {
    sync_applied_cache ();
    return;
  }

  if (structural_change) {
    // Read current body position from Jolt to preserve world placement
    auto const &bi = engine->get_body_interface ();
    glm::vec3 const current_pos = to_glm (bi.GetPosition (body_id));
    glm::quat const current_rot = to_glm (bi.GetRotation (body_id));
    rebuild_body (*engine, current_pos, current_rot, scale);
  } else {
    if (xform_change) {
      apply_transform_to_body (*engine);
    }
  }

  sync_applied_cache ();
}

} // namespace comp
} // namespace wsl
