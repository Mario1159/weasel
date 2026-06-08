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
            meta_info{ "Scene Index", "Scene inside the model to render", "" });
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
      serialize_field_if_diff (archive, "scene_index", scene_index,
                               def.scene_index);
    } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
      std::string path;
      scene_index = def.scene_index;
      try {
        archive (make_nvp ("model_path", path));
      } catch (const std::exception &) {
      }
      try {
        serialize_field_if_diff (archive, "scene_index", scene_index,
                                 def.scene_index);
      } catch (const std::exception &) {
      }

      if (path != "None" && !path.empty () && mgr) {
        id = mgr->register_model (path);
      } else {
        id.value = entt::null;
      }
    } else {
      std::string path = "None";
      if (mgr && id.value != entt::null) {
        path = mgr->get_resource_path (id);
      }
      archive (make_nvp ("model_path", path),
               make_nvp ("scene_index", scene_index));
      if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase,
                                      Archive>) {
        if (path != "None" && !path.empty () && mgr) {
          id = mgr->register_model (path);
        } else {
          id.value = entt::null;
        }
      }
    }
  }
};

} // namespace comp

} // namespace wsl
