#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "../comp/component_meta.hpp"
#include "../das/das_engine.hpp"

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/core/type_info.hpp>
#endif
#endif

// Serialization backends are implementation details (see wsl/serialize); the
// registry only passes opaque writer/reader handles through function pointers.
namespace wsl::serialize
{
class json_writer;
class json_reader;
class binary_writer;
class binary_reader;
}

#include "detail/registry_helpers.hpp"
#if !defined(WSL_MODULE_BUILD)
#include "wsl/log/log.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <memory>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <optional>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <cstddef>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <string>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <string_view>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <type_traits>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <unordered_map>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <vector>
#endif

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
    /** Saves component data to a binary stream. */
    void (*save_binary) (serialize::binary_writer &, entt::registry &)
        = nullptr;
    /** Loads component data from a binary stream. */
    void (*load_binary) (serialize::binary_reader &, entt::registry &)
        = nullptr;
    /** Saves component data to a JSON document. */
    void (*save_json) (serialize::json_writer &, entt::registry &) = nullptr;
    /** Loads component data from a JSON document. */
    void (*load_json) (serialize::json_reader &, entt::registry &) = nullptr;
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

  /** Saves one registered world component storage to a binary stream. */
  bool save_world_component_binary (serialize::binary_writer &writer,
                                    entt::registry &registry,
                                    entt::id_type component_type_id) const;

  /** Loads one registered world component storage from a binary stream. */
  bool load_world_component_binary (serialize::binary_reader &reader,
                                    entt::registry &registry,
                                    entt::id_type component_type_id) const;

  /** Saves one registered world component storage to a JSON document. */
  bool save_world_component_json (serialize::json_writer &writer,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /** Loads one registered world component storage from a JSON document. */
  bool load_world_component_json (serialize::json_reader &reader,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /**
   * Saves all das component data to a JSON document.
   *
   * Each das component is serialized as an array of objects with fields:
   * type_id (uint), entity (uint), data (hex string).
   */
  void save_das_components_json (serialize::json_writer &writer,
                                 entt::registry &registry) const;

  /**
   * Loads das component data from a JSON document.
   *
   * Expects the same format produced by save_das_components_json.
   */
  void load_das_components_json (serialize::json_reader &reader,
                                 entt::registry &registry);

  /**
   * Saves all das component data to a binary stream (for play/stop
   * snapshots).
   */
  void save_das_components_binary (serialize::binary_writer &writer,
                                   entt::registry &registry) const;

  /** Loads das component data from a binary stream (for play/stop snapshots).
   */
  void load_das_components_binary (serialize::binary_reader &reader,
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

} // namespace reg

} // namespace wsl
