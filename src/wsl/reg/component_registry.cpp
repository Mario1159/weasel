#include "component_registry.hpp"

#include <algorithm>
#include <entt/core/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <string>
#include <vector>


namespace wsl
{

namespace reg
{

void
component_registry::register_cached_runtime_world_component (
    entt::id_type type_id, std::string_view type_name,
    std::string_view display_name)
{
  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (type_name);
  desc.display_name = display_name.empty ()
                          ? comp::humanize_identifier (type_name)
                          : std::string (display_name);
  desc.runtime_registered = true;

  m_type_name_to_stable[desc.type_name] = type_id;
  m_display_name_to_stable[desc.display_name] = type_id;
  m_descriptors[type_id] = std::move (desc);
}

const component_registry::descriptor *
component_registry::find_world_component (entt::id_type type_id) const
{
  if (std::unordered_map<entt::id_type, descriptor>::const_iterator const it
      = m_descriptors.find (type_id);
      it != m_descriptors.end ()) {
    return &it->second;
  }

  if (std::unordered_map<entt::id_type, entt::id_type>::const_iterator const it
      = m_internal_to_stable.find (type_id);
      it != m_internal_to_stable.end ()) {
    if (std::unordered_map<entt::id_type, descriptor>::const_iterator const
            stable_it
        = m_descriptors.find (it->second);
        stable_it != m_descriptors.end ()) {
      return &stable_it->second;
    }
  }

  return nullptr;
}

const component_registry::descriptor *
component_registry::find_world_component (std::string_view type_name) const
{
  // 1. Exact C++ type name (e.g. "wsl::comp::transform")
  if (std::unordered_map<std::string, entt::id_type>::const_iterator const it
      = m_type_name_to_stable.find (std::string (type_name));
      it != m_type_name_to_stable.end ()) {
    return find_world_component (it->second);
  }

  // 2. Display name (e.g. "Transform", "Rigid Body")
  if (std::unordered_map<std::string, entt::id_type>::const_iterator const dit
      = m_display_name_to_stable.find (std::string (type_name));
      dit != m_display_name_to_stable.end ()) {
    return find_world_component (dit->second);
  }

  // 3. Short name — last segment after "::" (e.g. "transform", "rigid_body")
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

bool
component_registry::contains_world_component (entt::id_type type_id) const
{
  return find_world_component (type_id) != nullptr;
}

entt::id_type
component_registry::to_stable_world_component_id (entt::id_type type_id) const
{
  if (std::unordered_map<entt::id_type, entt::id_type>::const_iterator const it
      = m_internal_to_stable.find (type_id);
      it != m_internal_to_stable.end ()) {
    return it->second;
  }

  return type_id;
}

std::vector<const component_registry::descriptor *>
component_registry::get_world_components (world_component_order order) const
{
  std::vector<const descriptor *> out;
  out.reserve (m_descriptors.size ());

  for (const auto &entry : m_descriptors) {
    out.push_back (&entry.second);
  }

  if (order == world_component_order::type_id) {
    detail::sort_by_type_id (out);
  } else {
    detail::sort_by_display_name (out);
  }

  return out;
}

std::vector<const component_registry::descriptor *>
component_registry::get_addable_world_components (entt::registry &registry,
                                                  entt::entity entity) const
{
  std::vector<const descriptor *> out;
  if (!registry.valid (entity)) {
    return out;
  }

  for (const descriptor *desc : get_world_components ()) {
    if ((desc == nullptr) || !desc->can_add_default
        || (desc->contains == nullptr) || desc->contains (registry, entity)) {
      continue;
    }

    out.push_back (desc);
  }

  return out;
}

bool
component_registry::copy_world_component (entt::registry &src_registry,
                                         entt::entity src_entity,
                                         entt::registry &dst_registry,
                                         entt::entity dst_entity,
                                         entt::id_type component_type_id) const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || (desc->copy == nullptr)) {
    return false;
  }

  desc->copy (src_registry, src_entity, dst_registry, dst_entity);
  return true;
}

bool
component_registry::save_world_component_binary (
    cereal::BinaryOutputArchive &archive, entt::registry &registry,
    entt::id_type component_type_id) const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || (desc->save_binary == nullptr)) {
    return false;
  }

  desc->save_binary (archive, registry);
  return true;
}

bool
component_registry::load_world_component_binary (
    cereal::BinaryInputArchive &archive, entt::snapshot_loader &loader,
    entt::id_type component_type_id) const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || (desc->load_binary == nullptr)) {
    return false;
  }

  desc->load_binary (archive, loader);
  return true;
}

bool
component_registry::save_world_component_json (
    cereal::JSONOutputArchive &archive, entt::registry &registry,
    entt::id_type component_type_id) const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || (desc->save_json == nullptr)) {
    return false;
  }

  desc->save_json (archive, registry);
  return true;
}

bool
component_registry::load_world_component_json (cereal::JSONInputArchive &archive,
                                              entt::snapshot_loader &loader,
                                              entt::id_type component_type_id)
    const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || (desc->load_json == nullptr)) {
    return false;
  }

  desc->load_json (archive, loader);
  return true;
}

void
component_registry::clear_runtime_world_components ()
{
  std::vector<entt::id_type> runtime_ids;
  std::vector<entt::id_type> runtime_internal_ids;
  runtime_ids.reserve (m_descriptors.size ());

  for (const auto &entry : m_descriptors) {
    if (entry.second.runtime_registered) {
      runtime_ids.push_back (entry.first);
    }
  }

  for (const auto &entry : m_internal_to_stable) {
    if (auto it = m_descriptors.find (entry.second);
        it != m_descriptors.end () && it->second.runtime_registered) {
      runtime_internal_ids.push_back (entry.first);
    }
  }

  for (entt::id_type const type_id : runtime_ids) {
    entt::meta_reset (type_id);
    if (std::unordered_map<entt::id_type, descriptor>::const_iterator const it
        = m_descriptors.find (type_id);
        it != m_descriptors.end ()) {
      m_type_name_to_stable.erase (it->second.type_name);
      m_display_name_to_stable.erase (it->second.display_name);
    }
    m_descriptors.erase (type_id);
  }

  for (entt::id_type const internal_id : runtime_internal_ids) {
    m_internal_to_stable.erase (internal_id);
  }
}

} // namespace reg

} // namespace wsl
