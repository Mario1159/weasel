#include "sprite_2d.hpp"
#include <entt/meta/factory.hpp>

namespace wsl::comp
{

void
sprite_2d::register_meta ()
{
  using namespace entt::literals;

  auto factory = reflect_type<sprite_2d> (stable_type_id<sprite_2d> (), "Sprite 2D",
                                         "Component for rendering a 2D sprite.", "");

  reflect_field<sprite_2d, &sprite_2d::image> (factory, "image", "Image");
  reflect_field<sprite_2d, &sprite_2d::size> (factory, "size", "Size");
  reflect_field<sprite_2d, &sprite_2d::color> (factory, "color", "Color");
  reflect_field<sprite_2d, &sprite_2d::uv_offset> (factory, "uv_offset", "UV Offset");
  reflect_field<sprite_2d, &sprite_2d::uv_scale> (factory, "uv_scale", "UV Scale");
  reflect_field<sprite_2d, &sprite_2d::flip_h> (factory, "flip_h", "Flip Horizontal");
  reflect_field<sprite_2d, &sprite_2d::flip_v> (factory, "flip_v", "Flip Vertical");
  reflect_field<sprite_2d, &sprite_2d::z_index> (factory, "z_index", "Z Index");
}

} // namespace wsl::comp
