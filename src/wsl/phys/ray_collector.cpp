#include "ray_collector.hpp"
#include "Jolt/Physics/Collision/RayCast.h"
#include "phys/physics_engine.hpp"
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <algorithm>
#include <cstddef>


namespace wsl
{

void
phys::ray_collector::AddHit (const ResultType &result)
{
  m_results.push_back (result);

  if (m_results.size () >= m_max_hits) {
    ForceEarlyOut ();
}
}

phys::ray_collector::ray_result
phys::ray_collector::cast_ray (phys::engine &engine, const JPH::Vec3 pos,
                               const JPH::Vec3 &dir, float max_dist,
                               size_t max_hits)
{
  m_results.clear ();
  this->m_max_hits = max_hits;

  ray_result out{};

  JPH::RRayCast const ray (pos, dir * max_dist);

  const JPH::NarrowPhaseQuery &narrow = engine.get_narrow_phase_query ();

  narrow.CastRay (ray, {}, *this);

  // Sort by distance (fraction)
  std::sort (m_results.begin (), m_results.end (),
             [] (const JPH::RayCastResult &a, const JPH::RayCastResult &b) {
               return a.mFraction < b.mFraction;
             });

  for (const auto &r : m_results) {
    float const distance = r.mFraction * max_dist;

    out.bodies.push_back (r.mBodyID);
    out.distances.push_back (distance);
    out.hit_points.push_back (pos + dir * distance);
  }

  return out;
}

} // namespace wsl
