#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>


namespace wsl
{

class object_layer_pair_filter : public JPH::ObjectLayerPairFilter
{
public:
  bool ShouldCollide (JPH::ObjectLayer a,
                              JPH::ObjectLayer b) const override;
};

} // namespace wsl
