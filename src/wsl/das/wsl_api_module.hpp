#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <string>

namespace das
{
class Module;
class ModuleGroup;
class Context;
struct LineInfoArg;
template <typename TT> struct TArray;
template <typename Result, typename... Args> struct TBlock;
}

namespace wsl
{
class engine_event;
}

namespace wsl::das
{

/** Entity handle returned to daslang scripts. */
struct Entity
{
  uint32_t id;
};

constexpr uint32_t NULL_ENTITY_ID = 0xFFFFFFFFu;

/** Registers the Weasel API module with the daslang engine. */
void register_wsl_api_module (::das::ModuleGroup &module_group);

::das::Module *get_wsl_api_module ();

::das::Module *create_worker_weasel_api_module ();

/** Sets the active registry for the Weasel API module. */
void wsl_api_set_active_registry (entt::registry *registry);

/** Sets the current event for the Weasel API module. */
void wsl_api_set_current_event (const engine_event *ev);

void wsl_log_info (const char *msg);
void wsl_log_debug (const char *msg);
void wsl_log_warn (const char *msg);
void wsl_log_error (const char *msg);

/**
 * Returns the stable type ID for a world component by display name.
 * :return: The type ID, or 0 if not found.
 */
uint32_t wsl_get_component_type_id (const char *display_name);

uint32_t wsl_get_active_camera ();
uint32_t wsl_get_event_kind ();

/** Iterates entities owning ALL of the listed component types and invokes `blk`
 *  with each matching entity id. `type_ids` are stable component type ids (see
 *  `_get_component_type_id_by_name`). Backs the daslang `query` macro.
 */
void each_entity_id_with (const ::das::TArray<uint32_t> &type_ids,
                          const ::das::TBlock<void, uint32_t> &blk,
                          ::das::Context *context, ::das::LineInfoArg *at);
float wsl_get_event_mouse_dx ();
float wsl_get_event_mouse_dy ();
bool wsl_has_component (uint32_t type_id, uint32_t entity);
bool wsl_set_relative_mouse_mode (bool enabled);

// ── Generic component type lookup ──

/**
 * Looks up component type ID by daScript type name.
 * :param type_name: The daScript-visible type name (e.g. "MouseRotate").
 * :return: The stable type ID, or 0 if not found.
 */
uint32_t wsl_get_component_type_id_by_name (const char *type_name);

/** Returns a pointer to an existing Daslang component payload, or nullptr. */
void *wsl_get_component_data (uint32_t entity, uint32_t type_id);

} // namespace wsl::das
