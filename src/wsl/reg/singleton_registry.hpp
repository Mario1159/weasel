#pragma once

#include "detail/registry_helpers.hpp"

#if !defined(WSL_MODULE_BUILD)
#include "../rsc/world.hpp"

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
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

#if !defined(WSL_MODULE_BUILD)
#include <cassert>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <memory>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <optional>
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

/** Ordering modes for singleton component descriptor queries. */
enum class singleton_component_order
{
  /** Sort by display name, falling back to the C++ type name. */
  display_name,

  /** Sort by stable type identifier. */
  type_id
};

/** Registration options for singleton components. */
struct singleton_component_registration_options
{
  /** Optional editor-facing display name override. */
  std::string_view display_name;

  /** Whether the singleton is owned by the engine core. */
  bool core = false;

  /** Whether the registration came from runtime project code. */
  bool runtime_registered = false;

  /** Whether the singleton should be serialized with a scene snapshot. */
  bool serialize_with_scene = true;
};

/**
 * Customization point for bound singletons whose serialized state is a
 * projection of runtime-only objects: implement `write_state`/`read_state`
 * (out-of-line, in a .cpp) to round-trip only the editable fields.
 */
template <typename T>
concept has_state_io
    = requires (T &v, serialize::json_writer &w, serialize::json_reader &r) {
        { v.write_state (w) } -> std::same_as<void>;
        { v.read_state (r) } -> std::same_as<void>;
      };

/**
 * Central registry for singleton components (singletons) in the engine.
 *
 * This class manages the registration, creation, and destruction of singletons,
 * which are components that exist globally within a scene or registry.
 */
class singleton_registry
{
public:
  /** Describes a registered singleton type. */
  struct descriptor
  {
    /** The stable type ID of the singleton. */
    ::entt::id_type type_id{};
    /** The C++ type name. */
    std::string type_name;
    /** The user-friendly display name. */
    std::string display_name;
    /** Whether this singleton was registered by user code at runtime. */
    bool runtime_registered = false;
    /** Whether this is a core engine singleton that cannot be removed. */
    bool core = false;
    /**
     * Whether this singleton should be serialized as part of the scene
     * snapshot.
     */
    bool serialize_with_scene = false;
    /** Whether the singleton can be default-constructed. */
    bool can_add_default = false;
    /** Function pointer to check if the singleton exists in a registry. */
    bool (*contains) (::entt::registry &) = nullptr;
    /** Function pointer to emplace a default instance of the singleton. */
    bool (*emplace_default) (::entt::registry &) = nullptr;
    /** Function pointer to remove the singleton from a registry. */
    bool (*remove) (::entt::registry &) = nullptr;
    /** Function pointer to get a raw pointer to the singleton instance. */
    void *(*get_ptr) (::entt::registry &) = nullptr;
    /** Function pointer to save the singleton to a binary stream. */
    void (*save_binary) (serialize::binary_writer &, ::entt::registry &)
        = nullptr;
    /** Function pointer to load the singleton from a binary stream. */
    void (*load_binary) (serialize::binary_reader &, ::entt::registry &)
        = nullptr;
    /** Function pointer to save the singleton to a JSON document. */
    void (*save_json) (serialize::json_writer &, ::entt::registry &) = nullptr;
    /** Function pointer to load the singleton from a JSON document. */
    void (*load_json) (serialize::json_reader &, ::entt::registry &) = nullptr;
  };

  using singleton_component_descriptor = descriptor;

  /**
   * Registers a value-owned singleton component type.
   * :param T: Singleton component type.
   * :param options: Registration options.
   */
  template <comp::singleton_component_type T>
  void register_singleton_component (
      const singleton_component_registration_options &options = {});

  /**
   * Registers metadata for a runtime singleton without loading its C++
   * type.
   *
   * Cached descriptors are only suitable for discovery and name lookup. They do
   * not provide construction, reflection, access, or serialization callbacks.
   */
  void
  register_cached_runtime_singleton_component (::entt::id_type type_id,
                                               std::string_view type_name,
                                               std::string_view display_name);

  /**
   * Registers a bound singleton component type stored as a raw pointer.
   * :param T: Singleton component type.
   * :param options: Registration options.
   */
  template <comp::singleton_component_type T>
  void register_bound_singleton_component (
      const singleton_component_registration_options &options = {});

  /** Finds a singleton component descriptor by type ID. */
  const descriptor *find_singleton_component (::entt::id_type type_id) const;

  /**
   * Finds a singleton component descriptor by C++ type name.
   * :param type_name: Reflected type name.
   * :return: Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_singleton_component (std::string_view type_name) const;

  /**
   * Returns registered singleton component descriptors in the requested
   * order.
   * :param order: Requested descriptor ordering.
   * :return: Ordered descriptor list.
   */
  std::vector<const descriptor *>
  get_singleton_components (singleton_component_order order
                            = singleton_component_order::display_name) const;

  /** Ensures all core singleton components exist in the registry. */
  void apply_core_singleton_components (::entt::registry &registry) const;

  /** Resets or removes non-core singleton components in the registry. */
  void reset_scene_singleton_components (::entt::registry &registry) const;

  /** Clears runtime-registered singleton components from the world. */
  void clear_runtime_singleton_components (rsc::world &world);

  /** Saves one registered singleton component to a binary stream. */
  bool save_singleton_binary (serialize::binary_writer &writer,
                              ::entt::registry &registry,
                              ::entt::id_type type_id) const;

  /** Loads one registered singleton component from a binary stream. */
  bool load_singleton_binary (serialize::binary_reader &reader,
                              ::entt::registry &registry,
                              ::entt::id_type type_id) const;

  /** Saves one registered singleton component to a JSON document. */
  bool save_singleton_json (serialize::json_writer &writer,
                            ::entt::registry &registry,
                            ::entt::id_type type_id) const;

  /** Loads one registered singleton component from a JSON document. */
  bool load_singleton_json (serialize::json_reader &reader,
                            ::entt::registry &registry,
                            ::entt::id_type type_id) const;

  /** Finds a singleton descriptor by type ID. */
  const descriptor *
  find (::entt::id_type type_id) const
  {
    return find_singleton_component (type_id);
  }
  /** Returns all registered descriptors sorted by display name. */
  std::vector<const descriptor *>
  ordered () const
  {
    return get_singleton_components (singleton_component_order::display_name);
  }
  /** Returns all registered descriptors sorted by type ID. */
  std::vector<const descriptor *>
  by_type_id () const
  {
    return get_singleton_components (singleton_component_order::type_id);
  }

  /** Ensures all core singletons are present in the given registry. */
  void
  apply_core_singletons (::entt::registry &registry) const
  {
    apply_core_singleton_components (registry);
  }
  /** Resets or removes non-core singletons in the registry. */
  void
  reset_scene_registry (::entt::registry &registry) const
  {
    reset_scene_singleton_components (registry);
  }
  /** Clears runtime-registered singletons from the specified world. */
  void
  clear_runtime_singletons (rsc::world &world)
  {
    clear_runtime_singleton_components (world);
  }

private:
  std::unordered_map<::entt::id_type, descriptor> m_descriptors;
  std::unordered_map<std::string, ::entt::id_type> m_type_name_to_type_id;
  std::unordered_map<std::string, ::entt::id_type> m_display_name_to_type_id;
};

} // namespace reg

} // namespace wsl
