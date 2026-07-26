#pragma once

#include "../rsc/resource_manager.hpp"
#include "component_meta.hpp"
#include "singl/runtime_context.hpp"
#include <entt/entt.hpp>

#include <cereal/cereal.hpp>

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

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    using namespace cereal;
    auto *mgr = rsc::resource_manager::serialization_context::get ();
    model_instance_3d def{};

    if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
      std::string path = "None";
      if (mgr && id.value != entt::null) {
        path = mgr->get_resource_path (id);
      }
      if (path != "None") {
        archive (make_nvp ("model_path", path));
      }

      std::string mat_path = "None";
      if (mgr && material_override.value != entt::null) {
        mat_path = mgr->get_resource_path (material_override);
      }
      if (mat_path != "None") {
        archive (make_nvp ("material_path", mat_path));
      }

      serialize_field_if_diff (archive, "scene_index", scene_index,
                               def.scene_index);
      serialize_field_if_diff (archive, "mip_lod_bias", mip_lod_bias,
                               def.mip_lod_bias);
      serialize_field_if_diff (archive, "geometry_lod_bias", geometry_lod_bias,
                               def.geometry_lod_bias);
      serialize_field_if_diff (archive, "visibility_range", visibility_range,
                               def.visibility_range);
    } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
      std::string path;
      std::string mat_path;
      scene_index = def.scene_index;
      try {
        archive (make_nvp ("model_path", path));
      } catch (const std::exception &) {
      }
      try {
        archive (make_nvp ("material_path", mat_path));
      } catch (const std::exception &) {
      }
      try {
        serialize_field_if_diff (archive, "scene_index", scene_index,
                                 def.scene_index);
      } catch (const std::exception &) {
      }
      try {
        serialize_field_if_diff (archive, "mip_lod_bias", mip_lod_bias,
                                 def.mip_lod_bias);
      } catch (const std::exception &) {
      }
      try {
        serialize_field_if_diff (archive, "geometry_lod_bias",
                                 geometry_lod_bias, def.geometry_lod_bias);
      } catch (const std::exception &) {
      }
      try {
        serialize_field_if_diff (archive, "visibility_range", visibility_range,
                                 def.visibility_range);
      } catch (const std::exception &) {
      }

      if (path != "None" && !path.empty () && mgr) {
        id = mgr->register_model (path);
      } else {
        id.value = entt::null;
      }

      if (mat_path != "None" && !mat_path.empty () && mgr) {
        material_override = mgr->register_material (mat_path);
      } else {
        material_override.value = entt::null;
      }
    } else {
      std::string path = "None";
      if (mgr && id.value != entt::null) {
        path = mgr->get_resource_path (id);
      }
      std::string mat_path = "None";
      if (mgr && material_override.value != entt::null) {
        mat_path = mgr->get_resource_path (material_override);
      }
      archive (make_nvp ("model_path", path),
               make_nvp ("material_path", mat_path),
               make_nvp ("scene_index", scene_index),
               make_nvp ("mip_lod_bias", mip_lod_bias),
               make_nvp ("geometry_lod_bias", geometry_lod_bias),
               make_nvp ("visibility_range", visibility_range));
      if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase,
                                      Archive>) {
        if (path != "None" && !path.empty () && mgr) {
          id = mgr->register_model (path);
        } else {
          id.value = entt::null;
        }
        if (mat_path != "None" && !mat_path.empty () && mgr) {
          material_override = mgr->register_material (mat_path);
        } else {
          material_override.value = entt::null;
        }
      }
    }
  }
};

} // namespace comp

} // namespace wsl
