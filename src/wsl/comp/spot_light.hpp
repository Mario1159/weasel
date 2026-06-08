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

struct spot_light : world_component
{
  math::vec3f color = { 1, 1, 1 };
  float intensity = 1.0F;
  float inner_cos = 0.9F;
  float outer_cos = 0.8F;
  float range = 25.0F;
  bool cast_shadows = true;

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::spot_light> ()
        .type (entt::type_hash<comp::spot_light>::value ())
        .custom<comp::meta_info> (meta_info{
            "Spot Light",
            "Emits light in a cone shape from the entity position. "
            "Uses the entity transform for position and orientation.",
            "" })
        .data<&comp::spot_light::color> ("color"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Color", "RGB color of the emitted light.", "" })

        .data<&comp::spot_light::intensity> ("intensity"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Intensity", "Brightness multiplier of the light.", "" })

        .data<&comp::spot_light::inner_cos> ("inner_cos"_hs)
        .custom<comp::meta_info> (meta_info{
            "Inner Cone (cos)",
            "Cosine of the inner cone angle. Inside this cone the light "
            "has full intensity.",
            "" })

        .data<&comp::spot_light::outer_cos> ("outer_cos"_hs)
        .custom<comp::meta_info> (meta_info{
            "Outer Cone (cos)",
            "Cosine of the outer cone angle. Outside this cone the light "
            "has no effect. Between inner and outer the light fades "
            "smoothly.",
            "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    spot_light def{};
    serialize_field_if_diff (archive, "color", color, def.color);
    serialize_field_if_diff (archive, "intensity", intensity, def.intensity);
    serialize_field_if_diff (archive, "inner_cos", inner_cos, def.inner_cos);
    serialize_field_if_diff (archive, "outer_cos", outer_cos, def.outer_cos);
  }
};

} // namespace comp

} // namespace wsl
