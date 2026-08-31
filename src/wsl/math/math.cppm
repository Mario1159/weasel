module;
// Global module fragment: everything the exported engine headers skip under
// WSL_MODULE_BUILD is provided textually here. Math depends on glm (values),
// Jolt (conversion operators), entt + ImGui + component_meta (inline
// register_meta/custom_inspect) and the STL — all global-module attached, so
// legacy TUs and module consumers share the same entities.
#include <entt/entt.hpp>
#include "../comp/component_meta.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#define WSL_MODULE_BUILD

export module wsl.math;

export {
#include "vector.hpp"
#include "matrix.hpp"
}
