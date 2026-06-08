#pragma once

#include "component_meta.hpp"
#include "../rsc/resource_ids.hpp"

#include <entt/entt.hpp>
#include <cereal/cereal.hpp>

namespace wsl
{

namespace comp
{

struct prefab_instance : world_component
{
  prefab_instance () = default;

  prefab_instance (rsc::scene_id prefab_id_value,
                   entt::entity prefab_entity_value)
      : prefab_id (prefab_id_value), prefab_entity (prefab_entity_value)
  {
  }

  rsc::scene_id prefab_id;
  entt::entity prefab_entity{ entt::null }; // Entity ID in the prefab scene

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::prefab_instance> ()
        .type (entt::type_hash<comp::prefab_instance>::value ())
        .custom<comp::meta_info> (meta_info{
            "Prefab Instance", "Tracks the source prefab for this entity", "" })
        .data<&comp::prefab_instance::prefab_id> ("prefab_id"_hs)
        .data<&comp::prefab_instance::prefab_entity> ("prefab_entity"_hs);
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    prefab_instance def{};
    serialize_field_if_diff (archive, "prefab_id", prefab_id.value,
                             def.prefab_id.value);
    serialize_field_if_diff (archive, "prefab_entity", prefab_entity,
                             def.prefab_entity);
  }
};

} // namespace comp

} // namespace wsl
