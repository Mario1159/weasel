#pragma once

#include <cereal/cereal.hpp>
#include <glm/glm.hpp>

namespace glm
{

template <class Archive>
void
serialize (Archive &archive, glm::vec2 &v)
{
  archive (cereal::make_nvp ("x", v.x), cereal::make_nvp ("y", v.y));
}

template <class Archive>
void
serialize (Archive &archive, glm::vec3 &v)
{
  archive (cereal::make_nvp ("x", v.x), cereal::make_nvp ("y", v.y),
           cereal::make_nvp ("z", v.z));
}

template <class Archive>
void
serialize (Archive &archive, glm::vec4 &v)
{
  archive (cereal::make_nvp ("x", v.x), cereal::make_nvp ("y", v.y),
           cereal::make_nvp ("z", v.z), cereal::make_nvp ("w", v.w));
}

template <class Archive>
void
serialize (Archive &archive, glm::mat4 &m)
{
  archive (cereal::make_nvp ("m00", m[0][0]), cereal::make_nvp ("m01", m[0][1]),
           cereal::make_nvp ("m02", m[0][2]), cereal::make_nvp ("m03", m[0][3]),
           cereal::make_nvp ("m10", m[1][0]), cereal::make_nvp ("m11", m[1][1]),
           cereal::make_nvp ("m12", m[1][2]), cereal::make_nvp ("m13", m[1][3]),
           cereal::make_nvp ("m20", m[2][0]), cereal::make_nvp ("m21", m[2][1]),
           cereal::make_nvp ("m22", m[2][2]), cereal::make_nvp ("m23", m[2][3]),
           cereal::make_nvp ("m30", m[3][0]), cereal::make_nvp ("m31", m[3][1]),
           cereal::make_nvp ("m32", m[3][2]),
           cereal::make_nvp ("m33", m[3][3]));
}

} // namespace glm
