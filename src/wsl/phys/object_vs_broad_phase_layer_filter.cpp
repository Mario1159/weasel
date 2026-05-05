#include "object_vs_broad_phase_layer_filter.hpp"
#include "layers.hpp"
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>


namespace wsl
{

bool
object_vs_broad_phase_layer_filter::ShouldCollide (
    JPH::ObjectLayer in_layer1, JPH::BroadPhaseLayer in_layer2) const
{
  switch (phys::layers::get_motion_bucket (in_layer1)) {
  case phys::layers::motion_bucket::static_body:
    return in_layer2 == JPH::BroadPhaseLayer (1);
  case phys::layers::motion_bucket::moving_body:
  case phys::layers::motion_bucket::character:
    return true;
  default:
    JPH_ASSERT (false);
    return false;
  }
}

} // namespace wsl
