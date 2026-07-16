#pragma once

#include <entt/entt.hpp>
#include <cstdint>

namespace das
{
class ModuleGroup;
}

namespace wsl
{
class engine_event;
}

namespace wsl::das
{

/*!
 * \brief Entity handle returned to daslang scripts.
 *
 * A thin wrapper around a raw entity ID.  Used internally in C++ but
 * exposed to daslang as a plain `uint` — the daslang `Entity` struct
 * wraps this value with `null_entity()` / `entity_is_null()` helpers.
 */
struct Entity
{
  uint32_t id;
};

constexpr uint32_t NULL_ENTITY_ID = 0xFFFFFFFFu;

/*!
 * \brief Registers the Weasel API module with the daslang engine.
 *
 * This module exposes ECS operations (entity/component management),
 * transform access, scene queries, event access, and SDL window
 * operations to daslang scripts.
 */
void register_wsl_api_module (::das::ModuleGroup &module_group);

/*!
 * \brief Sets the active registry for the Weasel API module.
 *
 * Must be called before invoking any Weasel API functions from daslang.
 */
void wsl_api_set_active_registry (entt::registry *registry);

/*!
 * \brief Sets the current event for the Weasel API module.
 *
 * Must be called before invoking on_event from daslang so that
 * event query functions return valid data.  Pass nullptr to clear.
 */
void wsl_api_set_current_event (const engine_event *ev);

} // namespace wsl::das
