#pragma once

#include "layers.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>


namespace wsl
{

class broad_phase_layer_interface : public JPH::BroadPhaseLayerInterface
{
public:
  broad_phase_layer_interface ();

  uint GetNumBroadPhaseLayers () const override;
  JPH::BroadPhaseLayer
  GetBroadPhaseLayer (JPH::ObjectLayer in_layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char *
  GetBroadPhaseLayerName (JPH::BroadPhaseLayer inLayer) const override;
#endif

private:
};

} // namespace wsl
