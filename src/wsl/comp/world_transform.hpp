#pragma once

#include <entt/entt.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include "../rsc/cereal_glm.hpp"

#include "component_meta.hpp"

#include <cereal/cereal.hpp>


namespace wsl
{

namespace comp
{

struct world_transform : world_component
{
  glm::mat4 value{ 1.0F };

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::world_transform> ()
        .type (entt::type_hash<comp::world_transform>::value ())
        .custom<comp::meta_info> (meta_info{
            "World Transform", "Computed world-space transform (read-only)",
            "engine://icons/comp_world_transform.svg" })
        .data<&comp::world_transform::value> ("matrix"_hs)
        .custom<comp::meta_info> (meta_info{
            "Matrix", "Final world matrix after hierarchy evaluation" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("matrix", value));
  }
};

} // namespace comp

} // namespace wsl
