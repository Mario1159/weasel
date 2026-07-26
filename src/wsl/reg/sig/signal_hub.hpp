#pragma once

#include "../../comp/component_meta.hpp"
#include "signal_hub_fwd.hpp"

#include <algorithm>
#include <ranges>

#include <cereal/cereal.hpp>

namespace wsl::reg::sig
{

/**
 * Describes a related component type used by signal and iteration
 * metadata.
 */
struct component_type_debug_entry
{
  entt::id_type type_id{};
  std::string type_name;
};

template <typename... Components>
inline std::vector<component_type_debug_entry>
make_component_type_debug_entries ()
{
  std::vector<component_type_debug_entry> entries;
  entries.reserve (sizeof...(Components));

  (entries.push_back (component_type_debug_entry{
       comp::stable_type_id<Components> (),
       std::string (entt::type_name<Components> ().value ()) }),
   ...);

  return entries;
}

inline bool
contains_component_type (
    const std::vector<component_type_debug_entry> &component_types,
    entt::id_type component_type_id)
{
  return std::ranges::any_of (
      component_types,
      [component_type_id] (const component_type_debug_entry &entry) {
        return entry.type_id == component_type_id;
      });
}

template <typename... Components>
inline bool
matches_component_set (entt::registry &registry, entt::entity entity)
{
  if (!registry.valid (entity)) {
    return false;
  }

  if constexpr (sizeof...(Components) == 0) {
    return true;
  }

  return registry.all_of<Components...> (entity);
}

template <typename... Components>
inline entity_match_predicate_t
make_entity_match_predicate ()
{
  if constexpr (sizeof...(Components) == 0) {
    return nullptr;
  }

  return +[] (entt::registry &registry, entt::entity entity) -> bool {
    return matches_component_set<Components...> (registry, entity);
  };
}

/** Debug and editor metadata for a declared signal. */
struct signal_debug_entry
{
  entt::id_type type_id{};
  std::string type_name;
  std::size_t listener_count = 0;
  std::size_t emit_count = 0;
  entt::id_type owner_system_type_id{};
  std::string owner_system_type_name;
  std::vector<component_type_debug_entry> component_types;

  bool
  has_component (entt::id_type component_type_id) const
  {
    return contains_component_type (component_types, component_type_id);
  }
};

/** Debug metadata for a declared event handler. */
struct system_handler_debug_entry
{
  entt::id_type system_type_id{};
  std::string system_type_name;
  std::string handler_name;
};

/** Debug metadata for a declared system iteration. */
struct system_iteration_debug_entry
{
  entt::id_type system_type_id{};
  std::string system_type_name;
  std::string iteration_name;
  std::vector<component_type_debug_entry> component_types;
  entity_match_predicate_t entity_matches = nullptr;

  bool
  matches_entity (entt::registry &registry, entt::entity entity) const
  {
    if (entity_matches != nullptr) {
      return entity_matches (registry, entity);
    }

    return true;
  }

  bool
  has_component (entt::id_type component_type_id) const
  {
    return contains_component_type (component_types, component_type_id);
  }
};

/** Debug metadata for a connectable signal handler. */
struct signal_connectable_handler_debug_entry
{
  entt::id_type signal_type_id{};
  std::string signal_type_name;
  entt::id_type system_type_id{};
  std::string system_type_name;
  std::string handler_name;
  std::vector<component_type_debug_entry> component_types;
  entity_match_predicate_t entity_matches = nullptr;

  bool
  matches_entity (entt::registry &registry, entt::entity entity) const
  {
    if (entity_matches != nullptr) {
      return entity_matches (registry, entity);
    }

    return true;
  }

