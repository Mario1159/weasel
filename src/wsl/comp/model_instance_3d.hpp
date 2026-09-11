#pragma once

#include "../rsc/resource_manager.hpp"
#include "component_meta.hpp"

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif

namespace wsl
{

namespace comp
{

struct model_instance_3d : world_component
{
  rsc::model_id id{};
  uint32_t scene_index = 0;

  /**
   * Optional per-instance material override. When set (non-null) the
   * renderer uses this material instead of the model's own mesh materials.
   * Rendering hook is currently a stub (see scene_renderer::draw_command).
   */
  rsc::material_id material_override{};

  float mip_lod_bias = 0.0F;
  float geometry_lod_bias = 0.0F;
  float visibility_range = 0.0F;

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::model_instance_3d> ()
        .type (entt::type_hash<comp::model_instance_3d>::value ())
        .custom<comp::meta_info> (meta_info{
            "Model Instance", "Renders a 3D model using the current transform",
            "engine://icons/comp_model_instance.svg" })

        .data<&comp::model_instance_3d::id> ("model_id"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Model", "Model resource ID (hashed path)", "" })

        .data<&comp::model_instance_3d::scene_index> ("scene_index"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Scene Index", "Scene inside the model to render", "" })

        .data<&comp::model_instance_3d::material_override> (
            "material_override"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Material Override",
                       "Optional material assigned to this instance (None = "
                       "model default)",
                       "" })

        .data<&comp::model_instance_3d::mip_lod_bias> ("mip_lod_bias"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Mip LOD Bias",
                       "Texture sharpness bias (<0 sharper, >0 softer)", "" })

        .data<&comp::model_instance_3d::geometry_lod_bias> (
            "geometry_lod_bias"_hs)
        .custom<comp::meta_info> (meta_info{
            "Geometry LOD Bias",
            "Mesh LOD aggressiveness (<0 less aggressive, >0 more aggressive)",
            "" })

        .data<&comp::model_instance_3d::visibility_range> (
            "visibility_range"_hs)
        .custom<comp::meta_info> (meta_info{
            "Visibility Range",
            "Max draw distance in world units (0 = unlimited)", "" });
  }
};

} // namespace comp

} // namespace wsl
