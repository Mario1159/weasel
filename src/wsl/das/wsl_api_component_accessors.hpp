#pragma once

#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/directional_light.hpp"
#include "wsl/comp/point_light.hpp"
#include "wsl/comp/spot_light.hpp"
#include "wsl/comp/sprite_2d.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/transform_2d.hpp"
#include "wsl/math/vector.hpp"

#include <daScript/misc/vectypes.h>

#include <entt/entt.hpp>

#include <type_traits>

// The vector properties are exposed to daslang as references to the dasLang
// built-in float2/3/4 types, so the engine math types must be layout-identical
// to them (the getters reinterpret live component storage). These asserts keep
// that contract from silently breaking if either side's layout changes.

static_assert (sizeof (::wsl::math::vec2f) == sizeof (::das::float2)
                   && alignof (::wsl::math::vec2f) == alignof (::das::float2)
                   && std::is_standard_layout_v<::wsl::math::vec2f>
                   && std::is_trivially_copyable_v<::wsl::math::vec2f>,
               "::wsl::math::vec2f must be layout-identical to das::float2");

static_assert (sizeof (::wsl::math::vec3f) == sizeof (::das::float3)
                   && alignof (::wsl::math::vec3f) == alignof (::das::float3)
                   && std::is_standard_layout_v<::wsl::math::vec3f>
                   && std::is_trivially_copyable_v<::wsl::math::vec3f>,
               "::wsl::math::vec3f must be layout-identical to das::float3");

static_assert (sizeof (::wsl::math::vec4f) == sizeof (::das::float4)
                   && alignof (::wsl::math::vec4f) == alignof (::das::float4)
                   && std::is_standard_layout_v<::wsl::math::vec4f>
                   && std::is_trivially_copyable_v<::wsl::math::vec4f>,
               "::wsl::math::vec4f must be layout-identical to das::float4");

static_assert (sizeof (::wsl::math::quatf) == sizeof (::das::float4)
                   && alignof (::wsl::math::quatf) == alignof (::das::float4)
                   && std::is_standard_layout_v<::wsl::math::quatf>
                   && std::is_trivially_copyable_v<::wsl::math::quatf>,
               "::wsl::math::quatf must be layout-identical to das::float4");

namespace das
{
class Module;
class ModuleLibrary;
struct LineInfoArg;
}

namespace wsl::das
{

// The vector properties below are exposed to daslang as references to the
// dasLang built-in float2/3/4 types. math::vec2f/vec3f/vec4f and
// das::float2/3/4 are layout-identical (three/four consecutive floats), so a
// reinterpreted reference lets daslang read/write live component storage
// directly (chained writes like `t.scale.x = 2.0` and full-struct writes
// like `t.scale = float3(...)` both hit the component in place).

struct TransformProxy
{
  entt::entity entity;
  comp::transform *comp;

  bool valid () const;
  void bind (comp::transform *component);

  ::das::float3 &position ();
  ::das::float4 &rotation ();
  ::das::float3 &scale ();
};

struct Transform2DProxy
{
  entt::entity entity;
  comp::transform_2d *comp;

  bool valid () const;
  void bind (comp::transform_2d *component);

  ::das::float2 &position ();
  ::das::float2 &scale ();
  float &rotation ();
};

struct Camera2DProxy
{
  entt::entity entity;
  comp::camera_2d *comp;

  bool valid () const;
  void bind (comp::camera_2d *component);

  float &zoom ();
};

struct Sprite2DProxy
{
  entt::entity entity;
  comp::sprite_2d *comp;

  bool valid () const;
  void bind (comp::sprite_2d *component);

  ::das::float4 &color ();
  ::das::float2 &size ();
};

struct PointLightProxy
{
  entt::entity entity;
  comp::point_light *comp;

  bool valid () const;
  void bind (comp::point_light *component);

  ::das::float3 &color ();
  float &intensity ();
};

struct DirectionalLightProxy
{
  entt::entity entity;
  comp::directional_light *comp;

  bool valid () const;
  void bind (comp::directional_light *component);

  ::das::float3 &color ();
  float &intensity ();
};

struct SpotLightProxy
{
  entt::entity entity;
  comp::spot_light *comp;

  bool valid () const;
  void bind (comp::spot_light *component);

  ::das::float3 &color ();
  float &intensity ();
};

TransformProxy get_transform_accessor (uint32_t entity,
                                       ::das::LineInfoArg *at = nullptr);
Transform2DProxy get_transform_2d_accessor (uint32_t entity,
                                            ::das::LineInfoArg *at = nullptr);
Camera2DProxy get_camera_2d_accessor (uint32_t entity,
                                      ::das::LineInfoArg *at = nullptr);
Sprite2DProxy get_sprite_2d_accessor (uint32_t entity,
                                      ::das::LineInfoArg *at = nullptr);
PointLightProxy get_point_light_accessor (uint32_t entity,
                                          ::das::LineInfoArg *at = nullptr);
DirectionalLightProxy get_directional_light_accessor (uint32_t entity,
                                                      ::das::LineInfoArg *at
                                                      = nullptr);
SpotLightProxy get_spot_light_accessor (uint32_t entity,
                                        ::das::LineInfoArg *at = nullptr);

void register_component_accessors (::das::Module &mod,
                                   ::das::ModuleLibrary &lib);

} // namespace wsl::das
