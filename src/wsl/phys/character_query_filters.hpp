#pragma once

#include "layers.hpp"

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
// clang-format on


namespace wsl
{

namespace phys
{

//
// Broadphase filter used by CharacterVirtual
//
class character_broad_phase_filter final : public JPH::BroadPhaseLayerFilter
{
public:
  bool
  ShouldCollide (JPH::BroadPhaseLayer layer) const override
  {
    (void)layer;
    // Character should test against everything relevant
    return true;
  }
};

//
// Object-layer filter used by CharacterVirtual
//
class character_object_layer_filter final : public JPH::ObjectLayerFilter
{
public:
  bool
  ShouldCollide (JPH::ObjectLayer layer) const override
  {
    return layers::get_motion_bucket (layer) != layers::motion_bucket::character;
  }
};

//
// Ignore the character’s own body
//
class character_body_filter final : public JPH::BodyFilter
{
public:
  explicit character_body_filter (JPH::BodyID self) : m_self (self) {}

  bool
  ShouldCollide (const JPH::BodyID &id) const override
  {
    return id != m_self;
  }

private:
  JPH::BodyID m_self;
};

//
// Usually allow all shapes
//
class character_shape_filter final : public JPH::ShapeFilter
{
public:
  bool
  ShouldCollide (const JPH::Shape *shape,
                 const JPH::SubShapeID &id) const override
  {
    (void)shape;
    (void)id;
    return true;
  }
};

} // namespace phys

} // namespace wsl
