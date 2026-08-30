#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <glm/ext/vector_float3.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <glm/gtc/quaternion.hpp>
#endif

#if !defined(WSL_MODULE_BUILD)
#include <cereal/cereal.hpp>
#endif

namespace glm
{

template <class Archive>
void
serialize (Archive &ar, vec3 &v)
{
  ar (cereal::make_nvp ("x", v.x), cereal::make_nvp ("y", v.y),
      cereal::make_nvp ("z", v.z));
}

template <class Archive>
void
serialize (Archive &ar, quat &q)
{
  ar (cereal::make_nvp ("w", q.w), cereal::make_nvp ("x", q.x),
      cereal::make_nvp ("y", q.y), cereal::make_nvp ("z", q.z));
}

} // namespace glm