  bool
  has_component (entt::id_type component_type_id) const
  {
    return contains_component_type (component_types, component_type_id);
  }
};

/** Debug metadata for an explicit signal-to-handler connection. */
struct signal_connection_debug_entry
{
  entt::id_type signal_type_id{};
  std::string signal_type_name;
  entt::id_type system_type_id{};
  std::string system_type_name;
  std::string handler_name;
  entt::entity source_entity{ entt::null };
  entt::entity target_entity{ entt::null };
};

/** Serializable data for an explicit signal connection. */
struct signal_connection_data
{
  entt::id_type signal_type_id{};
  entt::id_type system_type_id{};
  std::string handler_name;
  entt::entity source_entity{ entt::null };
  entt::entity target_entity{ entt::null };

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (signal_type_id, system_type_id, handler_name, source_entity,
             target_entity);
  }
};

/** Editor/debug database that mirrors declared signals and connections. */
struct signal_debug_db
{
  std::unordered_map<entt::id_type, signal_debug_entry> entries;
  std::vector<system_handler_debug_entry> system_handlers;
  std::vector<system_iteration_debug_entry> system_iterations;
  std::vector<signal_connectable_handler_debug_entry> connectable_handlers;
  std::vector<signal_connection_debug_entry> connections;

  void
  note_listener (entt::id_type type_id, std::string_view type_name)
  {
    signal_debug_entry &entry = entries[type_id];
    entry.type_id = type_id;
    if (entry.type_name.empty ()) {
      entry.type_name = std::string (type_name);
    }
    entry.listener_count++;
  }

  void
  note_emit (entt::id_type type_id, std::string_view type_name)
  {
    signal_debug_entry &entry = entries[type_id];
    entry.type_id = type_id;
    if (entry.type_name.empty ()) {
      entry.type_name = std::string (type_name);
    }
    entry.emit_count++;
  }

  template <typename Signal, typename OwnerSystem, typename... Components>
  void
  declare_signal ()
  {
    const entt::id_type signal_type_id = comp::stable_type_id<Signal> ();
    signal_debug_entry &entry = entries[signal_type_id];
    entry.type_id = signal_type_id;
    entry.type_name = std::string (entt::type_name<Signal> ().value ());
    entry.owner_system_type_id = comp::stable_type_id<OwnerSystem> ();
    entry.owner_system_type_name
        = std::string (entt::type_name<OwnerSystem> ().value ());
    entry.component_types = make_component_type_debug_entries<Components...> ();
  }

  template <typename OwnerSystem>
  void
  declare_handler (const char *handler_name)
  {
    const entt::id_type system_type_id = comp::stable_type_id<OwnerSystem> ();
    const std::string name = handler_name != nullptr ? handler_name : "";

    std::erase_if (
        system_handlers,
        [system_type_id, &name] (const system_handler_debug_entry &entry) {
          return entry.system_type_id == system_type_id
                 && entry.handler_name == name;
        });

    system_handlers.push_back (
        { system_type_id,
          std::string (entt::type_name<OwnerSystem> ().value ()), name });
  }

  template <typename OwnerSystem, typename... Components>
  void
  declare_iteration (const char *iteration_name)
  {
    static_assert ((comp::world_component_type<Components> && ...),
                   "System iteration declarations may only reference "
                   "wsl::comp::world_component types.");

    const entt::id_type system_type_id = comp::stable_type_id<OwnerSystem> ();
    const std::string name = iteration_name != nullptr ? iteration_name : "";

    std::erase_if (
        system_iterations,
        [system_type_id, &name] (const system_iteration_debug_entry &entry) {
          return entry.system_type_id == system_type_id
                 && entry.iteration_name == name;
        });

    system_iteration_debug_entry entry;
    entry.system_type_id = system_type_id;
    entry.system_type_name
        = std::string (entt::type_name<OwnerSystem> ().value ());
    entry.iteration_name = name;
    entry.component_types = make_component_type_debug_entries<Components...> ();
    entry.entity_matches = make_entity_match_predicate<Components...> ();
    system_iterations.push_back (std::move (entry));
  }

