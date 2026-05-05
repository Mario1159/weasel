#pragma once

#include <Jolt/Jolt.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace wsl
{

inline JPH::Vec3
to_jolt (const glm::vec3 &v)
{
  return JPH::Vec3 (v.x, v.y, v.z);
}

inline glm::vec3
to_glm (const JPH::Vec3 &v)
{
  return glm::vec3 (v.GetX (), v.GetY (), v.GetZ ());
}

inline JPH::Quat
to_jolt (const glm::quat &q)
{
  return JPH::Quat (q.x, q.y, q.z, q.w);
}

inline glm::quat
to_glm (const JPH::Quat &q)
{
  return glm::quat (q.GetW (), q.GetX (), q.GetY (), q.GetZ ());
}

} // namespace wsl
