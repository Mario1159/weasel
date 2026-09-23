module;
// Global module fragment: everything the exported phys headers skip under
// WSL_MODULE_BUILD, provided textually. (Jolt was removed in the Box3D
// migration; only Box3D-backed headers remain.)

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#define WSL_MODULE_BUILD

export module wsl.phys;

export {
// Phys-internal includes are guarded behind WSL_MODULE_BUILD, so the
// purview order below must be topological.
// NOTE: headers removed in the Box3D migration (broad_phase_layer_interface,
// object_layer_pair_filter, object_vs_broad_phase_layer_filter,
// character_query_filters, ray_collector, jolt_runtime, utils) were
// pruned from this list; re-add successors here if new phys headers land.
#include "layers.hpp"
#include "physics_engine.hpp"
#include "box3d_adapter.hpp"
}