  template <typename Signal, typename OwnerSystem, typename... Components>
  void
  declare_connectable_handler (const char *handler_name)
  {
    static_assert ((comp::world_component_type<Components> && ...),
                   "Connectable handler declarations may only reference "
                   "wsl::comp::world_component types.");

    const entt::id_type signal_type_id = comp::stable_type_id<Signal> ();
    const entt::id_type system_type_id = comp::stable_type_id<OwnerSystem> ();
    const std::string name = handler_name != nullptr ? handler_name : "";

    std::erase_if (connectable_handlers,
                   [signal_type_id, system_type_id, &name] (
                       const signal_connectable_handler_debug_entry &entry) {
                     return entry.signal_type_id == signal_type_id
                            && entry.system_type_id == system_type_id
                            && entry.handler_name == name;
                   });

    signal_connectable_handler_debug_entry entry;
    entry.signal_type_id = signal_type_id;
    entry.signal_type_name = std::string (entt::type_name<Signal> ().value ());
    entry.system_type_id = system_type_id;
    entry.system_type_name
        = std::string (entt::type_name<OwnerSystem> ().value ());
    entry.handler_name = name;
    entry.component_types = make_component_type_debug_entries<Components...> ();
    entry.entity_matches = make_entity_match_predicate<Components...> ();
    connectable_handlers.push_back (std::move (entry));
  }

  void
  rebuild_listener_counts ()
  {
    for (auto &[_, entry] : entries) {
      entry.listener_count = 0;
    }

    for (const signal_connection_debug_entry &connection : connections) {
      note_listener (connection.signal_type_id, connection.signal_type_name);
    }
  }

  void
  note_connection (entt::id_type signal_type_id, const char *signal_type_name,
                   entt::id_type system_type_id, const char *system_type_name,
                   const char *handler_name, entt::entity source_entity,
                   entt::entity target_entity)
  {
    connections.push_back (
        { signal_type_id, signal_type_name != nullptr ? signal_type_name : "",
          system_type_id, system_type_name != nullptr ? system_type_name : "",
          handler_name != nullptr ? handler_name : "", source_entity,
          target_entity });
    rebuild_listener_counts ();
  }

  void
  clear_connections ()
  {
    connections.clear ();
    rebuild_listener_counts ();
  }

  void
  clear_system_declarations (entt::id_type system_type_id)
  {
    std::erase_if (system_handlers,
                   [system_type_id] (const system_handler_debug_entry &entry) {
                     return entry.system_type_id == system_type_id;
                   });

    std::erase_if (
        system_iterations,
        [system_type_id] (const system_iteration_debug_entry &entry) {
          return entry.system_type_id == system_type_id;
        });

    std::erase_if (
        connectable_handlers,
        [system_type_id] (const signal_connectable_handler_debug_entry &entry) {
          return entry.system_type_id == system_type_id;
        });

    for (auto it = entries.begin (); it != entries.end ();) {
      if (it->second.owner_system_type_id == system_type_id) {
        it = entries.erase (it);
      } else {
        ++it;
      }
    }

    rebuild_listener_counts ();
  }
};

/** Runtime signal declaration and explicit connection hub. */
struct signal_hub
{
  struct registered_connectable_handler
  {
    entt::id_type signal_type_id{};
    std::string signal_type_name;
    entt::id_type system_type_id{};
    std::string system_type_name;
    std::string handler_name;
    std::vector<component_type_debug_entry> component_types;
    entity_match_predicate_t entity_matches = nullptr;
    handler_invoke_fn invoke = nullptr;
  };

  struct registered_signal_source
  {
    entt::id_type signal_type_id{};
    std::string signal_type_name;
    entt::id_type owner_system_type_id{};
    std::string owner_system_type_name;
    std::vector<component_type_debug_entry> component_types;
    signal_source_entity_fn extract_source_entity = nullptr;
  };

