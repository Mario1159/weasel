#pragma once

// clang-format on
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
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
