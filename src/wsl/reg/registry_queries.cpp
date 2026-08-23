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

  for (const system_iteration_descriptor *iteration :
       get_matching_iterations (registry, entity)) {
    if (iteration != nullptr) {
      matched_system_ids.insert (iteration->system_type_id);
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

std::vector<const event::event_hub::registered_event_source *>
registry_queries::find_event_sources_owned_by_system (
    entt::id_type system_type_id) const
{
  std::vector<const event::event_hub::registered_event_source *> result;
  for (const event::event_hub::registered_event_source &source :
       m_hub.registered_signal_sources) {
    if (source.owner_system_type_id == system_type_id) {
      result.push_back (&source);
    }
  }
  return result;
}

std::vector<const event::system_handler_debug_entry *>
registry_queries::find_event_handlers_owned_by_system (
    entt::id_type system_type_id) const
{
  std::vector<const event::system_handler_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const event::system_handler_debug_entry &handler :
       m_hub.db->system_handlers) {
    if (handler.system_type_id == system_type_id) {
      result.push_back (&handler);
    }
  }

  return result;
}

std::vector<const event::event_hub::registered_event_sink *>
registry_queries::find_event_sinks_owned_by_system (
    entt::id_type system_type_id) const
{
  std::vector<const event::event_hub::registered_event_sink *> result;
  for (const event::event_hub::registered_event_sink &handler :
       m_hub.registered_event_sinks) {
    if (handler.system_type_id == system_type_id) {
      result.push_back (&handler);
    }
  }
  return result;
}

std::vector<const event::event_connection_debug_entry *>
registry_queries::find_connections_for_event (entt::id_type event_type_id) const
{
  std::vector<const event::event_connection_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const event::event_connection_debug_entry &connection :
       m_hub.db->connections) {
    if (connection.event_type_id == event_type_id) {
      result.push_back (&connection);
    }
  }

  return result;
}

std::vector<const event::event_connection_debug_entry *>
registry_queries::find_connections_for_system (
    entt::id_type system_type_id) const
{
  std::vector<const event::event_connection_debug_entry *> result;
  if (m_hub.db == nullptr) {
    return result;
  }

  for (const event::event_connection_debug_entry &connection :
       m_hub.db->connections) {
    bool owner = false;
    if (std::unordered_map<entt::id_type, event::event_source_debug_entry>::
            const_iterator const it
        = m_hub.db->entries.find (connection.event_type_id);
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

std::vector<const system_factory_registry::system_descriptor *>
registry_queries::find_systems_using_world_component (
    entt::id_type component_type_id) const
{
  std::set<entt::id_type> matched_system_ids;
  std::vector<const system_factory_registry::system_descriptor *> result;

  for (const system_iteration_descriptor *iteration :
       m_systems.find_iterations_using_world_component (component_type_id)) {
    matched_system_ids.insert (iteration->system_type_id);
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

} // namespace wsl::reg
