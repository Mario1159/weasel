#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "layers.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Jolt.h>
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#endif
#endif

#if !defined(WSL_MODULE_BUILD)
#include <cstdint>
#endif

namespace wsl
{

class broad_phase_layer_interface : public JPH::BroadPhaseLayerInterface
{
public:
  broad_phase_layer_interface ();

  uint32_t GetNumBroadPhaseLayers () const override;
  JPH::BroadPhaseLayer
  GetBroadPhaseLayer (JPH::ObjectLayer in_layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char *
  GetBroadPhaseLayerName (JPH::BroadPhaseLayer inLayer) const override;
#endif

private:
};

} // namespace wsl