  struct connected_handler
  {
    entt::id_type signal_type_id{};
    std::string signal_type_name;
    entt::id_type system_type_id{};
    std::string system_type_name;
    std::string handler_name;
    entt::entity source_entity{ entt::null };
    entt::entity target_entity{ entt::null };
    entity_match_predicate_t entity_matches = nullptr;
    handler_invoke_fn invoke = nullptr;
  };

  std::vector<registered_signal_source> registered_signal_sources;
  std::vector<registered_connectable_handler> registered_connectable_handlers;
  std::vector<connected_handler> connected_handlers;

  entt::dispatcher *dispatcher = nullptr;
  signal_debug_db *db = nullptr;
  std::function<entt::registry *()> resolve_active_registry;
  std::function<::wsl::sys::ecs_system *(entt::id_type)> resolve_system_by_type;

  signal_hub () = default;

  signal_hub (entt::dispatcher &dispatcher_ref, signal_debug_db &db_ref)
      : dispatcher (&dispatcher_ref), db (&db_ref)
  {
  }

  void
  clear_connections ()
  {
    connected_handlers.clear ();
    if (db != nullptr) {
      db->clear_connections ();
    }
  }

  void
  clear_system_declarations (entt::id_type system_type_id)
  {
    std::erase_if (registered_signal_sources,
                   [system_type_id] (const registered_signal_source &entry) {
                     return entry.owner_system_type_id == system_type_id;
                   });

    std::erase_if (
        registered_connectable_handlers,
        [system_type_id] (const registered_connectable_handler &entry) {
          return entry.system_type_id == system_type_id;
        });

    if (db != nullptr) {
      db->clear_system_declarations (system_type_id);
    }
  }

  template <typename OwnerSystem>
  void
  clear_system_declarations ()
  {
    clear_system_declarations (comp::stable_type_id<OwnerSystem> ());
  }

  std::vector<signal_connection_data>
  get_all_connections () const
  {
    std::vector<signal_connection_data> result;
    result.reserve (connected_handlers.size ());

    for (const connected_handler &handler : connected_handlers) {
      result.push_back ({ handler.signal_type_id, handler.system_type_id,
                          handler.handler_name, handler.source_entity,
                          handler.target_entity });
    }

    return result;
  }

  std::vector<const signal_connection_debug_entry *>
  get_connections_for_entity (entt::entity entity) const
  {
    std::vector<const signal_connection_debug_entry *> result;
    if (db == nullptr) {
      return result;
    }

    for (const signal_connection_debug_entry &connection : db->connections) {
      if (connection.source_entity == entity
          || connection.target_entity == entity) {
        result.push_back (&connection);
      }
    }

    return result;
  }

  std::vector<const signal_debug_entry *>
  get_signals_for_system (entt::id_type system_type_id) const
  {
    std::vector<const signal_debug_entry *> result;
    if (db == nullptr) {
      return result;
    }

    result.reserve (db->entries.size ());
    for (const auto &[_, entry] : db->entries) {
      if (entry.owner_system_type_id == system_type_id) {
        result.push_back (&entry);
      }
    }

    std::sort (
        result.begin (), result.end (),
        [] (const signal_debug_entry *lhs, const signal_debug_entry *rhs) {
          return lhs->type_name < rhs->type_name;
        });
    return result;
  }

  std::vector<const signal_debug_entry *>
  get_signals_for_component (entt::id_type component_type_id) const
  {
    std::vector<const signal_debug_entry *> result;
    if (db == nullptr) {
      return result;
    }

    for (const auto &[signal_type_id, entry] : db->entries) {
      bool related = entry.has_component (component_type_id);

      if (!related) {
        for (const signal_connectable_handler_debug_entry &handler :
             db->connectable_handlers) {
          if (handler.signal_type_id == signal_type_id
              && handler.has_component (component_type_id)) {
            related = true;
            break;
          }
        }
      }

      if (related) {
        result.push_back (&entry);
      }
    }

    std::sort (
        result.begin (), result.end (),
        [] (const signal_debug_entry *lhs, const signal_debug_entry *rhs) {
          return lhs->type_name < rhs->type_name;
        });
    return result;
  }

