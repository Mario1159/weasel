#include "object_layer_pair_filter.hpp"
#include "layers.hpp"
#include <Jolt/Physics/Collision/ObjectLayer.h>


namespace wsl
{

bool
object_layer_pair_filter::ShouldCollide (JPH::ObjectLayer a,
                                         JPH::ObjectLayer b) const
{
  const phys::layers::motion_bucket motion_a
      = phys::layers::get_motion_bucket (a);
  const phys::layers::motion_bucket motion_b
      = phys::layers::get_motion_bucket (b);

  if (motion_a == phys::layers::motion_bucket::character
      || motion_b == phys::layers::motion_bucket::character) {
    return motion_a != phys::layers::motion_bucket::character
           || motion_b != phys::layers::motion_bucket::character;
  }

  if (motion_a == phys::layers::motion_bucket::static_body
      && motion_b == phys::layers::motion_bucket::static_body) {
    return false;
  }

  if (phys::layers::is_encoded_rigidbody (a)
      && phys::layers::is_encoded_rigidbody (b)) {
    return phys::layers::rigidbodies_can_collide (a, b);
  }

  return true;
}

} // namespace wsl
