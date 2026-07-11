#pragma once

#include "detail/registry_helpers.hpp"

#include "../rsc/world.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <entt/entt.hpp>

#include <cassert>
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
 * \brief Ordering modes for singleton component descriptor queries.
 */
enum class singleton_component_order
{
  //! Sort by display name, falling back to the C++ type name.
  display_name,

  //! Sort by stable type identifier.
  type_id
};

/*!
 * \brief Registration options for singleton components.
 */
struct singleton_component_registration_options
{
  //! Optional editor-facing display name override.
  std::string_view display_name;

  //! Whether the singleton is owned by the engine core.
  bool core = false;

  //! Whether the registration came from runtime project code.
  bool runtime_registered = false;

  //! Whether the singleton should be serialized with a scene snapshot.
  bool serialize_with_scene = true;
};

/*!
 * \brief Concept for types that have a serialize method compatible with Cereal.
 */
template <typename T>
concept has_serialize
    = requires (T &v, cereal::BinaryOutputArchive &ar) { v.serialize (ar); };

/*!
 * \brief Central registry for singleton components (singletons) in the engine.
 *
 * This class manages the registration, creation, and destruction of singletons,
 * which are components that exist globally within a scene or registry.
 */
class singleton_registry
{
public:
  /*!
   * \brief Describes a registered singleton type.
   */
  struct descriptor
  {
    //! The stable type ID of the singleton.
    ::entt::id_type type_id{};
    //! The C++ type name.
    std::string type_name;
    //! The user-friendly display name.
    std::string display_name;
    //! Whether this singleton was registered by user code at runtime.
    bool runtime_registered = false;
    //! Whether this is a core engine singleton that cannot be removed.
    bool core = false;
    //! Whether this singleton should be serialized as part of the scene
    //! snapshot.
    bool serialize_with_scene = false;
    //! Whether the singleton can be default-constructed.
    bool can_add_default = false;
    //! Function pointer to check if the singleton exists in a registry.
    bool (*contains) (::entt::registry &) = nullptr;
    //! Function pointer to emplace a default instance of the singleton.
    bool (*emplace_default) (::entt::registry &) = nullptr;
    //! Function pointer to remove the singleton from a registry.
    bool (*remove) (::entt::registry &) = nullptr;
    //! Function pointer to get a raw pointer to the singleton instance.
    void *(*get_ptr) (::entt::registry &) = nullptr;
    //! Function pointer to save the singleton to a binary archive.
    void (*save_binary) (cereal::BinaryOutputArchive &, ::entt::registry &)
        = nullptr;
    //! Function pointer to load the singleton from a binary archive.
    void (*load_binary) (cereal::BinaryInputArchive &, ::entt::registry &)
        = nullptr;
    //! Function pointer to save the singleton to a JSON archive.
    void (*save_json) (cereal::JSONOutputArchive &, ::entt::registry &)
        = nullptr;
    //! Function pointer to load the singleton from a JSON archive.
    void (*load_json) (cereal::JSONInputArchive &, ::entt::registry &)
        = nullptr;
  };

  using singleton_component_descriptor = descriptor;

  /*!
   * \brief Registers a value-owned singleton component type.
   * \tparam T Singleton component type.
   * \param options Registration options.
   */
  template <comp::singleton_component_type T>
  void register_singleton_component (
      const singleton_component_registration_options &options = {});

  /*!
   * \brief Registers metadata for a runtime singleton without loading its C++
   * type.
   *
   * Cached descriptors are only suitable for discovery and name lookup. They do
   * not provide construction, reflection, access, or serialization callbacks.
   */
  void
  register_cached_runtime_singleton_component (::entt::id_type type_id,
                                               std::string_view type_name,
                                               std::string_view display_name);

  /*!
   * \brief Registers a bound singleton component type stored as a raw pointer.
   * \tparam T Singleton component type.
   * \param options Registration options.
   */
  template <comp::singleton_component_type T>
  void register_bound_singleton_component (
      const singleton_component_registration_options &options = {});

  /*! \brief Finds a singleton component descriptor by type ID. */
  const descriptor *find_singleton_component (::entt::id_type type_id) const;

  /*!
   * \brief Finds a singleton component descriptor by C++ type name.
   * \param type_name Reflected type name.
   * \return Matching descriptor, or `nullptr` when not found.
   */
  const descriptor *find_singleton_component (std::string_view type_name) const;