  std::vector<const system_iteration_debug_entry *>
  get_matching_iterations (entt::registry &registry, entt::entity entity) const
  {
    std::vector<const system_iteration_debug_entry *> result;
    if (db == nullptr) {
      return result;
    }

    for (const system_iteration_debug_entry &iteration :
         db->system_iterations) {
      if (iteration.matches_entity (registry, entity)) {
        result.push_back (&iteration);
      }
    }

    return result;
  }

  std::vector<const signal_connectable_handler_debug_entry *>
  get_matching_connectable_handlers (entt::registry &registry,
                                     entt::entity entity) const
  {
    std::vector<const signal_connectable_handler_debug_entry *> result;
    if (db == nullptr) {
      return result;
    }

    for (const signal_connectable_handler_debug_entry &handler :
         db->connectable_handlers) {
      if (handler.matches_entity (registry, entity)) {
        result.push_back (&handler);
      }
    }

    return result;
  }

  template <typename Signal>
  void
  note_listener ()
  {
    if (db != nullptr) {
      db->note_listener (comp::stable_type_id<Signal> (),
                         entt::type_name<Signal> ().value ());
    }
  }

  template <typename Signal>
  void
  note_emit ()
  {
    if (db != nullptr) {
      db->note_emit (comp::stable_type_id<Signal> (),
                     entt::type_name<Signal> ().value ());
    }
  }

  template <typename Signal, typename OwnerSystem, typename... Components>
  void
  declare_signal (signal_source_entity_fn extract_source_entity = nullptr)
  {
    static_assert ((comp::world_component_type<Components> && ...),
                   "Signal declarations may only reference "
                   "wsl::comp::world_component types.");

    const entt::id_type signal_type_id = comp::stable_type_id<Signal> ();

    if (db != nullptr) {
      db->template declare_signal<Signal, OwnerSystem, Components...> ();
    }

    std::erase_if (registered_signal_sources,
                   [signal_type_id] (const registered_signal_source &entry) {
                     return entry.signal_type_id == signal_type_id;
                   });

    registered_signal_source source;
    source.signal_type_id = signal_type_id;
    source.signal_type_name = std::string (entt::type_name<Signal> ().value ());
    source.owner_system_type_id = comp::stable_type_id<OwnerSystem> ();
    source.owner_system_type_name
        = std::string (entt::type_name<OwnerSystem> ().value ());
    source.component_types
        = make_component_type_debug_entries<Components...> ();
    source.extract_source_entity = extract_source_entity;
    registered_signal_sources.push_back (std::move (source));
  }

  template <typename OwnerSystem>
  void
  declare_handler (const char *handler_name)
  {
    if (db != nullptr) {
      db->template declare_handler<OwnerSystem> (handler_name);
    }
  }

  template <typename OwnerSystem, typename... Components>
  void
  declare_iteration (const char *iteration_name)
  {
    if (db != nullptr) {
      db->template declare_iteration<OwnerSystem, Components...> (
          iteration_name);
    }
  }

  template <typename Signal, typename OwnerSystem, typename... Components>
  void
  declare_connectable_handler (const char *handler_name,
                               handler_invoke_fn invoke)
  {
    static_assert ((comp::world_component_type<Components> && ...),
                   "Connectable handler declarations may only reference "
                   "wsl::comp::world_component types.");

    const entt::id_type signal_type_id = comp::stable_type_id<Signal> ();
    const entt::id_type system_type_id = comp::stable_type_id<OwnerSystem> ();
    const std::string name = handler_name != nullptr ? handler_name : "";

    if (db != nullptr) {
      db->template declare_connectable_handler<Signal, OwnerSystem,
                                               Components...> (handler_name);
    }

    std::erase_if (registered_connectable_handlers,
                   [signal_type_id, system_type_id,
                    &name] (const registered_connectable_handler &entry) {
                     return entry.signal_type_id == signal_type_id
                            && entry.system_type_id == system_type_id
                            && entry.handler_name == name;
                   });

    registered_connectable_handler handler;
    handler.signal_type_id = signal_type_id;
    handler.signal_type_name
        = std::string (entt::type_name<Signal> ().value ());
    handler.system_type_id = system_type_id;
    handler.system_type_name
        = std::string (entt::type_name<OwnerSystem> ().value ());
    handler.handler_name = name;
    handler.component_types
        = make_component_type_debug_entries<Components...> ();
    handler.entity_matches = make_entity_match_predicate<Components...> ();
    handler.invoke = invoke;
    registered_connectable_handlers.push_back (std::move (handler));
  }

