#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "../../comp/component_meta.hpp"

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/core/type_info.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/meta/factory.hpp>
#endif
#endif

#if !defined(WSL_MODULE_BUILD)
#include <algorithm>
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
#include <vector>
#endif

namespace wsl::reg::detail
{

/**
 * Resolves a display name for a registered type, falling back to
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

/**
 * Ensures that EnTT metadata is registered for a type, handling runtime
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

/** Generic helper to sort a list of descriptors by their display name. */
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

/** Generic helper to sort a list of descriptors by their stable type ID. */
template <typename Descriptor>
inline void
sort_by_type_id (std::vector<const Descriptor *> &descriptors)
{
  std::sort (descriptors.begin (), descriptors.end (),
             [] (const Descriptor *lhs, const Descriptor *rhs) {
               return lhs->type_id < rhs->type_id;
             });
}

/** Returns a Cereal archive name for a component or singleton type. */
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

/** Detects whether T is stored with the in-place deletion policy. */
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

} // namespace wsl::reg::detail
