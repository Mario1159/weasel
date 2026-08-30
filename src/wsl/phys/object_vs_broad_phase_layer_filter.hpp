#pragma once

// clang-format on
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Jolt.h>
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#endif
#endif
// clang-format off


namespace wsl
{

class object_vs_broad_phase_layer_filter
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer in_layer1,
                             JPH::BroadPhaseLayer in_layer2) const override;   
};

    

} // namespace wsl
