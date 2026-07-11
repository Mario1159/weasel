#pragma once

#include "../comp/component_meta.hpp"

#include "detail/registry_helpers.hpp"
#include "wsl/log/log.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>
#include <entt/core/type_info.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace reg
{

/*!
 * \brief Ordering modes for world component descriptor queries.
 */
enum class world_component_order
{
  //! Sort by display name, falling back to the C++ type name.
  display_name,

  //! Sort by stable type identifier.
  type_id
};

/*!
 * \brief Registration options for entity-owned world components.
 */
struct world_component_registration_options
{
  //! Optional editor-facing display name override.
  std::string_view display_name;

  //! Whether the registration was supplied by runtime project code.
  bool runtime_registered = false;
};

/*!
 * \brief Central registry for component types in the engine.
 *
 * This class manages the registration of component types, providing
 * metadata and function pointers for generic operations like copying,
 * serialization, and default construction.
 */
class component_registry
{
public:
  /*!
   * \brief Describes a registered component type.
   */
  struct descriptor
  {
    //! Stable type identifier.
    entt::id_type type_id{};
    //! Full type name as provided by the compiler.
    std::string type_name;
    //! Human-readable name for the component.
    std::string display_name;
    //! Whether the component was registered at runtime.
    bool runtime_registered = false;
    //! Whether the component can be default-constructed and added to an entity.
    bool can_add_default = false;
    //! Checks if the component exists on the given entity.
    bool (*contains) (entt::registry &, entt::entity) = nullptr;
    //! Emplaces a default instance of the component on the given entity.
    bool (*emplace_default) (entt::registry &, entt::entity) = nullptr;
    //! Removes the component from the given entity.
    bool (*remove) (entt::registry &, entt::entity) = nullptr;
    //! Copies the component from a source entity to a destination entity.
    void (*copy) (entt::registry &src_reg, entt::entity src_ent,
                  entt::registry &dst_reg, entt::entity dst_ent) = nullptr;
    //! Saves component data to a binary archive.
    void (*save_binary) (cereal::BinaryOutputArchive &, entt::registry &)
        = nullptr;
    //! Loads component data from a binary archive.
    void (*load_binary) (cereal::BinaryInputArchive &, entt::snapshot_loader &)
        = nullptr;
    //! Saves component data to a JSON archive.
    void (*save_json) (cereal::JSONOutputArchive &, entt::registry &) = nullptr;
    //! Loads component data from a JSON archive.
    void (*load_json) (cereal::JSONInputArchive &, entt::registry &) = nullptr;
  };

  using world_component_descriptor = descriptor;

  /*!
   * \brief Registers an entity-owned world component type.
   * \tparam T The world component type to register.
   * \param options Registration options.
   */
  template <comp::world_component_type T>
  void
  register_world_component (const world_component_registration_options &options
                            = {});

  /*!
   * \brief Registers metadata for a runtime component without loading its C++
   * type.
   *
   * Cached descriptors are only suitable for discovery and name lookup. They do
   * not provide construction, reflection, copy, or serialization callbacks.
   */
  void register_cached_runtime_world_component (entt::id_type type_id,
                                                std::string_view type_name,
                                                std::string_view display_name);

  /*!
   * \brief Finds a registered world component by stable or internal type ID.
   * \param type_id Stable world component ID or internal EnTT type ID.
   * \return Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_world_component (entt::id_type type_id) const;

  /*!
   * \brief Finds a registered world component by C++ type name.
   * \param type_name Type name returned by reflection.
   * \return Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_world_component (std::string_view type_name) const;

  /*!
   * \brief Returns whether a world component descriptor exists for the ID.
   * \param type_id Stable world component ID or internal EnTT type ID.
   * \return `true` when the descriptor exists, otherwise `false`.
   */
  bool contains_world_component (entt::id_type type_id) const;

  /*!
   * \brief Converts an internal or stable ID to the stable world component ID.
   * \param type_id Stable world component ID or internal EnTT type ID.
   * \return Stable world component ID when known, otherwise the input ID.
   */
  entt::id_type to_stable_world_component_id (entt::id_type type_id) const;

  /*!
   * \brief Returns registered world components in the requested order.
   * \param order Requested descriptor ordering.
   * \return Ordered descriptor list.
   */
  std::vector<const descriptor *>
  get_world_components (world_component_order order
                        = world_component_order::display_name) const;

  /*!
   * \brief Returns the world components that may still be added to an entity.
   * \param registry Registry that owns the entity.
   * \param entity Entity to test.
   * \return Addable world component descriptors for the entity.
   */
  std::vector<const descriptor *>
  get_addable_world_components (entt::registry &registry,
                                entt::entity entity) const;

  /*!
   * \brief Copies a single registered world component from one entity to
   * another.
   */
  bool copy_world_component (entt::registry &src_registry,
                             entt::entity src_entity,
                             entt::registry &dst_registry,
                             entt::entity dst_entity,
                             entt::id_type component_type_id) const;

  /*!
   * \brief Saves one registered world component storage to a binary archive.
   */
  bool save_world_component_binary (cereal::BinaryOutputArchive &archive,
                                    entt::registry &registry,
                                    entt::id_type component_type_id) const;

  /*!
   * \brief Loads one registered world component storage from a binary archive.
   */
  bool load_world_component_binary (cereal::BinaryInputArchive &archive,
                                    entt::snapshot_loader &loader,
                                    entt::id_type component_type_id) const;

  /*!
   * \brief Saves one registered world component storage to a JSON archive.
   */
  bool save_world_component_json (cereal::JSONOutputArchive &archive,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /*!
   * \brief Loads one registered world component storage from a JSON archive.
   */
  bool load_world_component_json (cereal::JSONInputArchive &archive,
                                  entt::registry &registry,
                                  entt::id_type component_type_id) const;

  /*!
   * \brief Clears descriptors that belong to runtime project code.
   */
  void clear_runtime_world_components ();

  /*!
   * \brief Finds a component descriptor by its stable type identifier.
   * \param type_id The stable type identifier.
   * \return Pointer to the descriptor if found, otherwise `nullptr`.
   */
  const descriptor *
  find (entt::id_type type_id) const
  {
    return find_world_component (type_id);
  }

  /*!
   * \brief Converts an internal entt type identifier to a stable type
   * identifier.
   * \param internal_id The internal type identifier.
   * \return The stable type identifier.
   */
  entt::id_type
  to_stable_id (entt::id_type internal_id) const
  {
    return to_stable_world_component_id (internal_id);
  }

  /*!
   * \brief Returns all registered descriptors in insertion order.
   * \return Vector of pointers to descriptors.
   */
  std::vector<const descriptor *>
  ordered () const
  {
    return get_world_components (world_component_order::display_name);
  }

  /*!
   * \brief Returns all registered descriptors sorted by type identifier.
   * \return Vector of pointers to descriptors.
   */
  std::vector<const descriptor *>
  by_type_id () const
  {
    return get_world_components (world_component_order::type_id);
  }

  /*!
   * \brief Clears all components registered at runtime.
   */
  void
  clear_runtime_components ()
  {
    clear_runtime_world_components ();
  }

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

  /*! \brief JSON save logic for one component type. */
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

  /*! \brief JSON load logic for one component type. */
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
}

} // namespace reg

} // namespace wsl
