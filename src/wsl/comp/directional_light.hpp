#pragma once

#include "../math/vector.hpp"
#include "component_meta.hpp"

#include <entt/entt.hpp>
#include <glm/ext/vector_float3.hpp>

#include <cereal/cereal.hpp>

namespace wsl
{

namespace comp
{

struct directional_light : world_component
{
  math::vec3f color = { 1, 1, 1 };
  float intensity = 1.0F;
  bool cast_shadows = true;

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::directional_light> ()
        .type (entt::type_hash<comp::directional_light>::value ())
        .custom<comp::meta_info> (meta_info{
            "Directional Light",
            "Emits parallel light rays in a single direction, like sunlight. "
            "Light affects all objects equally regardless of distance.",
            "" })
        .data<&comp::directional_light::color> ("color"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Color", "RGB color of the light.", "" })

        .data<&comp::directional_light::intensity> ("intensity"_hs)
        .custom<comp::meta_info> (meta_info{
            "Intensity", "Brightness multiplier of the light.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("color", color),
             cereal::make_nvp ("intensity", intensity));
  }
};

} // namespace comp

} // namespace wsl
