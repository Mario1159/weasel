#include "subviewport.hpp"

#include <entt/meta/factory.hpp>

namespace wsl::comp
{

void
subviewport::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::subviewport> ()
      .type (entt::type_hash<comp::subviewport>::value ())
      .custom<comp::meta_info> (meta_info{
          "Sub-Viewport",
          "Defines a viewport sub-region in the entity hierarchy.", "" })
      .data<&subviewport::x> ("x"_hs)
      .custom<comp::meta_info> (
          meta_info{ "X", "Normalized left edge (0 = left).", "" })
      .data<&subviewport::y> ("y"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Y", "Normalized top edge (0 = top).", "" })
      .data<&subviewport::width> ("width"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Width", "Normalized width (1.0 = full).", "" })
      .data<&subviewport::height> ("height"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Height", "Normalized height (1.0 = full).", "" })
      .data<&subviewport::clear_color> ("clear_color"_hs)
      .custom<comp::meta_info> (meta_info{
          "Clear Color", "Clear the color target before rendering.", "" })
      .data<&subviewport::clear_depth> ("clear_depth"_hs)
      .custom<comp::meta_info> (meta_info{
          "Clear Depth", "Clear the depth target before rendering.", "" })
      .data<&subviewport::clear_r> ("clear_r"_hs)
      .custom<comp::meta_info> (meta_info{ "Clear R", "Red clear value.", "" })
      .data<&subviewport::clear_g> ("clear_g"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Clear G", "Green clear value.", "" })
      .data<&subviewport::clear_b> ("clear_b"_hs)
      .custom<comp::meta_info> (meta_info{ "Clear B", "Blue clear value.", "" })
      .data<&subviewport::clear_a> ("clear_a"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Clear A", "Alpha clear value.", "" });
}

} // namespace wsl::comp
