#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <string>

namespace das
{
class Module;
class ModuleGroup;
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

/**
 * Sets the delta time for the current frame (accessible from das via
 * get_delta_time()).
 */
void wsl_api_set_delta_time (double dt);

/** Returns the delta time for the current frame. */
float wsl_get_delta_time ();

void wsl_log_info (const char *msg);
void wsl_log_debug (const char *msg);
void wsl_log_warn (const char *msg);
void wsl_log_error (const char *msg);

/**
 * Returns the stable type ID for a world component by display name.
 * :return: The type ID, or 0 if not found.
 */
uint32_t wsl_get_component_type_id (const char *display_name);

/** Fills the entity buffer with entities that have the given component. */
void wsl_refresh_entities_with_component (uint32_t type_id);

/**
 * Reads a float field from a das component on an entity.
 *
 * :param entity: Entity ID.
 * :param type_id: Stable component type ID (from get_component_type_id).
 * :param offset: Byte offset of the field within the component.
 * :return: The float value, or 0.0f on error.
 */
float wsl_get_component_field_f (uint32_t entity, uint32_t type_id, int offset);

/**
 * Writes a float field to a das component on an entity.
 *
 * :param entity: Entity ID.
 * :param type_id: Stable component type ID (from get_component_type_id).
 * :param offset: Byte offset of the field within the component.
 * :param value: The value to write.
 */
void wsl_set_component_field_f (uint32_t entity, uint32_t type_id, int offset,
                                float value);

// ── Generic component type lookup (for get_component<T> / set_component<T>) ──

/**
 * Looks up component type ID by daScript type name.
 * :param type_name: The daScript-visible type name (e.g. "MouseRotate").
 * :return: The stable type ID, or 0 if not found.
 */
uint32_t wsl_get_component_type_id_by_name (const char *type_name);

/**
 * Looks up component kind by daScript type name.
 * :param type_name: The daScript-visible type name.
 * :return: The component kind (0=CPP_NATIVE, 1=CPP_PROJECT, 2=DAS_SCRIPT), or 0
 * if not found.
 */
int wsl_get_component_kind_by_name (const char *type_name);

/**
 * Looks up component struct size by daScript type name.
 * :param type_name: The daScript-visible type name.
 * :return: The struct size in bytes, or 0 if not found.
 */
int wsl_get_component_struct_size_by_name (const char *type_name);

/**
 * Type-erased component getter. Fills dest with the component data.
 * :param entity: Entity ID.
 * :param type_id: Stable component type ID.
 * :param kind: Component storage kind (0=CPP_NATIVE, 1=CPP_PROJECT,
 * 2=DAS_SCRIPT). :param dest: Destination buffer to receive the component data.
 */
void wsl_get_component_into (uint32_t entity, uint32_t type_id, int kind,
                             void *dest);

/**
 * Type-erased component setter. Writes src to component storage.
 * :param entity: Entity ID.
 * :param type_id: Stable component type ID.
 * :param kind: Component storage kind (0=CPP_NATIVE, 1=CPP_PROJECT,
 * 2=DAS_SCRIPT). :param src: Source buffer containing the component data.
 */
void wsl_set_component_from (uint32_t entity, uint32_t type_id, int kind,
                             const void *src);

} // namespace wsl::das
