// character_body.cpp
#include "character_body.hpp"

#include "../phys/layers.hpp"
#include "comp/singl/runtime_context.hpp"
#include "phys/physics_engine.hpp"

#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <algorithm> // std::max
#include <glm/ext/vector_float3.hpp>


namespace wsl
{

namespace comp
{

void
character_body::sanitize_dimensions (float &h, float &r) 
{
  // Make sure radius is valid
  r = std::max (r, min_radius);

  // Need: half_h = 0.5*h - r > 0  =>  h > 2*r
  // We enforce a strict margin so half_h >= min_half_height.
  const float min_h = 2.0F * (r + min_half_height);
  h = std::max (h, min_h);
}

float
character_body::capsule_half_height (float h, float r) 
{
  // After sanitize_dimensions this is guaranteed >= min_half_height,
  // but keep it robust anyway.
  const float half_h = (0.5F * h) - r;
  return std::max (min_half_height, half_h);
}

void
character_body::build_settings (JPH::CharacterVirtualSettings &settings) const
{
  settings.mUp = JPH::Vec3::sAxisY ();
  settings.mMaxSlopeAngle = JPH::DegreesToRadians (50.0F);
  settings.mMaxStrength = 100.0F;
  settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
  settings.mInnerBodyLayer = phys::layers::character;

  // Supporting volume plane at -radius (your convention)
  settings.mSupportingVolume = JPH::Plane (JPH::Vec3::sAxisY (), -radius);
}

character_body::character_body (phys::engine &physics,
                                const JPH::Vec3 &position, float h, float r)
{
  height = h;
  radius = r;
  sanitize_dimensions (height, radius);

  JPH::CharacterVirtualSettings settings;
  build_settings (settings);

  const float half_h = capsule_half_height (height, radius);
  JPH::Ref<JPH::CapsuleShape> const capsule = new JPH::CapsuleShape (half_h, radius);

  settings.mShape = capsule;
  settings.mInnerBodyShape = capsule;

  m_body = new JPH::CharacterVirtual (
      &settings, position, JPH::Quat::sIdentity (), &physics.get_system ());

  m_applied_height = height;
  m_applied_radius = radius;
}

void
character_body::create_body (phys::engine &physics, const JPH::Vec3 &position)
{
  destroy_body ();

  sanitize_dimensions (height, radius);

  JPH::CharacterVirtualSettings settings;
  build_settings (settings);

  const float half_h = capsule_half_height (height, radius);
  JPH::Ref<JPH::CapsuleShape> const capsule = new JPH::CapsuleShape (half_h, radius);

  settings.mShape = capsule;
  settings.mInnerBodyShape = capsule;

  m_body = new JPH::CharacterVirtual (
      &settings, position, JPH::Quat::sIdentity (), &physics.get_system ());

  m_applied_height = height;
  m_applied_radius = radius;
}

void
character_body::destroy_body ()
{
  m_body = nullptr;
}

void
character_body::recreate (phys::engine &physics, const JPH::Vec3 &position)
{
  create_body (physics, position);
}

void
character_body::on_inspector_changed (comp::singl::runtime_context *runtime,
                                      const glm::vec3 & /*unused*/)
{
  phys::engine *engine
      = (runtime != nullptr) ? runtime->try_get_active_physics_engine () : nullptr;
  if (engine == nullptr) {
    return;
}

  // If user typed invalid values (height <= 2*radius), fix them here.
  float new_h = height;
  float new_r = radius;
  sanitize_dimensions (new_h, new_r);

  // Write back sanitized values so UI shows the truth.
  height = new_h;
  radius = new_r;

  const bool changed = (height != m_applied_height) || (radius != m_applied_radius);
  if (!changed) {
    return;
}

  // Keep current world position if we already exist; otherwise origin.
  JPH::Vec3 pos = JPH::Vec3::sZero ();
  if (m_body != nullptr) {
    pos = m_body->GetPosition ();
  }

  recreate (*engine, pos);
}

} // namespace comp

} // namespace wsl
