#pragma once

#include "../../rsc/resource_manager.hpp"
#include "../component_meta.hpp"

#include <entt/entt.hpp>

#include <cereal/cereal.hpp>

namespace wsl
{

namespace comp
{

namespace singl
{

class skybox_instance_3d
{
public:
  // rsc::resource_manager::cubemap_handle cubemap;
  rsc::cubemap_id id{};

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::singl::skybox_instance_3d> ()
        .type (entt::type_hash<comp::singl::skybox_instance_3d>::value ())
        .custom<comp::meta_info> (
            meta_info{ "Skybox", "Global skybox cubemap", "" })
        .data<&comp::singl::skybox_instance_3d::id> ("cubemap_id"_hs)
        .custom<comp::meta_info> (meta_info{ "Cubemap", "Skybox texture", "" });

    entt::meta_factory<rsc::model_id> ().type (
        entt::type_hash<rsc::model_id>::value ());

    entt::meta_factory<rsc::cubemap_id> ().type (
        entt::type_hash<rsc::cubemap_id>::value ());
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    skybox_instance_3d def{};
    serialize_field_if_diff (archive, "cubemap_id", id.value, def.id.value);
  }
};

} // namespace singl

} // namespace comp

} // namespace wsl
