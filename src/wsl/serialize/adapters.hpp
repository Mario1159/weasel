#pragma once

// Phase B — rfl adapters for glm, entt, and engine math types
// Replaces rsc/cereal_glm.hpp and rsc/data_types_serialization.hpp
// Each glm type is represented as a helper struct with from_class/to_class
// so rfl can reflect it (glm::vec has anonymous union, not directly
// reflectable).

#ifndef IN_MODULE_INTERFACE
#include <rfl.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/Field.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/parsing/Parser.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <rfl/parsing/CustomParser.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/gtc/quaternion.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include "../math/vector.hpp"
#endif
#ifndef IN_MODULE_INTERFACE
#include "../math/matrix.hpp"
#endif

#ifndef IN_MODULE_INTERFACE
#include <array>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstdint>
#endif

namespace wsl::serialize
{

// Helper structs — rfl reflects these, then CustomParser converts to glm
struct Vec2Helper
{
  float x = 0, y = 0;
  static Vec2Helper
  from_class (const glm::vec2 &v)
  {
    return { v.x, v.y };
  }
  glm::vec2
  to_class () const
  {
    return { x, y };
  }
};

struct Vec3Helper
{
  float x = 0, y = 0, z = 0;
  static Vec3Helper
  from_class (const glm::vec3 &v)
  {
    return { v.x, v.y, v.z };
  }
  glm::vec3
  to_class () const
  {
    return { x, y, z };
  }
};

struct Vec4Helper
{
  float x = 0, y = 0, z = 0, w = 0;
  static Vec4Helper
  from_class (const glm::vec4 &v)
  {
    return { v.x, v.y, v.z, v.w };
  }
  glm::vec4
  to_class () const
  {
    return { x, y, z, w };
  }
};

struct QuatHelper
{
  float w = 1, x = 0, y = 0, z = 0;
  static QuatHelper
  from_class (const glm::quat &q)
  {
    return { q.w, q.x, q.y, q.z };
  }
  glm::quat
  to_class () const
  {
    return { w, x, y, z };
  }
};

struct Mat4Helper
{
  // Column-major 4x4 as flat array for rfl (16 floats)
  std::array<float, 16> m{};
  static Mat4Helper
  from_class (const glm::mat4 &mat)
  {
    Mat4Helper h;
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r)
        h.m[c * 4 + r] = mat[c][r];
    return h;
  }
  glm::mat4
  to_class () const
  {
    glm::mat4 mat (1.0f);
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r)
        mat[c][r] = m[c * 4 + r];
    return mat;
  }
};

// entt::entity as uint32_t (stable id)
struct EntityHelper
{
  std::uint32_t id = 0;
  static EntityHelper
  from_class (const entt::entity &e)
  {
    return { static_cast<std::uint32_t> (e) };
  }
  entt::entity
  to_class () const
  {
    return entt::entity{ id };
  }
};

// Engine math helpers — wsl::math::quatf/vec* have private members and
// cannot be reflected directly via structured binding.
struct QuatfHelper
{
  float w = 1, x = 0, y = 0, z = 0;
  static QuatfHelper
  from_class (const wsl::math::quatf &q)
  {
    return { q.w (), q.x (), q.y (), q.z () };
  }
  wsl::math::quatf
  to_class () const
  {
    return wsl::math::quatf{ x, y, z, w };
  }
};

struct Vec2fHelper
{
  float x = 0, y = 0;
  static Vec2fHelper
  from_class (const wsl::math::vec2f &v)
  {
    return { v.x (), v.y () };
  }
  wsl::math::vec2f
  to_class () const
  {
    return wsl::math::vec2f{ x, y };
  }
};

struct Vec3fHelper
{
  float x = 0, y = 0, z = 0;
  static Vec3fHelper
  from_class (const wsl::math::vec3f &v)
  {
    return { v.x (), v.y (), v.z () };
  }
  wsl::math::vec3f
  to_class () const
  {
    return wsl::math::vec3f{ x, y, z };
  }
};

struct Vec4fHelper
{
  float x = 0, y = 0, z = 0, w = 0;
  static Vec4fHelper
  from_class (const wsl::math::vec4f &v)
  {
    return { v.x (), v.y (), v.z (), v.w () };
  }
  wsl::math::vec4f
  to_class () const
  {
    return wsl::math::vec4f{ x, y, z, w };
  }
};

struct Mat44fHelper
{
  std::array<float, 16> m{};
  static Mat44fHelper
  from_class (const wsl::math::mat44f &mat)
  {
    Mat44fHelper h;
    const float *src = mat.data ();
    for (int i = 0; i < 16; ++i)
      h.m[i] = src[i];
    return h;
  }
  wsl::math::mat44f
  to_class () const
  {
    wsl::math::mat44f out;
    for (int i = 0; i < 16; ++i)
      out.data ()[i] = m[i];
    return out;
  }
};

} // namespace wsl::serialize

// rfl custom parsers — tell rfl to use HelperStruct for OriginalClass
namespace rfl::parsing
{
template <class R, class W, class ProcessorsType>
struct Parser<R, W, glm::vec2, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, glm::vec2,
                          wsl::serialize::Vec2Helper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, glm::vec3, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, glm::vec3,
                          wsl::serialize::Vec3Helper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, glm::vec4, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, glm::vec4,
                          wsl::serialize::Vec4Helper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, glm::quat, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, glm::quat,
                          wsl::serialize::QuatHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, glm::mat4, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, glm::mat4,
                          wsl::serialize::Mat4Helper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, entt::entity, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, entt::entity,
                          wsl::serialize::EntityHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, wsl::math::quatf, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, wsl::math::quatf,
                          wsl::serialize::QuatfHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, wsl::math::vec2f, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, wsl::math::vec2f,
                          wsl::serialize::Vec2fHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, wsl::math::vec3f, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, wsl::math::vec3f,
                          wsl::serialize::Vec3fHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, wsl::math::vec4f, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, wsl::math::vec4f,
                          wsl::serialize::Vec4fHelper>
{
};

template <class R, class W, class ProcessorsType>
struct Parser<R, W, wsl::math::mat44f, ProcessorsType>
    : public CustomParser<R, W, ProcessorsType, wsl::math::mat44f,
                          wsl::serialize::Mat44fHelper>
{
};
} // namespace rfl::parsing
