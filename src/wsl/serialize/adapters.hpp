#pragma once

// Implementation-only: reflect-cpp adapters for third-party and engine math
// types. Never include from engine headers (module purview) — .cpp files only.

#include <array>
#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "math/matrix.hpp"
#include "math/vector.hpp"

namespace rfl
{

// --- glm -----------------------------------------------------------------

template <> struct Reflector<glm::vec2>
{
  struct ReflType
  {
    float x;
    float y;
  };
  static glm::vec2
  to (const ReflType &v) noexcept
  {
    return { v.x, v.y };
  }
  static ReflType
  from (const glm::vec2 &v)
  {
    return { v.x, v.y };
  }
};

template <> struct Reflector<glm::vec3>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
  };
  static glm::vec3
  to (const ReflType &v) noexcept
  {
    return { v.x, v.y, v.z };
  }
  static ReflType
  from (const glm::vec3 &v)
  {
    return { v.x, v.y, v.z };
  }
};

template <> struct Reflector<glm::vec4>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
    float w;
  };
  static glm::vec4
  to (const ReflType &v) noexcept
  {
    return { v.x, v.y, v.z, v.w };
  }
  static ReflType
  from (const glm::vec4 &v)
  {
    return { v.x, v.y, v.z, v.w };
  }
};

template <> struct Reflector<glm::quat>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
    float w;
  };
  static glm::quat
  to (const ReflType &v) noexcept
  {
    return { v.w, v.x, v.y, v.z };
  }
  static ReflType
  from (const glm::quat &v)
  {
    return { v.x, v.y, v.z, v.w };
  }
};

template <> struct Reflector<glm::mat4>
{
  using ReflType = std::array<std::array<float, 4>, 4>;
  static glm::mat4
  to (const ReflType &v) noexcept
  {
    glm::mat4 m{};
    for (int c = 0; c < 4; ++c) {
      for (int r = 0; r < 4; ++r) {
        m[c][r] = v[c][r];
      }
    }
    return m;
  }
  static ReflType
  from (const glm::mat4 &m)
  {
    ReflType v{};
    for (int c = 0; c < 4; ++c) {
      for (int r = 0; r < 4; ++r) {
        v[c][r] = m[c][r];
      }
    }
    return v;
  }
};

// --- engine math ----------------------------------------------------------

template <> struct Reflector<wsl::math::vec2f>
{
  struct ReflType
  {
    float x;
    float y;
  };
  static wsl::math::vec2f
  to (const ReflType &v) noexcept
  {
    return wsl::math::vec2f{ v.x, v.y };
  }
  static ReflType
  from (const wsl::math::vec2f &v)
  {
    return { v.x (), v.y () };
  }
};

template <> struct Reflector<wsl::math::vec3f>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
  };
  static wsl::math::vec3f
  to (const ReflType &v) noexcept
  {
    return wsl::math::vec3f{ v.x, v.y, v.z };
  }
  static ReflType
  from (const wsl::math::vec3f &v)
  {
    return { v.x (), v.y (), v.z () };
  }
};

template <> struct Reflector<wsl::math::vec4f>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
    float w;
  };
  static wsl::math::vec4f
  to (const ReflType &v) noexcept
  {
    return wsl::math::vec4f{ v.x, v.y, v.z, v.w };
  }
  static ReflType
  from (const wsl::math::vec4f &v)
  {
    return { v.x (), v.y (), v.z (), v.w () };
  }
};

template <> struct Reflector<wsl::math::quatf>
{
  struct ReflType
  {
    float x;
    float y;
    float z;
    float w;
  };
  static wsl::math::quatf
  to (const ReflType &v) noexcept
  {
    return wsl::math::quatf{ v.x, v.y, v.z, v.w };
  }
  static ReflType
  from (const wsl::math::quatf &v)
  {
    return { v.x (), v.y (), v.z (), v.w () };
  }
};

template <> struct Reflector<wsl::math::mat44f>
{
  using ReflType = std::array<float, 16>;
  static wsl::math::mat44f
  to (const ReflType &v) noexcept
  {
    wsl::math::mat44f m{};
    for (int i = 0; i < 16; ++i) {
      m.data ()[i] = v[i];
    }
    return m;
  }
  static ReflType
  from (const wsl::math::mat44f &m)
  {
    ReflType v{};
    for (int i = 0; i < 16; ++i) {
      v[i] = m.data ()[i];
    }
    return v;
  }
};

template <> struct Reflector<wsl::math::mat33f>
{
  using ReflType = std::array<float, 9>;
  static wsl::math::mat33f
  to (const ReflType &v) noexcept
  {
    wsl::math::mat33f m{};
    for (int i = 0; i < 9; ++i) {
      m.data ()[i] = v[i];
    }
    return m;
  }
  static ReflType
  from (const wsl::math::mat33f &m)
  {
    ReflType v{};
    for (int i = 0; i < 9; ++i) {
      v[i] = m.data ()[i];
    }
    return v;
  }
};

// --- entt -----------------------------------------------------------------

template <> struct Reflector<entt::entity>
{
  using ReflType = std::uint32_t;
  static entt::entity
  to (const ReflType &v) noexcept
  {
    return static_cast<entt::entity> (v);
  }
  static ReflType
  from (const entt::entity &v)
  {
    return static_cast<std::uint32_t> (entt::to_integral (v));
  }
};

} // namespace rfl
