#include "camera_2d.hpp"
#include <entt/meta/factory.hpp>

namespace wsl::comp
{

void
camera_2d::register_meta ()
{
  using namespace entt::literals;

  auto factory = reflect_type<camera_2d> (
      stable_type_id<camera_2d> (), "Camera 2D", "2D orthographic camera.", "");

  reflect_field<camera_2d, &camera_2d::zoom> (factory, "zoom", "Zoom");
  reflect_field<camera_2d, &camera_2d::use_window_as_viewport> (
      factory, "use_window_as_viewport", "Use Window As Viewport");
  reflect_field<camera_2d, &camera_2d::viewport_offset> (
      factory, "viewport_offset", "Viewport Offset");
  reflect_field<camera_2d, &camera_2d::viewport_size> (factory, "viewport_size",
                                                       "Viewport Size");
  reflect_field<camera_2d, &camera_2d::layer> (factory, "layer", "Layer");
  reflect_field<camera_2d, &camera_2d::only_for_editor> (
      factory, "only_for_editor", "Only For Editor");
}

} // namespace wsl::comp
