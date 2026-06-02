#include "singleton_registry.hpp"

#include "../rsc/scene.hpp"
#include "../rsc/world.hpp"

#include <algorithm>
#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wsl
{

namespace reg
{

void
singleton_registry::register_cached_runtime_singleton_component (
    ::entt::id_type type_id, std::string_view type_name,
    std::string_view display_name)
{
  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (type_name);
  desc.display_name = display_name.empty ()
                          ? comp::humanize_identifier (type_name)
                          : std::string (display_name);
  desc.runtime_registered = true;
  desc.serialize_with_scene = true;

  m_type_name_to_type_id[desc.type_name] = type_id;
  m_display_name_to_type_id[desc.display_name] = type_id;
  m_descriptors[type_id] = std::move (desc);
}

const singleton_registry::descriptor *
singleton_registry::find_singleton_component (::entt::id_type type_id) const
{
  if (std::unordered_map<::entt::id_type, descriptor>::const_iterator const it
      = m_descriptors.find (type_id);
      it != m_descriptors.end ()) {
    return &it->second;
  }

  return nullptr;
}

const singleton_registry::descriptor *
singleton_registry::find_singleton_component (std::string_view type_name) const
{
  // 1. Fully qualified C++ type name (e.g. "wsl::comp::singl::physics_manager")
  if (std::unordered_map<std::string, ::entt::id_type>::const_iterator const it
      = m_type_name_to_type_id.find (std::string (type_name));
      it != m_type_name_to_type_id.end ()) {
    return find_singleton_component (it->second);
  }

  // 2. Display name (e.g. "Physics Manager", "Score")
  if (std::unordered_map<std::string, ::entt::id_type>::const_iterator const dit
      = m_display_name_to_type_id.find (std::string (type_name));
      dit != m_display_name_to_type_id.end ()) {
    return find_singleton_component (dit->second);
  }

  // 3. Short name — last segment after "::" (e.g. "physics_manager", "score")
  for (const auto &entry : m_descriptors) {
    std::string_view const full = entry.second.type_name;
    std::size_t const pos = full.rfind ("::");
    std::string_view const short_name
        = (pos != std::string_view::npos) ? full.substr (pos + 2) : full;
    if (short_name == type_name) {
      return &entry.second;
    }
  }

  return nullptr;
}

std::vector<const singleton_registry::descriptor *>
singleton_registry::get_singleton_components (
    singleton_component_order order) const
{
  std::vector<const descriptor *> out;
  out.reserve (m_descriptors.size ());

  for (const auto &entry : m_descriptors) {
    out.push_back (&entry.second);
  }

  if (order == singleton_component_order::type_id) {
    detail::sort_by_type_id (out);
  } else {
    detail::sort_by_display_name (out);
  }

  return out;
}

void
singleton_registry::apply_core_singleton_components (
    ::entt::registry &registry) const
{
  for (const descriptor *desc :
       get_singleton_components (singleton_component_order::type_id)) {
    if ((desc == nullptr) || !desc->core
        || (desc->emplace_default == nullptr)) {
      continue;
    }

    desc->emplace_default (registry);
  }
}

void
singleton_registry::reset_scene_singleton_components (
    ::entt::registry &registry) const
{
  for (const descriptor *desc :
       get_singleton_components (singleton_component_order::type_id)) {
    if (desc == nullptr) {
      continue;
    }

    if (desc->core) {
      if (desc->emplace_default != nullptr) {
        desc->emplace_default (registry);
      }
      continue;
    }

    if (desc->remove != nullptr) {
      desc->remove (registry);
    }
  }
}

void
singleton_registry::clear_runtime_singleton_components (rsc::world &world)
{
  std::vector<::entt::id_type> runtime_ids;
  runtime_ids.reserve (m_descriptors.size ());

  for (const std::pair<const ::entt::id_type, descriptor> &entry :
       m_descriptors) {
    if (entry.second.runtime_registered) {
      runtime_ids.push_back (entry.first);
    }
  }

  for (std::unique_ptr<rsc::scene> &scene_ptr : world.get_scenes ()) {
    if (!scene_ptr) {
      continue;
    }

    ::entt::registry &registry = scene_ptr->get_registry ();
    for (::entt::id_type const type_id : runtime_ids) {
      if (const descriptor *desc = find (type_id);
          (desc != nullptr) && (desc->remove != nullptr) && !desc->core) {
        desc->remove (registry);
      }
    }
  }

  for (::entt::id_type const type_id : runtime_ids) {
    ::entt::meta_reset (type_id);
    if (std::unordered_map<::entt::id_type, descriptor>::const_iterator const it
        = m_descriptors.find (type_id);
        it != m_descriptors.end ()) {
      m_type_name_to_type_id.erase (it->second.type_name);
      m_display_name_to_type_id.erase (it->second.display_name);
    }
    m_descriptors.erase (type_id);
  }
}

bool
singleton_registry::save_singleton_binary (cereal::BinaryOutputArchive &archive,
                                           ::entt::registry &registry,
                                           ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->save_binary == nullptr)) {
    return false;
  }

  desc->save_binary (archive, registry);
  return true;
}

bool
singleton_registry::load_singleton_binary (cereal::BinaryInputArchive &archive,
                                           ::entt::registry &registry,
                                           ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->load_binary == nullptr)) {
    return false;
  }

  desc->load_binary (archive, registry);
  return true;
}

bool
singleton_registry::save_singleton_json (cereal::JSONOutputArchive &archive,
                                         ::entt::registry &registry,
                                         ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->save_json == nullptr)) {
    return false;
  }

  desc->save_json (archive, registry);
  return true;
}

bool
singleton_registry::load_singleton_json (cereal::JSONInputArchive &archive,
                                         ::entt::registry &registry,
                                         ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->load_json == nullptr)) {
    return false;
  }

  desc->load_json (archive, registry);
  return true;
}

} // namespace reg

} // namespace wsl
