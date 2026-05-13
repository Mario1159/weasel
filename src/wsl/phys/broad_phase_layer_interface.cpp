#include "broad_phase_layer_interface.hpp"
#include "phys/layers.hpp"
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <sys/types.h>


namespace wsl
{

broad_phase_layer_interface::broad_phase_layer_interface ()
{
}

uint32_t
broad_phase_layer_interface::GetNumBroadPhaseLayers () const
{
  return 2;
}

JPH::BroadPhaseLayer
broad_phase_layer_interface::GetBroadPhaseLayer (JPH::ObjectLayer in_layer) const
{
  const phys::layers::motion_bucket motion
      = phys::layers::get_motion_bucket (in_layer);
  return motion == phys::layers::motion_bucket::static_body
             ? JPH::BroadPhaseLayer (0)
             : JPH::BroadPhaseLayer (1);
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char *
broad_phase_layer_interface::GetBroadPhaseLayerName (
    JPH::BroadPhaseLayer inLayer) const
{
  return inLayer == JPH::BroadPhaseLayer (0) ? "Static" : "Moving";
}
#endif

} // namespace wsl
