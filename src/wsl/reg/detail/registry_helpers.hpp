#pragma once

#include "../../comp/component_meta.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>

#include <entt/entt.hpp>
#include <entt/core/type_info.hpp>
#include <entt/meta/factory.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace wsl::reg::detail
{

/*!
 * \brief Resolves a display name for a registered type, falling back to
 *        humanized type name or meta information.
 */
template <typename T>
inline std::string
resolve_display_name (std::string_view display_name)
{
  if (!display_name.empty ()) {
    return std::string (display_name);
  }

  const entt::meta_type meta = entt::resolve<T> ();
  if (const std::optional<comp::meta_info> info = comp::get_meta_info (meta);
      info && !info->display_name.empty ()) {
    return info->display_name;
  }

  return comp::humanize_identifier (entt::type_name<T> ().value ());
}

/*!
 * \brief Ensures that EnTT metadata is registered for a type, handling runtime
 *        reset if requested.
 */
template <typename T>
inline void
ensure_meta_registered (entt::id_type type_id, bool runtime_registered)
{
  if (runtime_registered) {
    entt::meta_reset (type_id);
  }

  if constexpr (comp::has_register_meta<T>) {
    T::register_meta ();
  } else if (!entt::resolve (type_id)) {
    entt::meta_factory<T> ().type (type_id);
  }
}

/*!
 * \brief Generic helper to sort a list of descriptors by their display name.
 */
template <typename Descriptor>
inline void
sort_by_display_name (std::vector<const Descriptor *> &descriptors)
{
  std::sort (descriptors.begin (), descriptors.end (),
             [] (const Descriptor *lhs, const Descriptor *rhs) {
               const std::string &lhs_label = lhs->display_name.empty ()
                                                  ? lhs->type_name
                                                  : lhs->display_name;
               const std::string &rhs_label = rhs->display_name.empty ()
                                                  ? rhs->type_name
                                                  : rhs->display_name;
               if (lhs_label == rhs_label) {
                 return lhs->type_id < rhs->type_id;
               }
               return lhs_label < rhs_label;
             });
}

/*!
 * \brief Generic helper to sort a list of descriptors by their stable type ID.
 */
template <typename Descriptor>
inline void
sort_by_type_id (std::vector<const Descriptor *> &descriptors)
{
  std::sort (descriptors.begin (), descriptors.end (),
             [] (const Descriptor *lhs, const Descriptor *rhs) {
               return lhs->type_id < rhs->type_id;
             });
}

/*!
 * \brief Returns a Cereal archive name for a component or singleton type.
 */
inline std::string
make_archive_name (std::string_view prefix, std::string_view type_name)
{
  return std::string (prefix) + std::string (type_name);
}

// ------------------------------------------------------------------
// Named JSON snapshot helpers.
//
// EnTT's entt::snapshot / entt::snapshot_loader write the per-entity,
// per-component stream as a flat sequence of unnamed values, which
// Cereal's JSON output renders as auto-incremented "value0", "value1",
// ... names. The helpers below re-implement the snapshot protocol for
// JSON archives so that the produced JSON is human readable.
// ------------------------------------------------------------------

/*! \brief Detects whether T is stored with the in-place deletion policy. */
template <typename T, typename = void>
struct is_in_place_storage : std::false_type
{
};

template <typename T>
struct is_in_place_storage<
    T,
    std::void_t<decltype (entt::registry::storage_for_type<T>::storage_policy)>>
    : std::bool_constant<entt::registry::storage_for_type<T>::storage_policy
                         == entt::deletion_policy::in_place>
{
};

template <typename T>
inline constexpr bool is_in_place_storage_v = is_in_place_storage<T>::value;

/*! \brief Single save entry: writes an entity id and its component data,
 *         or a tombstone marker for in-place deleted slots. */
template <typename T> struct component_save_entry
{
  entt::entity entity_id{};
  const T *data = nullptr;

  template <class Archive>
  void
  serialize (Archive &ar) const
  {
    if (data == nullptr) {
      ar (cereal::make_nvp ("tombstone", true));
    } else {
      ar (cereal::make_nvp ("entity", entity_id),
          cereal::make_nvp ("data", *data));
    }
  }
};

/*! \brief Single load entry: reads an entity id and its component data,
 *         or detects a tombstone marker. */
template <typename T> struct component_load_entry
{
  entt::entity entity_id{};
  T data{};
  bool is_tombstone = false;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    try {
      bool tombstone = false;
      ar (cereal::make_nvp ("tombstone", tombstone));
      if (tombstone) {
        is_tombstone = true;
        return;
      }
    } catch (const cereal::Exception &) {
      /* not a tombstone entry */
    }
    ar (cereal::make_nvp ("entity", entity_id),
        cereal::make_nvp ("data", data));
  }
};

} // namespace wsl::reg::detail
