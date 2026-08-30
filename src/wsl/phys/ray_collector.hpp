#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "physics_engine.hpp"
#endif

// clang-format off
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Jolt.h>
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Body/BodyID.h>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Collision/CastResult.h>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <Jolt/Physics/Collision/CollisionCollector.h>
#endif
#endif
// clang-format on

namespace wsl
{

namespace phys
{

class ray_collector
    : public JPH::CollisionCollector<JPH::RayCastResult,
                                     JPH::CollisionCollectorTraitsCastRay>
{
public:
  struct ray_result
  {
    std::vector<JPH::BodyID> bodies;
    std::vector<JPH::Vec3> hit_points;
    std::vector<float> distances;
  };

  void AddHit (const ResultType &result_type) override;
  ray_result cast_ray (phys::engine &engine, JPH::Vec3 pos,
                       const JPH::Vec3 &dir, float max_dist, size_t max_hits);

private:
  std::vector<JPH::RayCastResult> m_results;
  size_t m_max_hits;
};

} // namespace phys

} // namespace wsl