  /*!
   * \brief Returns registered singleton component descriptors in the requested
   * order.
   * \param order Requested descriptor ordering.
   * \return Ordered descriptor list.
   */
  std::vector<const descriptor *>
  get_singleton_components (singleton_component_order order
                            = singleton_component_order::display_name) const;

  /*! \brief Ensures all core singleton components exist in the registry. */
  void apply_core_singleton_components (::entt::registry &registry) const;

  /*! \brief Resets or removes non-core singleton components in the registry. */
  void reset_scene_singleton_components (::entt::registry &registry) const;

  /*! \brief Clears runtime-registered singleton components from the world. */
  void clear_runtime_singleton_components (rsc::world &world);

  /*!
   * \brief Saves one registered singleton component to a binary archive.
   */
  bool save_singleton_binary (cereal::BinaryOutputArchive &archive,
                              ::entt::registry &registry,
                              ::entt::id_type type_id) const;

  /*!
   * \brief Loads one registered singleton component from a binary archive.
   */
  bool load_singleton_binary (cereal::BinaryInputArchive &archive,
                              ::entt::registry &registry,
                              ::entt::id_type type_id) const;

  /*!
   * \brief Saves one registered singleton component to a JSON archive.
   */
  bool save_singleton_json (cereal::JSONOutputArchive &archive,
                            ::entt::registry &registry,
                            ::entt::id_type type_id) const;

  /*!
   * \brief Loads one registered singleton component from a JSON archive.
   */
  bool load_singleton_json (cereal::JSONInputArchive &archive,
                            ::entt::registry &registry,
                            ::entt::id_type type_id) const;

  /*! \brief Finds a singleton descriptor by type ID. */
  const descriptor *
  find (::entt::id_type type_id) const
  {
    return find_singleton_component (type_id);
  }
  /*! \brief Returns all registered descriptors sorted by display name. */
  std::vector<const descriptor *>
  ordered () const
  {
    return get_singleton_components (singleton_component_order::display_name);
  }
  /*! \brief Returns all registered descriptors sorted by type ID. */
  std::vector<const descriptor *>
  by_type_id () const
  {
    return get_singleton_components (singleton_component_order::type_id);
  }

