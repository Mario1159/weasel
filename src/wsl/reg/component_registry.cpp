#include "component_registry.hpp"

#include <algorithm>
#include <cstring>
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
    std::string_view display_name, int struct_size,
    std::vector<descriptor::das_field> fields)
{
  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (type_name);
  desc.display_name = display_name.empty ()
                          ? comp::humanize_identifier (type_name)
                          : std::string (display_name);
  desc.runtime_registered = true;
  desc.can_add_default = true;
  desc.is_das_component = true;
  desc.das_struct_size = struct_size;
  desc.das_fields = std::move (fields);

  m_type_name_to_stable[desc.type_name] = type_id;
  m_display_name_to_stable[desc.display_name] = type_id;
  m_descriptors[type_id] = std::move (desc);
}

bool
component_registry::das_component_contains (entt::id_type type_id,
                                            entt::entity entity) const
{
  auto it = m_das_component_state.find (type_id);
  if (it == m_das_component_state.end ()) {
    return false;
  }
  return it->second.count (entity) != 0;
}

bool
component_registry::das_component_add (entt::id_type type_id,
                                       entt::entity entity)
{
  if (das_component_contains (type_id, entity)) {
    return false;
  }
  m_das_component_state[type_id].insert (entity);

  // Allocate zero-initialized storage for the component data.
  const auto *desc = find_world_component (type_id);
  int size = (desc && desc->das_struct_size > 0) ? desc->das_struct_size : 64;
  m_das_component_data[type_id][entity].assign (static_cast<std::size_t> (size),
                                                0);
  return true;
}

bool
component_registry::das_component_remove (entt::id_type type_id,
                                          entt::entity entity)
{
  auto it = m_das_component_state.find (type_id);
  if (it == m_das_component_state.end ()) {
    return false;
  }
  bool removed = it->second.erase (entity) > 0;
  if (removed) {
    auto data_it = m_das_component_data.find (type_id);
    if (data_it != m_das_component_data.end ()) {
      data_it->second.erase (entity);
    }
  }
  return removed;
}

uint8_t *
component_registry::das_component_data (entt::id_type type_id,
                                        entt::entity entity)
{
  auto data_it = m_das_component_data.find (type_id);
  if (data_it == m_das_component_data.end ()) {
    return nullptr;
  }
  auto ent_it = data_it->second.find (entity);
  if (ent_it == data_it->second.end ()) {
    return nullptr;
  }
  return ent_it->second.data ();
}

