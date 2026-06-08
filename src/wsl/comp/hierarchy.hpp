#pragma once

#include "component_meta.hpp"

#include <entt/entt.hpp>
#include <cereal/cereal.hpp>

namespace wsl
{

namespace comp
{

struct hierarchy : world_component
{
  entt::entity parent{ entt::null };
  entt::entity first{ entt::null }; // first child
  entt::entity next{ entt::null };  // next sibling

  static bool
  has_hierarchy (entt::registry &reg, entt::entity e)
  {
    return reg.all_of<comp::hierarchy> (e);
  }

  static comp::hierarchy &
  get_hierarchy (entt::registry &reg, entt::entity e)
  {
    return reg.get<comp::hierarchy> (e);
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::hierarchy> ()
        .type (entt::type_hash<comp::hierarchy>::value ())
        .custom<comp::meta_info> (meta_info{
            "Hierarchy", "Defines parent/child relationships between entities",
            "engine://icons/comp_hierarchy.svg" })
        .data<&comp::hierarchy::parent> ("parent"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Parent", "Parent entity (entt::null if root)", "" })
        .data<&comp::hierarchy::first> ("first"_hs)
        .custom<comp::meta_info> (
            meta_info{ "First Child", "First child entity in hierarchy", "" })
        .data<&comp::hierarchy::next> ("next"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Next Sibling", "Next sibling entity", "" });

    entt::meta_factory<entt::entity> ().type (
        entt::type_hash<entt::entity>::value ());
    entt::meta_factory<std::string> ()
        .type (entt::type_hash<std::string>::value ())
        .custom<comp::meta_info> (meta_info{ "String", "A String", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    hierarchy def{};
    serialize_field_if_diff (archive, "parent", parent, def.parent);
    serialize_field_if_diff (archive, "first", first, def.first);
    serialize_field_if_diff (archive, "next", next, def.next);
  }
};

} // namespace comp

} // namespace wsl
