#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Jolt.h>
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Collision/ObjectLayer.h>
#endif
#endif

namespace wsl
{

class object_layer_pair_filter : public JPH::ObjectLayerPairFilter
{
public:
  bool ShouldCollide (JPH::ObjectLayer a, JPH::ObjectLayer b) const override;
};

} // namespace wsl
