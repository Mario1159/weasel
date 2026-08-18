#pragma once

#include "../comp/component_meta.hpp"
#include "../das/das_engine.hpp"

#include "detail/registry_helpers.hpp"
#include "wsl/log/log.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>
#include <entt/core/type_info.hpp>

#include <memory>
#include <optional>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace reg
{

/** Component type classification for generic dispatch. */
enum class ComponentKind
{
  /** C++ native component (engine-built, stored in EnTT typed storage). */
  CPP_NATIVE,
  /** daScript component (no C++ backing type, raw byte storage). */
  DAS_SCRIPT
};

/** Scene-local storage for components whose types exist only in Daslang. */
class das_component_storage
{
public:
  struct default_field
  {
    int offset = 0;
    std::vector<uint8_t> value;
  };

  struct data_block
  {
    std::vector<std::max_align_t> words;
    std::size_t size = 0;

    uint8_t *
    data ()
    {
      return words.empty () ? nullptr
                            : reinterpret_cast<uint8_t *> (words.data ());
    }

    const uint8_t *
    data () const
    {
      return words.empty () ? nullptr
                            : reinterpret_cast<const uint8_t *> (words.data ());
    }
  };

  struct pool
  {
    std::size_t size = 0;
    std::unordered_map<entt::entity, data_block> entries;
  };

  bool contains (entt::id_type type_id, entt::entity entity) const;
  bool add (entt::id_type type_id, entt::entity entity, std::size_t size,
            const std::vector<default_field> &defaults);
  bool remove (entt::id_type type_id, entt::entity entity);
  uint8_t *data (entt::id_type type_id, entt::entity entity);
  const uint8_t *data (entt::id_type type_id, entt::entity entity) const;
  const pool *find_pool (entt::id_type type_id) const;
  pool *find_pool (entt::id_type type_id);
  void clear_entity (entt::entity entity);
  void clear ();

private:
  std::unordered_map<entt::id_type, pool> m_pools;
};

/** Type-erased metadata for a registered component type. */
struct ComponentTypeInfo
{
  /** Stable type identifier. */
  uint64_t type_id = 0;
  /** Component storage kind. */
  ComponentKind kind = ComponentKind::DAS_SCRIPT;
  /** Size of the component struct in bytes. */
  size_t struct_size = 0;
};

/** Ordering modes for world component descriptor queries. */
enum class world_component_order
{
  /** Sort by display name, falling back to the C++ type name. */
  display_name,

  /** Sort by stable type identifier. */
  type_id
};

/** Registration options for entity-owned world components. */
struct world_component_registration_options
{
  /** Optional editor-facing display name override. */
  std::string_view display_name;

  /** Whether the registration was supplied by runtime project code. */
  bool runtime_registered = false;
};

/**
 * Central registry for component types in the engine.
 *
 * This class manages the registration of component types, providing
 * metadata and function pointers for generic operations like copying,
 * serialization, and default construction.
 */
class component_registry
{
public:
  /** Describes a registered component type. */
  struct descriptor
  {
    /** Stable type identifier. */
    entt::id_type type_id{};
    /** Full type name as provided by the compiler. */
    std::string type_name;
    /** Human-readable name for the component. */
    std::string display_name;
    /** Whether the component was registered at runtime. */
    bool runtime_registered = false;
    /** Whether the component can be default-constructed and added to an entity.
     */
    bool can_add_default = false;
    /** Whether this is a daslang component (no C++ backing type). */
    bool is_das_component = false;
    /** Fields for daslang components. */
    struct das_field
    {
      std::string name;
      std::string type_name;
      int offset = 0;
      int size = 0;
      wsl::das::das_engine::field_type_kind kind
          = wsl::das::das_engine::field_type_kind::unsupported;
      std::vector<uint8_t> default_value;
    };
    int das_struct_size = 0;
    std::vector<das_field> das_fields;
    /** Checks if the component exists on the given entity. */
    bool (*contains) (entt::registry &, entt::entity) = nullptr;
    /** Emplaces a default instance of the component on the given entity. */
    bool (*emplace_default) (entt::registry &, entt::entity) = nullptr;
    /** Removes the component from the given entity. */
    bool (*remove) (entt::registry &, entt::entity) = nullptr;
    /** Copies the component from a source entity to a destination entity. */
    void (*copy) (entt::registry &src_reg, entt::entity src_ent,
                  entt::registry &dst_reg, entt::entity dst_ent) = nullptr;
    /** Saves component data to a binary archive. */
    void (*save_binary) (cereal::BinaryOutputArchive &, entt::registry &)
        = nullptr;
    /** Loads component data from a binary archive. */
    void (*load_binary) (cereal::BinaryInputArchive &, entt::snapshot_loader &)
        = nullptr;
    /** Saves component data to a JSON archive. */
    void (*save_json) (cereal::JSONOutputArchive &, entt::registry &) = nullptr;
    /** Loads component data from a JSON archive. */
    void (*load_json) (cereal::JSONInputArchive &, entt::registry &) = nullptr;
  };

  using world_component_descriptor = descriptor;

  /**
   * Registers an entity-owned world component type.
   * :param T: The world component type to register.
   * :param options: Registration options.
   */
  template <comp::world_component_type T>
  void
  register_world_component (const world_component_registration_options &options
                            = {});

  /**
   * Registers metadata for a runtime component without loading its C++
   * type.
   *
   * Cached descriptors are only suitable for discovery and name lookup. They do
   * not provide construction, reflection, copy, or serialization callbacks.
   */
  void register_cached_runtime_world_component (
      entt::id_type type_id, std::string_view type_name,
      std::string_view display_name, int struct_size = 0,
      std::vector<descriptor::das_field> fields = {});

  /**
   * Finds a registered world component by stable or internal type ID.
   * :param type_id: Stable world component ID or internal EnTT type ID.
   * :return: Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_world_component (entt::id_type type_id) const;

  /**
   * Finds a registered world component by C++ type name.
   * :param type_name: Type name returned by reflection.
   * :return: Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_world_component (std::string_view type_name) const;

  /**
   * Returns whether a world component descriptor exists for the ID.
   * :param type_id: Stable world component ID or internal EnTT type ID.
   * :return: `true` when the descriptor exists, otherwise `false`.
   */
  bool contains_world_component (entt::id_type type_id) const;

  /**
   * Converts an internal or stable ID to the stable world component ID.
   * :param type_id: Stable world component ID or internal EnTT type ID.
   * :return: Stable world component ID when known, otherwise the input ID.
   */
  entt::id_type to_stable_world_component_id (entt::id_type type_id) const;

  /**
   * Returns registered world components in the requested order.
   * :param order: Requested descriptor ordering.
   * :return: Ordered descriptor list.
   */
  std::vector<const descriptor *>
  get_world_components (world_component_order order
                        = world_component_order::display_name) const;

  /**
   * Returns the world components that may still be added to an entity.
   * :param registry: Registry that owns the entity.
   * :param entity: Entity to test.
   * :return: Addable world component descriptors for the entity.
   */
  std::vector<const descriptor *>
  get_addable_world_components (entt::registry &registry,
                                entt::entity entity) const;

  /**
   * Copies a single registered world component from one entity to
   * another.
   */
  bool copy_world_component (entt::registry &src_registry,
                             entt::entity src_entity,
                             entt::registry &dst_registry,
                             entt::entity dst_entity,
                             entt::id_type component_type_id) const;

  /** Saves one registered world component storage to a binary archive. */
  bool save_world_component_binary (cereal::BinaryOutputArchive &archive,
                                    entt::registry &registry,
                                    entt::id_type component_type_id) const;

  /** Loads one registered world component storage from a binary archive. */
  bool load_world_component_binary (cereal::BinaryInputArchive &archive,
                                    entt::snapshot_loader &loader,
                                    entt::id_type component_type_id) const;

  /** Saves one registered world component storage to a JSON archive. */
  bool save_world_component_json (cereal::JSONOutputArchive &archive,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /** Loads one registered world component storage from a JSON archive. */
  bool load_world_component_json (cereal::JSONInputArchive &archive,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /**
   * Saves all das component data to a JSON archive.
   *
   * Each das component is serialized as a JSON array of objects with fields:
   * type_id (uint), entity (uint), data (hex string).
   */
  void save_das_components_json (cereal::JSONOutputArchive &archive,
                                 entt::registry &registry) const;

  /**
   * Loads das component data from a JSON archive.
   *
   * Expects the same format produced by save_das_components_json.
   */
  void load_das_components_json (cereal::JSONInputArchive &archive,
                                 entt::registry &registry);

  /**
   * Saves all das component data to a binary archive (for play/stop
   * snapshots).
   */
  void save_das_components_binary (cereal::BinaryOutputArchive &archive,
                                   entt::registry &registry) const;

  /** Loads das component data from a binary archive (for play/stop snapshots).
   */
  void load_das_components_binary (cereal::BinaryInputArchive &archive,
                                   entt::registry &registry);

  // ── Das component tracking ──

  /** Checks if a das component is present on an entity. */
  bool das_component_contains (entt::registry &registry, entt::id_type type_id,
                               entt::entity entity) const;

  /** Adds a das component marker to an entity. */
  bool das_component_add (entt::registry &registry, entt::id_type type_id,
                          entt::entity entity);

  /** Removes the das component marker from an entity. */
  bool das_component_remove (entt::registry &registry, entt::id_type type_id,
                             entt::entity entity);

  /**
   * Returns a mutable pointer to the raw byte storage for a das
   * component on an entity. Allocates storage if not yet present.
   */
  uint8_t *das_component_data (entt::registry &registry, entt::id_type type_id,
                               entt::entity entity);

  /**
   * Returns a const pointer to the raw byte storage for a das
   * component on an entity, or nullptr if not present.
   */
  const uint8_t *das_component_data (const entt::registry &registry,
                                     entt::id_type type_id,
                                     entt::entity entity) const;

  /** Returns the scene-local pool for query iteration, or nullptr. */
  const das_component_storage::pool *
  das_component_pool (const entt::registry &registry,
                      entt::id_type type_id) const;

  /** Clears all Daslang component payloads attached to a registry. */
  void clear_das_component_storage (entt::registry &registry) const;

  /** Removes one entity from all Daslang component pools in a registry. */
  void clear_das_component_entity (entt::registry &registry,
                                   entt::entity entity) const;

  /** Clears descriptors that belong to runtime project code. */
  void clear_runtime_world_components ();

  /**
   * Finds a component descriptor by its stable type identifier.
   * :param type_id: The stable type identifier.
   * :return: Pointer to the descriptor if found, otherwise `nullptr`.
   */
  const descriptor *
  find (entt::id_type type_id) const
  {
    return find_world_component (type_id);
  }

  /**
   * Converts an internal entt type identifier to a stable type
   * identifier.
   * :param internal_id: The internal type identifier.
   * :return: The stable type identifier.
   */
  entt::id_type
  to_stable_id (entt::id_type internal_id) const
  {
    return to_stable_world_component_id (internal_id);
  }

  /**
   * Returns all registered descriptors in insertion order.
   * :return: Vector of pointers to descriptors.
   */
  std::vector<const descriptor *>
  ordered () const
  {
    return get_world_components (world_component_order::display_name);
  }

  /**
   * Returns all registered descriptors sorted by type identifier.
   * :return: Vector of pointers to descriptors.
   */
  std::vector<const descriptor *>
  by_type_id () const
  {
    return get_world_components (world_component_order::type_id);
  }

  /** Clears all components registered at runtime. */
  void
  clear_runtime_components ()
  {
    clear_runtime_world_components ();
  }

  // ── Component type lookup table (for generic daScript dispatch) ──

  /**
   * Registers component type info in the lookup table.
   * :param das_type_name: The daScript-visible type name (e.g. "Transform").
   * :param type_id: Stable type identifier.
   * :param kind: Component storage kind.
   * :param struct_size: Size of the component struct in bytes.
   */
  void register_component_type_info (const std::string &das_type_name,
                                     uint64_t type_id, ComponentKind kind,
                                     size_t struct_size);

  /**
   * Looks up component type info by daScript type name.
   * :param das_type_name: The daScript-visible type name.
   * :return: Pointer to the type info, or nullptr if not found.
   */
  const ComponentTypeInfo *
  find_component_type_info (const std::string &das_type_name) const;

  /**
   * Looks up component type info by type_id.
   * :param type_id: The stable type identifier.
   * :return: Pointer to the type info, or nullptr if not found.
   */
  const ComponentTypeInfo *
  find_component_type_info_by_id (uint64_t type_id) const;

private:
  // ------------------------------------------------------------------
  // Named JSON snapshot wrappers.
  //
  // EnTT's entt::snapshot / entt::snapshot_loader write the per-entity,
  // per-component stream as a flat sequence of unnamed values, which
  // Cereal's JSON output renders as auto-incremented "value0", "value1",
  // ... names. The wrappers below reimplement the snapshot protocol for
  // JSON archives so that the produced JSON is human readable:
  //
  //   {
  //     "count": <number of entries>,
  //     "entries": [
  //       { "entity": <entt::entity>, "data": <component T> },
  //       { "entity": null }              // tombstone (in-place storage)
  //     ]
  //   }
  //
  // Binary archives keep using EnTT's snapshot directly.
  // ------------------------------------------------------------------

  /** JSON save logic for one component type. */
  template <typename T>
  static void
  save_component_json (cereal::JSONOutputArchive &ar, entt::registry &registry)
  {
    using storage_t = entt::registry::storage_for_type<T>;
    const auto &const_registry = registry;
    const storage_t *storage = const_registry.template storage<T> ();

    if (storage == nullptr) {
      entt::entity zero{};
      ar (cereal::make_nvp ("count", zero));
      return;
    }

    ar (cereal::make_nvp ("count", storage->size ()));

    std::vector<detail::component_save_entry<T>> entries;
    entries.reserve (storage->size ());

    if constexpr (detail::is_in_place_storage_v<T>) {
      for (auto it = storage->rbegin (), last = storage->rend (); it != last;
           ++it) {
        const auto ent = *it;
        if (ent == entt::tombstone) {
          detail::component_save_entry<T> tomb;
          tomb.data = nullptr;
          entries.push_back (tomb);
        } else {
          entries.push_back (
              detail::component_save_entry<T>{ ent, &storage->get (ent) });
        }
      }
    } else {
      for (auto elem : storage->reach ()) {
        entries.push_back (detail::component_save_entry<T>{
            std::get<0> (elem), &std::get<1> (elem) });
      }
    }

    ar (cereal::make_nvp ("entries", entries));
  }

  /** JSON load logic for one component type. */
  template <typename T>
  static void
  load_component_json (cereal::JSONInputArchive &ar, entt::registry &registry)
  {
    using storage_t = entt::registry::storage_for_type<T>;
    storage_t &storage = registry.template storage<T> ();

    std::size_t count{};
    ar (cereal::make_nvp ("count", count));

    if (count == 0U) {
      return;
    }

    std::vector<detail::component_load_entry<T>> entries;
    ar (cereal::make_nvp ("entries", entries));

    for (auto &entry : entries) {
      if (!entry.is_tombstone) {
        if (storage.contains (entry.entity_id)) {
          storage.get (entry.entity_id) = std::move (entry.data);
        } else {
          storage.emplace (entry.entity_id, std::move (entry.data));
        }
      }
    }
  }

  // Internal wrappers previously in rsc::detail. Moved here to avoid an
  // additional namespace and keep implementation details private to this
  // class.
  template <typename T> struct component_snapshot_wrapper
  {
    entt::registry &registry;
    template <class Archive>
    void
    serialize (Archive &ar)
    {
      if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
        save_component_json<T> (ar, registry);
      } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
        // Loading is performed by component_loader_wrapper. This branch
        // exists so the wrapper is symmetric when reused.
        (void)ar;
      } else {
        entt::snapshot const snapshot{ registry };
        snapshot.get<T> (ar);
      }
    }
  };

  template <typename T> struct component_loader_wrapper
  {
    entt::snapshot_loader &loader;
    template <class Archive>
    void
    serialize (Archive &ar)
    {
      loader.template get<T> (ar);
    }
  };

  template <typename T> struct component_json_loader_wrapper
  {
    entt::registry &registry;
    template <class Archive>
    void
    serialize (Archive &ar)
    {
      if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
        load_component_json<T> (ar, registry);
      }
    }
  };

  std::unordered_map<entt::id_type, descriptor> m_descriptors;
  std::unordered_map<entt::id_type, entt::id_type> m_internal_to_stable;
  std::unordered_map<std::string, entt::id_type> m_type_name_to_stable;
  std::unordered_map<std::string, entt::id_type> m_display_name_to_stable;

  // Das component tracking: type_id -> set of entities that have it.
  std::unordered_map<entt::id_type, std::unordered_set<entt::entity>>
      m_das_component_state;

  // Das component data storage: type_id -> entity -> raw bytes.
  std::unordered_map<entt::id_type,
                     std::unordered_map<entt::entity, std::vector<uint8_t>>>
      m_das_component_data;

  // Component type lookup table: daScript type name -> type info.
  std::unordered_map<std::string, ComponentTypeInfo> m_type_info_by_name;
  // Reverse lookup: type_id -> daScript type name.
  std::unordered_map<uint64_t, std::string> m_type_id_to_name;
};