  bool
  has_signal_source (entt::id_type signal_type_id) const
  {
    return std::ranges::any_of (
        registered_signal_sources,
        [signal_type_id] (const registered_signal_source &source) {
          return source.signal_type_id == signal_type_id;
        });
  }

  bool
  connect (entt::id_type signal_type_id, entt::id_type system_type_id,
           const std::string &handler_name, entt::entity source_entity,
           entt::entity target_entity,
           entt::registry *provided_registry = nullptr)
  {
    entt::registry *registry = provided_registry;
    if (registry == nullptr && resolve_active_registry) {
      registry = resolve_active_registry ();
    }

    const registered_connectable_handler *registered_handler = nullptr;
    for (const registered_connectable_handler &handler :
         registered_connectable_handlers) {
      if (handler.signal_type_id == signal_type_id
          && handler.system_type_id == system_type_id
          && handler.handler_name == handler_name) {
        registered_handler = &handler;
        break;
      }
    }

    if (registered_handler == nullptr) {
      return false;
    }

    if (source_entity != entt::null && !has_signal_source (signal_type_id)) {
      return false;
    }

    if (registry != nullptr && target_entity != entt::null) {
      if (!registry->valid (target_entity)) {
        return false;
      }

      if (registered_handler->entity_matches != nullptr
          && !registered_handler->entity_matches (*registry, target_entity)) {
        return false;
      }
    }

    for (const connected_handler &connection : connected_handlers) {
      if (connection.signal_type_id == signal_type_id
          && connection.system_type_id == system_type_id
          && connection.handler_name == handler_name
          && connection.source_entity == source_entity
          && connection.target_entity == target_entity) {
        return false;
      }
    }

    connected_handler connection;
    connection.signal_type_id = signal_type_id;
    connection.signal_type_name = registered_handler->signal_type_name;
    connection.system_type_id = system_type_id;
    connection.system_type_name = registered_handler->system_type_name;
    connection.handler_name = handler_name;
    connection.source_entity = source_entity;
    connection.target_entity = target_entity;
    connection.entity_matches = registered_handler->entity_matches;
    connection.invoke = registered_handler->invoke;
    connected_handlers.push_back (connection);

    if (db != nullptr) {
      db->note_connection (
          signal_type_id, registered_handler->signal_type_name.c_str (),
          system_type_id, registered_handler->system_type_name.c_str (),
          handler_name.c_str (), source_entity, target_entity);
    }

    return true;
  }

  bool
  disconnect (entt::id_type signal_type_id, entt::id_type system_type_id,
              const std::string &handler_name, entt::entity source_entity,
              entt::entity target_entity)
  {
    const std::size_t old_size = connected_handlers.size ();
    std::erase_if (connected_handlers,
                   [signal_type_id, system_type_id, &handler_name,
                    source_entity,
                    target_entity] (const connected_handler &connection) {
                     return connection.signal_type_id == signal_type_id
                            && connection.system_type_id == system_type_id
                            && connection.handler_name == handler_name
                            && connection.source_entity == source_entity
                            && connection.target_entity == target_entity;
                   });

    if (connected_handlers.size () == old_size) {
      return false;
    }

    if (db != nullptr) {
      std::erase_if (
          db->connections,
          [signal_type_id, system_type_id, &handler_name, source_entity,
           target_entity] (const signal_connection_debug_entry &connection) {
            return connection.signal_type_id == signal_type_id
                   && connection.system_type_id == system_type_id
                   && connection.handler_name == handler_name
                   && connection.source_entity == source_entity
                   && connection.target_entity == target_entity;
          });
      db->rebuild_listener_counts ();
    }

    return true;
  }

