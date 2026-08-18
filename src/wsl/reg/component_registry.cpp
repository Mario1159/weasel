#include "component_registry.hpp"

#include <algorithm>
#include <cstring>
#include <entt/core/fwd.hpp>
#include <entt/meta/factory.hpp>
#include <string>
#include <vector>
#include "../log/log.hpp"

namespace wsl
{

namespace reg
{

namespace
{

das_component_storage &
storage_for (entt::registry &registry)
{
  if (!registry.ctx ().contains<das_component_storage> ()) {
    registry.ctx ().emplace<das_component_storage> ();
  }
  return registry.ctx ().get<das_component_storage> ();
}

const das_component_storage *
try_storage_for (const entt::registry &registry)
{
  if (!registry.ctx ().contains<das_component_storage> ()) {
    return nullptr;
  }
  return &registry.ctx ().get<das_component_storage> ();
}

} // namespace

bool
das_component_storage::contains (entt::id_type type_id,
                                 entt::entity entity) const
{
  const pool *component_pool = find_pool (type_id);
  return component_pool != nullptr
         && component_pool->entries.find (entity)
                != component_pool->entries.end ();
}

bool
das_component_storage::add (entt::id_type type_id, entt::entity entity,
                            std::size_t size,
                            const std::vector<default_field> &defaults)
{
  pool &component_pool = m_pools[type_id];
  if (component_pool.size != 0 && component_pool.size != size) {
    return false;
  }
  component_pool.size = size;

  auto [it, inserted] = component_pool.entries.try_emplace (entity);
  if (!inserted) {
    return false;
  }

  data_block &block = it->second;
  block.size = size;
  const std::size_t word_count
      = (size + sizeof (std::max_align_t) - 1) / sizeof (std::max_align_t);
  block.words.resize (word_count);
  if (size > 0) {
    std::memset (block.data (), 0, size);
  }

  for (const default_field &field : defaults) {
    if (field.offset < 0 || field.value.empty ()) {
      continue;
    }
    const std::size_t offset = static_cast<std::size_t> (field.offset);
    if (offset > size || field.value.size () > size - offset) {
      continue;
    }
    std::memcpy (block.data () + offset, field.value.data (),
                 field.value.size ());
  }

  return true;
}

bool
das_component_storage::remove (entt::id_type type_id, entt::entity entity)
{
  pool *component_pool = find_pool (type_id);
  return component_pool != nullptr
         && component_pool->entries.erase (entity) != 0;
}

uint8_t *
das_component_storage::data (entt::id_type type_id, entt::entity entity)
{
  pool *component_pool = find_pool (type_id);
  if (component_pool == nullptr) {
    return nullptr;
  }
  auto it = component_pool->entries.find (entity);
  return it == component_pool->entries.end () ? nullptr : it->second.data ();
}

const uint8_t *
das_component_storage::data (entt::id_type type_id, entt::entity entity) const
{
  const pool *component_pool = find_pool (type_id);
  if (component_pool == nullptr) {
    return nullptr;
  }
  auto it = component_pool->entries.find (entity);
  return it == component_pool->entries.end () ? nullptr : it->second.data ();
}

const das_component_storage::pool *
das_component_storage::find_pool (entt::id_type type_id) const
{
  auto it = m_pools.find (type_id);
  return it == m_pools.end () ? nullptr : &it->second;
}

das_component_storage::pool *
das_component_storage::find_pool (entt::id_type type_id)
{
  auto it = m_pools.find (type_id);
  return it == m_pools.end () ? nullptr : &it->second;
}

void
das_component_storage::clear_entity (entt::entity entity)
{
  for (auto &[type_id, component_pool] : m_pools) {
    (void)type_id;
    component_pool.entries.erase (entity);
  }
}

void
das_component_storage::clear ()
{
  m_pools.clear ();
}

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
component_registry::das_component_contains (entt::registry &registry,
                                            entt::id_type type_id,
                                            entt::entity entity) const
{
  const das_component_storage *storage = try_storage_for (registry);
  return storage != nullptr && storage->contains (type_id, entity);
}

bool
component_registry::das_component_add (entt::registry &registry,
                                       entt::id_type type_id,
                                       entt::entity entity)
{
  if (das_component_contains (registry, type_id, entity)) {
    return false;
  }

  // Allocate storage for the component data, applying default values
  // from the daScript struct definition when available.
  const auto *desc = find_world_component (type_id);
  if (desc == nullptr || desc->das_struct_size <= 0) {
    return false;
  }

  std::vector<das_component_storage::default_field> defaults;
  defaults.reserve (desc->das_fields.size ());
  for (const auto &field : desc->das_fields) {
    defaults.push_back ({ field.offset, field.default_value });
  }
  return storage_for (registry).add (
      type_id, entity, static_cast<std::size_t> (desc->das_struct_size),
      defaults);
}

bool
component_registry::das_component_remove (entt::registry &registry,
                                          entt::id_type type_id,
                                          entt::entity entity)
{
  das_component_storage *storage
      = registry.ctx ().contains<das_component_storage> ()
            ? &registry.ctx ().get<das_component_storage> ()
            : nullptr;
  return storage != nullptr && storage->remove (type_id, entity);
}

uint8_t *
component_registry::das_component_data (entt::registry &registry,
                                        entt::id_type type_id,
                                        entt::entity entity)
{
  if (!registry.ctx ().contains<das_component_storage> ()) {
    return nullptr;
  }
  return registry.ctx ().get<das_component_storage> ().data (type_id, entity);
}

const uint8_t *
component_registry::das_component_data (const entt::registry &registry,
                                        entt::id_type type_id,
                                        entt::entity entity) const
{
  const das_component_storage *storage = try_storage_for (registry);
  return storage == nullptr ? nullptr : storage->data (type_id, entity);
}

const das_component_storage::pool *
component_registry::das_component_pool (const entt::registry &registry,
                                        entt::id_type type_id) const
{
  const das_component_storage *storage = try_storage_for (registry);
  return storage == nullptr ? nullptr : storage->find_pool (type_id);
}

void
component_registry::clear_das_component_storage (entt::registry &registry) const
{
  if (registry.ctx ().contains<das_component_storage> ()) {
    registry.ctx ().get<das_component_storage> ().clear ();
  }
}

void
component_registry::clear_das_component_entity (entt::registry &registry,
                                                entt::entity entity) const
{
  if (registry.ctx ().contains<das_component_storage> ()) {
    registry.ctx ().get<das_component_storage> ().clear_entity (entity);
  }
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

  desc->load_json (archive, registry);
  return true;
}

namespace
{

std::string
bytes_to_hex (const uint8_t *bytes, std::size_t size)
{
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve (size * 2);
  for (std::size_t i = 0; i < size; ++i) {
    const uint8_t byte = bytes[i];
    result.push_back (hex_chars[byte >> 4]);
    result.push_back (hex_chars[byte & 0x0F]);
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
    cereal::JSONOutputArchive &archive, entt::registry &registry) const
{
  // Build a flat list of all das component entries.
  struct das_entry
  {
    uint32_t type_id;
    uint32_t entity;
    std::string data_hex;
  };
  std::vector<das_entry> entries;

  const das_component_storage *storage = try_storage_for (registry);
  if (storage != nullptr) {
    for (const auto *desc :
         get_world_components (world_component_order::type_id)) {
      if (desc == nullptr || !desc->is_das_component) {
        continue;
      }
      const auto *component_pool = storage->find_pool (desc->type_id);
      if (component_pool == nullptr) {
        continue;
      }
      for (const auto &[entity, block] : component_pool->entries) {
        entries.push_back ({ static_cast<uint32_t> (desc->type_id),
                             static_cast<uint32_t> (entt::to_integral (entity)),
                             bytes_to_hex (block.data (), block.size) });
      }
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
component_registry::load_das_components_json (cereal::JSONInputArchive &archive,
                                              entt::registry &registry)
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
    if (!das_component_contains (registry, tid, entity)) {
      das_component_add (registry, tid, entity);
    }

    // Overwrite with saved data.
    std::vector<uint8_t> data = hex_to_bytes (data_hex);
    uint8_t *dest = das_component_data (registry, tid, entity);
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
component_registry::save_das_components_binary (
    cereal::BinaryOutputArchive &archive, entt::registry &registry) const
{
  // Count total entries.
  std::size_t count = 0;
  const das_component_storage *storage = try_storage_for (registry);
  if (storage != nullptr) {
    for (const auto *desc :
         get_world_components (world_component_order::type_id)) {
      if (desc == nullptr || !desc->is_das_component) {
        continue;
      }
      if (const auto *component_pool = storage->find_pool (desc->type_id)) {
        count += component_pool->entries.size ();
      }
    }
  }
  archive (count);

  if (storage != nullptr) {
    for (const auto *desc :
         get_world_components (world_component_order::type_id)) {
      if (desc == nullptr || !desc->is_das_component) {
        continue;
      }
      const auto *component_pool = storage->find_pool (desc->type_id);
      if (component_pool == nullptr) {
        continue;
      }
      for (const auto &[entity, block] : component_pool->entries) {
        auto tid = static_cast<uint32_t> (desc->type_id);
        auto ent = static_cast<uint32_t> (entt::to_integral (entity));
        auto data_size = static_cast<uint32_t> (block.size);
        archive (tid, ent, data_size);
        archive (cereal::binary_data (block.data (), block.size));
      }
    }
  }
}

void
component_registry::load_das_components_binary (
    cereal::BinaryInputArchive &archive, entt::registry &registry)
{
  std::size_t count = 0;
  archive (count);

  for (std::size_t i = 0; i < count; ++i) {
    uint32_t tid_raw = 0;
    uint32_t ent_raw = 0;
    uint32_t data_size = 0;
    archive (tid_raw, ent_raw, data_size);

    auto tid = static_cast<entt::id_type> (tid_raw);
    auto entity = static_cast<entt::entity> (ent_raw);

    // Ensure the component type and entity exist.
    if (!contains_world_component (tid)) {
      register_cached_runtime_world_component (tid, "unknown", "unknown");
    }
    if (!das_component_contains (registry, tid, entity)) {
      das_component_add (registry, tid, entity);
    }

    // Overwrite with saved data.
    uint8_t *dest = das_component_data (registry, tid, entity);
    if (dest && data_size > 0) {
      std::vector<uint8_t> data (data_size);
      archive (cereal::binary_data (data.data (), data_size));
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
  }

  for (entt::id_type const internal_id : runtime_internal_ids) {
    m_internal_to_stable.erase (internal_id);
  }

  // Also clear the lookup table entries for runtime components.
  for (auto it = m_type_info_by_name.begin ();
       it != m_type_info_by_name.end ();) {
    if (auto id_it = m_type_id_to_name.find (it->second.type_id);
        id_it != m_type_id_to_name.end ()) {
      if (auto desc_it = m_descriptors.find (it->second.type_id);
          desc_it != m_descriptors.end ()
          && desc_it->second.runtime_registered) {
        it = m_type_info_by_name.erase (it);
        m_type_id_to_name.erase (id_it);
        continue;
      }
    }
    ++it;
  }
}

// ── Component type lookup table ──

void
component_registry::register_component_type_info (
    const std::string &das_type_name, uint64_t type_id, ComponentKind kind,
    size_t struct_size)
{
  m_type_info_by_name[das_type_name] = ComponentTypeInfo{
    .type_id = type_id, .kind = kind, .struct_size = struct_size
  };
  m_type_id_to_name[type_id] = das_type_name;
}

const ComponentTypeInfo *
component_registry::find_component_type_info (
    const std::string &das_type_name) const
{
  auto it = m_type_info_by_name.find (das_type_name);
  if (it != m_type_info_by_name.end ()) {
    return &it->second;
  }

  std::string normalized = das_type_name;
  if (normalized.ends_with (" const")) {
    normalized.resize (normalized.size () - 6);
    it = m_type_info_by_name.find (normalized);
    if (it != m_type_info_by_name.end ()) {
      return &it->second;
    }
  }

  const std::size_t separator = normalized.rfind ("::");
  if (separator != std::string::npos) {
    normalized.erase (0, separator + 2);
    it = m_type_info_by_name.find (normalized);
    if (it != m_type_info_by_name.end ()) {
      return &it->second;
    }
  }

  return nullptr;
}

const ComponentTypeInfo *
component_registry::find_component_type_info_by_id (uint64_t type_id) const
{
  auto it = m_type_id_to_name.find (type_id);
  if (it != m_type_id_to_name.end ()) {
    return find_component_type_info (it->second);
  }
  return nullptr;
}

} // namespace reg

} // namespace wsl
