#include "transform_2d.hpp"
#include <entt/meta/factory.hpp>

namespace wsl::comp
{

void
transform_2d::register_meta ()
{
  using namespace entt::literals;

  auto factory = reflect_type<transform_2d> (
      stable_type_id<transform_2d> (), "Transform 2D",
      "2D local transform (position, rotation, scale).", "");

  reflect_field<transform_2d, &transform_2d::position> (factory, "position",
                                                        "Position");
  reflect_field<transform_2d, &transform_2d::rotation> (factory, "rotation",
                                                        "Rotation");
  reflect_field<transform_2d, &transform_2d::scale> (factory, "scale", "Scale");
  reflect_field<transform_2d, &transform_2d::pivot> (factory, "pivot", "Pivot");
}

} // namespace wsl::comp
