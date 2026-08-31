module;
// Global module fragment: everything the exported phys headers skip under
// WSL_MODULE_BUILD, provided textually. Jolt is consumed through the
// jolt_all.hpp umbrella (the union of every Jolt include the phys API
// needs); its declarations stay attached to the global module.
#include "jolt_all.hpp"

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
#include "layers.hpp"
#include "broad_phase_layer_interface.hpp"
#include "object_layer_pair_filter.hpp"
#include "object_vs_broad_phase_layer_filter.hpp"
#include "character_query_filters.hpp"
#include "physics_engine.hpp"
#include "ray_collector.hpp"
#include "jolt_runtime.hpp"
#include "utils.hpp"
}
