#pragma once

// See wsl/math/vector.hpp for why EnTT is skipped inside a module build.
#if !defined(WSL_MODULE_BUILD)
#include "entt/entt.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <cstdint>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <string>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <vector>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <functional>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <unordered_map>
#endif

namespace wsl::event
{

using entity_match_predicate_t = bool (*) (entt::registry &, entt::entity);
// The handler is invoked with a `void *` owner so that the owner need not be
// an `ecs_system` (e.g. `runtime_context` / `ecs_inspector` can own event sinks
// for `scene_changed`). For `ecs_system` owners the pointer is the resolved
// `ecs_system *`; for other owners it is whatever instance pointer was captured
// at declaration time.
using handler_invoke_fn
    = void (*) (void *owner, entt::registry &, const void *);

// Forward declarations
struct event_source_debug_entry;
struct component_type_debug_entry;
struct system_handler_debug_entry;
struct system_iteration_debug_entry;
struct event_sink_debug_entry;
struct event_connection_debug_entry;
struct event_connection_data;
struct event_debug_db;
struct event_hub;

} // namespace wsl::event
