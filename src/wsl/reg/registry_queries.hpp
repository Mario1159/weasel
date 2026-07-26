#pragma once

#include "component_registry.hpp"
#include "singleton_registry.hpp"
#include "system_factory_registry.hpp"
#include "sig/signal_hub.hpp"

#include <entt/entt.hpp>
#include <vector>

namespace wsl::reg
{

/**
 * Centralized query interface for reasoning about the registered engine model.
 *
 * This class provides methods to query relationships between entities,
 * world components, singleton components, systems, signals, and handlers.
 */
class registry_queries
{
public:
  /** Constructs the query interface from the core registries. */
  registry_queries (component_registry &components,
                    system_factory_registry &systems, sig::signal_hub &hub)
      : m_components (components), m_systems (systems), m_hub (hub)
  {
  }

  // --- Entity Relation Contract ---

  /** Returns the world components that may still be added to an entity. */
  std::vector<const component_registry::descriptor *>
  get_addable_world_components (entt::registry &registry,
                                entt::entity entity) const;

  /** Returns every declared system iteration whose component contract matches the entity. */
  std::vector<const system_iteration_descriptor *>
  get_matching_iterations (entt::registry &registry, entt::entity entity) const;

  /** Returns every declared system that has at least one iteration or event handler relevant to the entity. */
  std::vector<const system_factory_registry::system_descriptor *>
  get_matching_systems (entt::registry &registry, entt::entity entity) const;

  /** Returns every signal related to the entity through source ownership, signature components, or handlers. */
  std::vector<const sig::signal_debug_entry *>
  get_related_signals (entt::registry &registry, entt::entity entity) const;

  /** Returns the explicit signal connections where the entity is either the signal source or the handler target. */
  std::vector<const sig::signal_connection_debug_entry *>
  get_entity_signal_connections (entt::entity entity) const;

  // --- Cross-Concept Query Contract ---

  /** Finds all signals owned by a specific system. */
  std::vector<const sig::signal_debug_entry *>
  find_signals_owned_by_system (entt::id_type system_type_id) const;

  /** Finds all event handlers owned by a specific system. */
  std::vector<const sig::system_handler_debug_entry *>
  find_event_handlers_owned_by_system (entt::id_type system_type_id) const;

  /** Finds all signals whose signature includes the queried world component. */
  std::vector<const sig::signal_debug_entry *>
  find_signals_using_world_component (entt::id_type component_type_id) const;

  /** Finds all event handlers whose component contract includes the queried world component. */
  std::vector<const sig::signal_connectable_handler_debug_entry *>
  find_event_handlers_using_world_component (entt::id_type component_type_id) const;

  /** Finds all systems that iterate over or handle events for the queried world component. */
  std::vector<const system_factory_registry::system_descriptor *>
  find_systems_using_world_component (entt::id_type component_type_id) const;

  /** Finds all explicit connections for a specific signal type. */
  std::vector<const sig::signal_connection_debug_entry *>
  find_connections_for_signal (entt::id_type signal_type_id) const;

  /** Finds all explicit connections involving a specific system as either owner or target. */
  std::vector<const sig::signal_connection_debug_entry *>
  find_connections_for_system (entt::id_type system_type_id) const;

  /** Finds all explicit connections whose signal or handler involves the queried world component. */
  std::vector<const sig::signal_connection_debug_entry *>
  find_connections_for_world_component (entt::id_type component_type_id) const;

private:
  component_registry &m_components;
  system_factory_registry &m_systems;
  sig::signal_hub &m_hub;
};

} // namespace wsl::reg
