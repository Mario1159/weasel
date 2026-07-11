#include "registry_queries.hpp"

#include <algorithm>
#include <set>

namespace wsl::reg
{

std::vector<const component_registry::descriptor *>
registry_queries::get_addable_world_components (entt::registry &registry,
                                                entt::entity entity) const
{
  return m_components.get_addable_world_components (registry, entity);
}

std::vector<const system_iteration_descriptor *>
registry_queries::get_matching_iterations (entt::registry &registry,
                                           entt::entity entity) const
{
  return m_hub.get_matching_iterations (registry, entity);
}

std::vector<const system_factory_registry::system_descriptor *>
registry_queries::get_matching_systems (entt::registry &registry,
                                        entt::entity entity) const
{
  std::set<entt::id_type> matched_system_ids;
  std::vector<const system_factory_registry::system_descriptor *> result;

  // Check iterations
  for (const system_iteration_descriptor *iteration :
       get_matching_iterations (registry, entity)) {
    if (iteration != nullptr) {
      matched_system_ids.insert (iteration->system_type_id);
    }
  }

  // Check connectable handlers
  if (m_hub.db != nullptr) {
    for (const sig::signal_connectable_handler_debug_entry &handler :
         m_hub.db->connectable_handlers) {
      if (handler.matches_entity (registry, entity)) {
        matched_system_ids.insert (handler.system_type_id);
      }
    }
  }

  result.reserve (matched_system_ids.size ());
  for (entt::id_type system_id : matched_system_ids) {
    if (const system_factory_registry::system_descriptor *desc
        = m_systems.find_system (system_id)) {
      result.push_back (desc);
    }
  }

  return result;
}

std::vector<const sig::signal_debug_entry *>
registry_queries::get_related_signals (entt::registry &registry,
                                       entt::entity entity) const
{
  std::set<entt::id_type> related_signal_ids;
  std::vector<const sig::signal_debug_entry *> result;

  if (m_hub.db == nullptr) {
    return result;
  }

  // Related by source entity (owned signals)
  // Actually, we need to check if the entity can be a source for any signal.
  // This is hard to check without an instance.
  // But we can check signals related to components on the entity.
  for (auto [type_id, storage] : registry.storage ()) {
    if (storage.contains (entity)) {
      entt::id_type const stable_id
          = m_components.to_stable_world_component_id (type_id);
      for (const sig::signal_debug_entry *signal :
           m_hub.get_signals_for_component (stable_id)) {
        related_signal_ids.insert (signal->type_id);
      }
    }
  }

  // Related by explicit connections
  for (const sig::signal_connection_debug_entry &connection :
       m_hub.db->connections) {
    if (connection.source_entity == entity
        || connection.target_entity == entity) {
      related_signal_ids.insert (connection.signal_type_id);
    }
  }

  result.reserve (related_signal_ids.size ());
  for (entt::id_type signal_id : related_signal_ids) {
    if (std::unordered_map<entt::id_type,
                           sig::signal_debug_entry>::const_iterator const it
        = m_hub.db->entries.find (signal_id);
        it != m_hub.db->entries.end ()) {
      result.push_back (&it->second);
    }
  }

  return result;
}

std::vector<const sig::signal_connection_debug_entry *>
registry_queries::get_entity_signal_connections (entt::entity entity) const
{
  return m_hub.get_connections_for_entity (entity);
}

std::vector<const sig::signal_debug_entry *>
registry_queries::find_signals_owned_by_system (
    entt::id_type system_type_id) const
{
  return m_hub.get_signals_for_system (system_type_id);
}

std::vector<const sig::system_handler_debug_entry *>
registry_queries::find_event_handlers_owned_by_system (
    entt::id_type system_type_id) const
{
  std::vector<const sig::system_handler_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const sig::system_handler_debug_entry &handler :
       m_hub.db->system_handlers) {
    if (handler.system_type_id == system_type_id) {
      result.push_back (&handler);
    }
  }

  return result;
}