template <comp::world_component_type T>
inline void
component_registry::register_world_component (
    const world_component_registration_options &options)
{
  const entt::id_type type_id = wsl::comp::stable_type_id<T> ();
  const entt::id_type internal_id = entt::type_id<T> ().hash ();
  m_internal_to_stable[internal_id] = type_id;

  wsl::log::sys ()->trace ("Registering component '{}' (id={})",
                           entt::type_name<T> ().value (), type_id);

  detail::ensure_meta_registered<T> (type_id, options.runtime_registered);

  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (entt::type_name<T> ().value ());
  desc.display_name = detail::resolve_display_name<T> (options.display_name);
  m_type_name_to_stable[desc.type_name] = type_id;
  m_display_name_to_stable[desc.display_name] = type_id;
  desc.runtime_registered = options.runtime_registered;
  desc.contains = +[] (entt::registry &registry, entt::entity entity) -> bool {
    return registry.valid (entity) && registry.all_of<T> (entity);
  };

  if constexpr (std::is_default_constructible_v<T>) {
    desc.can_add_default = true;
    desc.emplace_default
        = +[] (entt::registry &registry, entt::entity entity) -> bool {
      if (!registry.valid (entity) || registry.all_of<T> (entity)) {
        return false;
      }

      registry.emplace<T> (entity);
      return true;
    };
  }

  desc.remove = +[] (entt::registry &registry, entt::entity entity) -> bool {
    if (!registry.valid (entity) || !registry.all_of<T> (entity)) {
      return false;
    }

    (registry.remove<T>)(entity);
    return true;
  };

  wsl::log::sys ()->trace ("Registered component '{}' ({})", desc.display_name,
                           desc.type_name);

  if constexpr (std::is_copy_constructible_v<T>) {
    desc.copy = +[] (entt::registry &src_reg, entt::entity src_ent,
                     entt::registry &dst_reg, entt::entity dst_ent) {
      if (src_reg.all_of<T> (src_ent)) {
        if constexpr (std::is_empty_v<T>) {
          dst_reg.emplace_or_replace<T> (dst_ent);
        } else {
          dst_reg.emplace_or_replace<T> (dst_ent, src_reg.get<T> (src_ent));
        }
      }
    };
  }

  desc.save_binary
      = +[] (cereal::BinaryOutputArchive &archive, entt::registry &registry) {
          archive (cereal::make_nvp (
              detail::make_archive_name ("component_data_",
                                         entt::type_name<T> ().value ()),
              component_registry::component_snapshot_wrapper<T>{ registry }));
        };
  desc.load_binary = +[] (cereal::BinaryInputArchive &archive,
                          entt::snapshot_loader &loader) {
    archive (cereal::make_nvp (
        detail::make_archive_name ("component_data_",
                                   entt::type_name<T> ().value ()),
        component_registry::component_loader_wrapper<T>{ loader }));
  };
  desc.save_json
      = +[] (cereal::JSONOutputArchive &archive, entt::registry &registry) {
          archive (cereal::make_nvp (
              detail::make_archive_name ("component_data_",
                                         entt::type_name<T> ().value ()),
              component_registry::component_snapshot_wrapper<T>{ registry }));
        };
  desc.load_json = +[] (cereal::JSONInputArchive &archive,
                        entt::registry &registry) {
    archive (cereal::make_nvp (
        detail::make_archive_name ("component_data_",
                                   entt::type_name<T> ().value ()),
        component_registry::component_json_loader_wrapper<T>{ registry }));
  };

  m_descriptors[type_id] = std::move (desc);

  // Also register in the lookup table for generic dispatch.
  // Use the display name (or type name) as the daScript-visible key.
  std::string das_name = desc.display_name.empty ()
                             ? std::string (entt::type_name<T> ().value ())
                             : desc.display_name;
  register_component_type_info (das_name, type_id, ComponentKind::CPP_NATIVE,
                                sizeof (T));
}

} // namespace reg

} // namespace wsl
