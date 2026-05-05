#pragma once

#include "entt/entt.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace wsl::sys { class ecs_system; }

namespace wsl::reg::sig
{

using entity_match_predicate_t = bool (*)(entt::registry &, entt::entity);
using signal_source_entity_fn = entt::entity (*)(const void *);
using handler_invoke_fn = void (*)(::wsl::sys::ecs_system &, entt::registry &,
                                      entt::entity, const void *);

// Forward declarations
struct signal_debug_entry;
struct component_type_debug_entry;
struct system_handler_debug_entry;
struct system_iteration_debug_entry;
struct signal_connectable_handler_debug_entry;
struct signal_connection_debug_entry;
struct signal_connection_data;
struct signal_debug_db;
struct signal_hub;

} // namespace wsl::reg::sig
