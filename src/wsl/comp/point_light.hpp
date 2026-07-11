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

struct point_light : world_component
{
  math::vec3f color = { 1, 1, 1 };
  float intensity = 1.0F;
  float radius = 10.0F;

  bool cast_shadows = false;
  float shadow_far = 35.0F;
  float shadow_bias = 0.06F;

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::point_light> ()
        .type (entt::type_hash<comp::point_light>::value ())
        .custom<comp::meta_info> (meta_info{
            "Point Light",
            "Emits light equally in all directions from the entity position, "
            "like a light bulb. Uses the entity transform for position.",
            "" })

        .data<&comp::point_light::color> ("color"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Color", "RGB color of the emitted light.", "" })

        .data<&comp::point_light::intensity> ("intensity"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Intensity", "Brightness multiplier of the light.", "" })

        .data<&comp::point_light::radius> ("radius"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Radius",
                       "Maximum distance the light affects objects. "
                       "Light intensity fades to zero at this distance.",
                       "" })
        .data<&comp::point_light::cast_shadows> ("cast_shadows"_hs)
        .custom<comp::meta_info> (meta_info{
            "Cast Shadows",
            "Enable cubemap shadow rendering for this point light.", "" })

        .data<&comp::point_light::shadow_far> ("shadow_far"_hs)
        .custom<comp::meta_info> (meta_info{
            "Shadow Far",
            "Maximum distance stored in the point shadow cubemap.", "" })

        .data<&comp::point_light::shadow_bias> ("shadow_bias"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Shadow Bias",
                       "Bias used to reduce self-shadowing artifacts.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    point_light const def{};
    serialize_field_if_diff (archive, "color", color, def.color);
    serialize_field_if_diff (archive, "intensity", intensity, def.intensity);
    serialize_field_if_diff (archive, "radius", radius, def.radius);
    serialize_field_if_diff (archive, "cast_shadows", cast_shadows,
                             def.cast_shadows);
    serialize_field_if_diff (archive, "shadow_far", shadow_far, def.shadow_far);
    serialize_field_if_diff (archive, "shadow_bias", shadow_bias,
                             def.shadow_bias);
  }
};

} // namespace comp

} // namespace wsl
