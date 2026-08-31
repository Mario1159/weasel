module;
// Global module fragment: everything the exported engine headers skip under
// WSL_MODULE_BUILD is provided textually here. Math depends only on glm
// (values), Jolt (conversion operators) and the STL — no entt/imgui/serialization
// (those live in math_meta.cpp, a plain translation unit).
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#define WSL_MODULE_BUILD

export module wsl.math;

export {
#include "vector.hpp"
#include "matrix.hpp"
}
