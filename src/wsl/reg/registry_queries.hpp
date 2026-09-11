#pragma once

#include "component_registry.hpp"
#include "singleton_registry.hpp"
#include "system_factory_registry.hpp"
#include "event/event_hub.hpp"

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl::reg
{

/**
 * Centralized query interface for reasoning about the registered engine model.
 *
 * This class provides methods to query relationships between entities,
 * world components, singleton components, systems, events, and handlers.
 *
 * Events are type-level: they have no entity or component relation, so the
 * event query interface reasons purely about event types, system ownership,
 * and explicit connections. Iterations retain an entity/component contract.
 */
class registry_queries
{
public:
  /** Constructs the query interface from the core registries. */
  registry_queries (component_registry &components,
                    system_factory_registry &systems, event::event_hub &hub)
      : m_components (components), m_systems (systems), m_hub (hub)
  {
  }

  // --- Entity Relation Contract (iterations only) ---

  /** Returns the world components that may still be added to an entity. */
  std::vector<const component_registry::descriptor *>
  get_addable_world_components (entt::registry &registry,
                                entt::entity entity) const;

  /** Returns every declared system iteration whose component contract matches
   * the entity. */
  std::vector<const system_iteration_descriptor *>
  get_matching_iterations (entt::registry &registry, entt::entity entity) const;

  /** Returns every declared system that has at least one iteration matching the
   * entity. */
  std::vector<const system_factory_registry::system_descriptor *>
  get_matching_systems (entt::registry &registry, entt::entity entity) const;

  // --- Event Contract (type-level) ---

  /** Finds all event sources owned by a specific system. */
  std::vector<const event::event_hub::registered_event_source *>
  find_event_sources_owned_by_system (entt::id_type system_type_id) const;

  /** Finds all non-connectable event handlers owned by a specific system. */
  std::vector<const event::system_handler_debug_entry *>
  find_event_handlers_owned_by_system (entt::id_type system_type_id) const;

  /** Finds all event sinks (connectable handlers) owned by a specific system.
   */
  std::vector<const event::event_hub::registered_event_sink *>
  find_event_sinks_owned_by_system (entt::id_type system_type_id) const;

  /** Finds all explicit connections for a specific event type. */
  std::vector<const event::event_connection_debug_entry *>
  find_connections_for_event (entt::id_type event_type_id) const;

  /** Finds all explicit connections involving a specific system as owner or
   * sink. */
  std::vector<const event::event_connection_debug_entry *>
  find_connections_for_system (entt::id_type system_type_id) const;

  // --- Cross-Concept Query Contract ---

  /** Finds all systems that iterate over the queried world component. */
  std::vector<const system_factory_registry::system_descriptor *>
  find_systems_using_world_component (entt::id_type component_type_id) const;

private:
  component_registry &m_components;
  system_factory_registry &m_systems;
  event::event_hub &m_hub;
};

} // namespace wsl::reg