std::vector<const sig::signal_debug_entry *>
registry_queries::find_signals_using_world_component (
    entt::id_type component_type_id) const
{
  return m_hub.get_signals_for_component (component_type_id);
}

std::vector<const sig::signal_connectable_handler_debug_entry *>
registry_queries::find_event_handlers_using_world_component (
    entt::id_type component_type_id) const
{
  std::vector<const sig::signal_connectable_handler_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const sig::signal_connectable_handler_debug_entry &handler :
       m_hub.db->connectable_handlers) {
    if (handler.has_component (component_type_id)) {
      result.push_back (&handler);
    }
  }

  return result;
}

std::vector<const system_factory_registry::system_descriptor *>
registry_queries::find_systems_using_world_component (
    entt::id_type component_type_id) const
{
  std::set<entt::id_type> matched_system_ids;
  std::vector<const system_factory_registry::system_descriptor *> result;

  // Systems that iterate over the component
  for (const system_iteration_descriptor *iteration :
       m_systems.find_iterations_using_world_component (component_type_id)) {
    matched_system_ids.insert (iteration->system_type_id);
  }

  // Systems that handle signals carrying the component
  for (const sig::signal_connectable_handler_debug_entry *handler :
       find_event_handlers_using_world_component (component_type_id)) {
    matched_system_ids.insert (handler->system_type_id);
  }

  result.reserve (matched_system_ids.size ());
  for (entt::id_type system_id : matched_system_ids) {
    if (const system_factory_registry::system_descriptor *desc
        = m_systems.find_system (system_id)) {
      result.push_back (desc);
    }
  }

  return result;
}

std::vector<const sig::signal_connection_debug_entry *>
registry_queries::find_connections_for_signal (
    entt::id_type signal_type_id) const
{
  std::vector<const sig::signal_connection_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const sig::signal_connection_debug_entry &connection :
       m_hub.db->connections) {
    if (connection.signal_type_id == signal_type_id) {
      result.push_back (&connection);
    }
  }

  return result;
}

std::vector<const sig::signal_connection_debug_entry *>
registry_queries::find_connections_for_system (
    [[maybe_unused]] entt::id_type system_type_id) const
{
  std::vector<const sig::signal_connection_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const sig::signal_connection_debug_entry &connection :
       m_hub.db->connections) {
    // Check if the signal is owned by the system
    bool owner = false;
    if (std::unordered_map<entt::id_type,
                           sig::signal_debug_entry>::const_iterator const it
        = m_hub.db->entries.find (connection.signal_type_id);
        it != m_hub.db->entries.end ()) {
      if (it->second.owner_system_type_id == system_type_id) {
        owner = true;
      }
    }

    if (owner || connection.system_type_id == system_type_id) {
      result.push_back (&connection);
    }
  }

  return result;
}

std::vector<const sig::signal_connection_debug_entry *>
registry_queries::find_connections_for_world_component (
    [[maybe_unused]] entt::id_type component_type_id) const
{
  std::vector<const sig::signal_connection_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const sig::signal_connection_debug_entry &connection :
       m_hub.db->connections) {
    bool related = false;

    // Related by signal signature
    if (std::unordered_map<entt::id_type,
                           sig::signal_debug_entry>::const_iterator const it
        = m_hub.db->entries.find (connection.signal_type_id);
        it != m_hub.db->entries.end ()) {
      if (it->second.has_component (component_type_id)) {
        related = true;
      }
    }

    // Related by handler requirements
    if (!related) {
      for (const sig::signal_connectable_handler_debug_entry &handler :
           m_hub.db->connectable_handlers) {
        if (handler.signal_type_id == connection.signal_type_id
            && handler.system_type_id == connection.system_type_id
            && handler.handler_name == connection.handler_name) {
          if (handler.has_component (component_type_id)) {
            related = true;
            break;
          }
        }
      }
    }

    if (related) {
      result.push_back (&connection);
    }
  }

  return result;
}

} // namespace wsl::reg