const uint8_t *
component_registry::das_component_data (entt::id_type type_id,
                                        entt::entity entity) const
{
  auto data_it = m_das_component_data.find (type_id);
  if (data_it == m_das_component_data.end ()) {
    return nullptr;
  }
  auto ent_it = data_it->second.find (entity);
  if (ent_it == data_it->second.end ()) {
    return nullptr;
  }
  return ent_it->second.data ();
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
            stable_it = m_descriptors.find (it->second);
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
  if ((desc == nullptr) || desc->is_das_component || (desc->copy == nullptr)) {
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
  if ((desc == nullptr) || desc->is_das_component
      || (desc->save_binary == nullptr)) {
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
  if ((desc == nullptr) || desc->is_das_component
      || (desc->load_binary == nullptr)) {
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
  if ((desc == nullptr) || desc->is_das_component
      || (desc->save_json == nullptr)) {
    return false;
  }

  desc->save_json (archive, registry);
  return true;
}

bool
component_registry::load_world_component_json (
    cereal::JSONInputArchive &archive, entt::registry &registry,
    entt::id_type component_type_id) const
{
  const descriptor *desc = find_world_component (component_type_id);
  if ((desc == nullptr) || desc->is_das_component
      || (desc->load_json == nullptr)) {
    return false;
  }

  wsl::log::sys ()->debug (
      "load_world_component_json: type_id={} type_name='{}' load_json={}",
      component_type_id, desc->type_name,
      reinterpret_cast<const void *> (desc->load_json));

  desc->load_json (archive, registry);
  return true;
}

namespace
{

std::string
bytes_to_hex (const std::vector<uint8_t> &bytes)
{
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve (bytes.size () * 2);
  for (uint8_t b : bytes) {
    result.push_back (hex_chars[b >> 4]);
    result.push_back (hex_chars[b & 0x0F]);
  }
  return result;
}

std::vector<uint8_t>
hex_to_bytes (const std::string &hex)
{
  std::vector<uint8_t> result;
  result.reserve (hex.size () / 2);
  for (std::size_t i = 0; i + 1 < hex.size (); i += 2) {
    auto hi = static_cast<uint8_t> (hex[i]);
    auto lo = static_cast<uint8_t> (hex[i + 1]);
    auto nibble = [] (uint8_t c) -> uint8_t {
      if (c >= '0' && c <= '9')
        return static_cast<uint8_t> (c - '0');
      if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t> (c - 'a' + 10);
      if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t> (c - 'A' + 10);
      return 0;
    };
    result.push_back (static_cast<uint8_t> ((nibble (hi) << 4) | nibble (lo)));
  }
  return result;
}

} // namespace

void
component_registry::save_das_components_json (
    cereal::JSONOutputArchive &archive) const
{
  // Build a flat list of all das component entries.
  struct das_entry
  {
    uint32_t type_id;
    uint32_t entity;
    std::string data_hex;
  };
  std::vector<das_entry> entries;

  for (const auto &[type_id, entity_map] : m_das_component_data) {
    for (const auto &[entity, data] : entity_map) {
      entries.push_back ({ static_cast<uint32_t> (type_id),
                           static_cast<uint32_t> (entt::to_integral (entity)),
                           bytes_to_hex (data) });
    }
  }

  std::size_t count = entries.size ();
  archive (cereal::make_nvp ("das_component_count", count));

  for (std::size_t i = 0; i < count; ++i) {
    archive (cereal::make_nvp ("das_type_id", entries[i].type_id));
    archive (cereal::make_nvp ("das_entity", entries[i].entity));
    archive (cereal::make_nvp ("das_data", entries[i].data_hex));
  }
}

void
component_registry::load_das_components_json (cereal::JSONInputArchive &archive)
{
  std::size_t count = 0;
  archive (cereal::make_nvp ("das_component_count", count));

  for (std::size_t i = 0; i < count; ++i) {
    uint32_t type_id = 0;
    uint32_t entity_raw = 0;
    std::string data_hex;

    archive (cereal::make_nvp ("das_type_id", type_id));
    archive (cereal::make_nvp ("das_entity", entity_raw));
    archive (cereal::make_nvp ("das_data", data_hex));

    auto tid = static_cast<entt::id_type> (type_id);
    auto entity = static_cast<entt::entity> (entity_raw);

    // Ensure the component type and entity exist.
    if (!contains_world_component (tid)) {
      register_cached_runtime_world_component (tid, "unknown", "unknown");
    }
    if (!das_component_contains (tid, entity)) {
      das_component_add (tid, entity);
    }

    // Overwrite with saved data.
    std::vector<uint8_t> data = hex_to_bytes (data_hex);
    uint8_t *dest = das_component_data (tid, entity);
    if (dest) {
      const descriptor *desc = find_world_component (tid);
      std::size_t copy_size
          = desc ? static_cast<std::size_t> (desc->das_struct_size)
                 : data.size ();
      copy_size = std::min (copy_size, data.size ());
      std::memcpy (dest, data.data (), copy_size);
    }
  }
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
    // Also clear the das component state and data for this type.
    m_das_component_state.erase (type_id);
    m_das_component_data.erase (type_id);
  }

  for (entt::id_type const internal_id : runtime_internal_ids) {
    m_internal_to_stable.erase (internal_id);
  }
}

} // namespace reg

} // namespace wsl