  /*! \brief Ensures all core singletons are present in the given registry. */
  void
  apply_core_singletons (::entt::registry &registry) const
  {
    apply_core_singleton_components (registry);
  }
  /*! \brief Resets or removes non-core singletons in the registry. */
  void
  reset_scene_registry (::entt::registry &registry) const
  {
    reset_scene_singleton_components (registry);
  }
  /*! \brief Clears runtime-registered singletons from the specified world. */
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

template <comp::singleton_component_type T>
inline void
singleton_registry::register_singleton_component (
    const singleton_component_registration_options &options)
{
  static_assert (std::is_default_constructible_v<T>,
                 "Registered singleton types must be default constructible.");

  const ::entt::id_type type_id = ::entt::type_hash<T>::value ();
  detail::ensure_meta_registered<T> (type_id, options.runtime_registered);

  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (::entt::type_name<T> ().value ());
  desc.display_name = detail::resolve_display_name<T> (options.display_name);
  desc.runtime_registered = options.runtime_registered;
  desc.core = options.core;
  desc.serialize_with_scene = options.serialize_with_scene;
  m_type_name_to_type_id[desc.type_name] = type_id;
  m_display_name_to_type_id[desc.display_name] = type_id;
  desc.contains = +[] (::entt::registry &registry) -> bool {
    return registry.ctx ().contains<T> ();
  };
  desc.emplace_default = +[] (::entt::registry &registry) -> bool {
    auto &ctx = registry.ctx ();
    if (ctx.contains<T> ()) {
      ctx.get<T> () = T{};
      return false;
    }

    ctx.emplace<T> ();
    return true;
  };
  desc.remove = +[] (::entt::registry &registry) -> bool {
    auto &ctx = registry.ctx ();
    if (!ctx.contains<T> ()) {
      return false;
    }

    ctx.erase<T> ();
    return true;
  };

  if (options.core) {
    desc.remove = +[] (::entt::registry &) -> bool { return false; };
  }
  desc.get_ptr = +[] (::entt::registry &registry) -> void * {
    auto &ctx = registry.ctx ();
    if (!ctx.contains<T> ()) {
      return nullptr;
    }

    return &ctx.get<T> ();
  };
  desc.can_add_default = true;

  if (options.serialize_with_scene) {
    desc.save_binary = +[] (cereal::BinaryOutputArchive &archive,
                            ::entt::registry &registry) {
      archive (cereal::make_nvp (
          detail::make_archive_name ("singleton_data_",
                                     ::entt::type_name<T> ().value ()),
          registry.ctx ().get<T> ()));
    };
    desc.load_binary = +[] (cereal::BinaryInputArchive &archive,
                            ::entt::registry &registry) {
      T value{};
      archive (cereal::make_nvp (
          detail::make_archive_name ("singleton_data_",
                                     ::entt::type_name<T> ().value ()),
          value));

      auto &ctx = registry.ctx ();
      if (ctx.contains<T> ()) {
        ctx.get<T> () = std::move (value);
      } else {
        ctx.emplace<T> (std::move (value));
      }
    };
    desc.save_json
        = +[] (cereal::JSONOutputArchive &archive, ::entt::registry &registry) {
            archive (cereal::make_nvp (
                detail::make_archive_name ("singleton_data_",
                                           ::entt::type_name<T> ().value ()),
                registry.ctx ().get<T> ()));
          };
    desc.load_json
        = +[] (cereal::JSONInputArchive &archive, ::entt::registry &registry) {
            T value{};
            archive (cereal::make_nvp (
                detail::make_archive_name ("singleton_data_",
                                           ::entt::type_name<T> ().value ()),
                value));

            auto &ctx = registry.ctx ();
            if (ctx.contains<T> ()) {
              auto &existing = ctx.get<T> ();
              existing = std::move (value);
            } else {
              ctx.emplace<T> (std::move (value));
            }
          };
  }

  m_descriptors[type_id] = std::move (desc);
}

template <comp::singleton_component_type T>
inline void
singleton_registry::register_bound_singleton_component (
    const singleton_component_registration_options &options)
{
  const ::entt::id_type type_id = ::entt::type_hash<T>::value ();
  detail::ensure_meta_registered<T> (type_id, options.runtime_registered);

  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (::entt::type_name<T> ().value ());
  desc.display_name = detail::resolve_display_name<T> (options.display_name);
  desc.runtime_registered = options.runtime_registered;
  desc.core = options.core;
  desc.can_add_default = false;
  desc.serialize_with_scene = options.serialize_with_scene;
  m_type_name_to_type_id[desc.type_name] = type_id;
  m_display_name_to_type_id[desc.display_name] = type_id;
  desc.contains = +[] (::entt::registry &registry) -> bool {
    return registry.ctx ().contains<T *> ();
  };
  desc.remove = +[] (::entt::registry &registry) -> bool {
    auto &ctx = registry.ctx ();
    if (!ctx.contains<T *> ()) {
      return false;
    }

    ctx.erase<T *> ();
    return true;
  };

  if (options.core) {
    desc.remove = +[] (::entt::registry &) -> bool { return false; };
  }
  desc.get_ptr = +[] (::entt::registry &registry) -> void * {
    auto &ctx = registry.ctx ();
    if (!ctx.contains<T *> ()) {
      return nullptr;
    }

    return ctx.get<T *> ();
  };

  if (options.serialize_with_scene) {
    if constexpr (has_serialize<T>) {
      desc.save_binary = +[] (cereal::BinaryOutputArchive &archive,
                              ::entt::registry &registry) {
        archive (cereal::make_nvp (
            detail::make_archive_name ("singleton_data_",
                                       ::entt::type_name<T> ().value ()),
            *registry.ctx ().get<T *> ()));
      };
      desc.load_binary = +[] (cereal::BinaryInputArchive &archive,
                              ::entt::registry &registry) {
        archive (cereal::make_nvp (
            detail::make_archive_name ("singleton_data_",
                                       ::entt::type_name<T> ().value ()),
            *registry.ctx ().get<T *> ()));
      };
      desc.save_json = +[] (cereal::JSONOutputArchive &archive,
                            ::entt::registry &registry) {
        archive (cereal::make_nvp (
            detail::make_archive_name ("singleton_data_",
                                       ::entt::type_name<T> ().value ()),
            *registry.ctx ().get<T *> ()));
      };
      desc.load_json = +[] (cereal::JSONInputArchive &archive,
                            ::entt::registry &registry) {
        archive (cereal::make_nvp (
            detail::make_archive_name ("singleton_data_",
                                       ::entt::type_name<T> ().value ()),
            *registry.ctx ().get<T *> ()));
      };
    }
  }

  m_descriptors[type_id] = std::move (desc);
}

} // namespace reg

} // namespace wsl