  template <typename Signal>
  void
  dispatch (const Signal &event)
  {
    note_emit<Signal> ();

    if (dispatcher != nullptr) {
      dispatcher->trigger (event);
    }

    if (!resolve_active_registry || !resolve_system_by_type) {
      return;
    }

    entt::registry *registry = resolve_active_registry ();
    if (registry == nullptr) {
      return;
    }

    const entt::id_type signal_type_id = comp::stable_type_id<Signal> ();
    entt::entity source_entity = entt::null;

    for (const registered_signal_source &source : registered_signal_sources) {
      if (source.signal_type_id == signal_type_id
          && source.extract_source_entity != nullptr) {
        source_entity = source.extract_source_entity (&event);
        break;
      }
    }

    for (const connected_handler &connection : connected_handlers) {
      if (connection.signal_type_id != signal_type_id
          || connection.invoke == nullptr) {
        continue;
      }

      if (connection.source_entity != entt::null) {
        if (source_entity == entt::null
            || connection.source_entity != source_entity) {
          continue;
        }
      }

      if (connection.target_entity != entt::null) {
        if (!registry->valid (connection.target_entity)) {
          continue;
        }

        if (connection.entity_matches != nullptr
            && !connection.entity_matches (*registry,
                                           connection.target_entity)) {
          continue;
        }
      }

      ::wsl::sys::ecs_system *system
          = resolve_system_by_type (connection.system_type_id);
      if (system == nullptr) {
        continue;
      }

      connection.invoke (*system, *registry, connection.target_entity, &event);
    }
  }
};

/**
 * Emits a fully constructed signal instance.
 * :param Signal: Signal type.
 * :param hub: Signal hub that owns the runtime connections.
 * :param event: Signal instance to emit.
 */
template <typename Signal>
void
emit (signal_hub &hub, const Signal &event)
{
  hub.dispatch<Signal> (event);
}

/**
 * Constructs and emits a signal instance in one call.
 * :param Signal: Signal type.
 * :param Args: Constructor argument types.
 * :param hub: Signal hub that owns the runtime connections.
 * :param args: Constructor arguments used to build the signal.
 */
template <typename Signal, typename... Args>
void
emit (signal_hub &hub, Args &&...args)
{
  hub.dispatch<Signal> (Signal{ std::forward<Args> (args)... });
}

/** Declares a signal owned by a system-like type. */
template <typename Signal, typename OwnerSystem, typename... Components>
void
declare_system (signal_hub &hub)
{
  hub.template declare_signal<Signal, OwnerSystem, Components...> ();
}

/** Declares a signal whose source entity lives in a signal data member. */
template <typename Signal, typename OwnerSystem,
          entt::entity Signal::*SourceMember, typename... Components>
void
declare_entity_signal (signal_hub &hub)
{
  hub.template declare_signal<Signal, OwnerSystem, Components...> (
      +[] (const void *signal_ptr) -> entt::entity {
        return static_cast<const Signal *> (signal_ptr)->*SourceMember;
      });
}

/** Declares a non-connectable event handler owned by a system. */
template <typename OwnerSystem>
void
declare_handler (signal_hub &hub, const char *handler_name)
{
  hub.template declare_handler<OwnerSystem> (handler_name);
}

/** Declares a named system iteration and its component contract. */
template <typename OwnerSystem, typename... Components>
void
declare_iteration (signal_hub &hub, const char *iteration_name)
{
  hub.template declare_iteration<OwnerSystem, Components...> (iteration_name);
}

} // namespace wsl::reg::sig
