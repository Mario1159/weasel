module;

#define WSL_MODULE_BUILD

import "thirdparty/thirdparty_all.hpp";
import "phys/jolt_all.hpp";
import "das/daScript_all.hpp";

export module wsl.phys;

export {
#include "layers.hpp"
#include "broad_phase_layer_interface.hpp"
#include "object_layer_pair_filter.hpp"
#include "object_vs_broad_phase_layer_filter.hpp"
#include "character_query_filters.hpp"
#include "jolt_runtime.hpp"
#include "physics_engine.hpp"
#include "ray_collector.hpp"
#include "utils.hpp"
}
