#include "singleton_registry.hpp"

#include "../rsc/scene.hpp"
#include "../rsc/world.hpp"

#include <algorithm>
#include <cstring>
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

namespace
{

das_singleton_storage &
storage_for (::entt::registry &registry)
{
  if (!registry.ctx ().contains<das_singleton_storage> ()) {
    registry.ctx ().emplace<das_singleton_storage> ();
  }
  return registry.ctx ().get<das_singleton_storage> ();
}

const das_singleton_storage *
try_storage_for (const ::entt::registry &registry)
{
  if (!registry.ctx ().contains<das_singleton_storage> ()) {
    return nullptr;
  }
  return &registry.ctx ().get<das_singleton_storage> ();
}

} // namespace

bool
das_singleton_storage::contains (::entt::id_type type_id) const
{
  return m_blocks.find (type_id) != m_blocks.end ();
}

das_singleton_storage::block *
das_singleton_storage::emplace_default (::entt::id_type type_id,
                                        std::size_t struct_size)
{
  return emplace_with (type_id, struct_size, std::vector<default_field>{});
}

das_singleton_storage::block *
das_singleton_storage::emplace_with (::entt::id_type type_id,
                                     std::size_t struct_size,
                                     const std::vector<default_field> &defaults)
{
  if ((struct_size == 0) || contains (type_id)) {
    return nullptr;
  }

  block &b = m_blocks[type_id];
  // `max_align_t` keeps every scalar field aligned; daslang structs are
  // naturally aligned, so byte offsets recorded at registration are valid.
  b.words.assign ((struct_size + sizeof (std::max_align_t) - 1)
                      / sizeof (std::max_align_t),
                  std::max_align_t{});
  b.size = struct_size;
  std::fill (b.words.begin (), b.words.end (), std::max_align_t{});

  // Apply the declared initialisers over the zeroed block. A short or missing
  // initialiser leaves the zero in place rather than reading past the value.
  for (const default_field &field : defaults) {
    if ((field.offset < 0) || (field.value.empty ())) {
      continue;
    }
    const std::size_t offset = static_cast<std::size_t> (field.offset);
    if (offset >= struct_size) {
      continue;
    }
    const std::size_t n
        = std::min (struct_size - offset, field.value.size ());
    std::memcpy (reinterpret_cast<uint8_t *> (b.words.data ()) + offset,
                 field.value.data (), n);
  }
  return &b;
}

das_singleton_storage::block *
das_singleton_storage::data (::entt::id_type type_id)
{
  const auto it = m_blocks.find (type_id);
  return it != m_blocks.end () ? &it->second : nullptr;
}

const das_singleton_storage::block *
das_singleton_storage::data (::entt::id_type type_id) const
{
  const auto it = m_blocks.find (type_id);
  return it != m_blocks.end () ? &it->second : nullptr;
}

bool
das_singleton_storage::remove (::entt::id_type type_id)
{
  return m_blocks.erase (type_id) != 0;
}

void
das_singleton_storage::clear ()
{
  m_blocks.clear ();
}

void
singleton_registry::register_cached_runtime_singleton_component (
    ::entt::id_type type_id, std::string_view type_name,
    std::string_view display_name, int das_struct_size,
    std::vector<descriptor::das_field> das_fields)
{
  descriptor desc{};
  desc.type_id = type_id;
  desc.type_name = std::string (type_name);
  desc.display_name = display_name.empty ()
                          ? comp::humanize_identifier (type_name)
                          : std::string (display_name);
  desc.runtime_registered = true;
  desc.serialize_with_scene = true;

  // A daslang singleton with a known layout can be created, removed and
  // serialized. The value bytes live in das_singleton_storage, and the
  // descriptor's bare function-pointer callbacks receive no type id, so they
  // cannot be filled in here. `das_singleton_*` below is the access path
  // instead -- the same arrangement daslang components use in
  // component_registry. Without a recorded layout this stays
  // discovery-only, as before.
  if (das_struct_size > 0) {
    desc.is_das_singleton = true;
    desc.das_struct_size = das_struct_size;
    desc.das_fields = std::move (das_fields);
    desc.can_add_default = true;
  }

  m_type_name_to_type_id[desc.type_name] = type_id;
  m_display_name_to_type_id[desc.display_name] = type_id;
  m_descriptors[type_id] = std::move (desc);
}

bool
singleton_registry::das_singleton_contains (const ::entt::registry &registry,
                                            ::entt::id_type type_id) const
{
  const das_singleton_storage *storage = try_storage_for (registry);
  return storage != nullptr && storage->contains (type_id);
}

bool
singleton_registry::das_singleton_add (::entt::registry &registry,
                                       ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || !desc->is_das_singleton
      || (desc->das_struct_size <= 0)
      || das_singleton_contains (registry, type_id)) {
    return false;
  }
  // Apply the initialisers declared in the daslang struct so `singl add`
  // yields the authored defaults rather than zeroes.
  std::vector<das_singleton_storage::default_field> defaults;
  defaults.reserve (desc->das_fields.size ());
  for (const auto &field : desc->das_fields) {
    defaults.push_back ({ field.offset, field.default_value });
  }

  return storage_for (registry).emplace_with (
             type_id, static_cast<std::size_t> (desc->das_struct_size),
             defaults)
         != nullptr;
}

bool
singleton_registry::das_singleton_remove (::entt::registry &registry,
                                          ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || !desc->is_das_singleton) {
    return false;
  }
  if (!registry.ctx ().contains<das_singleton_storage> ()) {
    return false;
  }
  return registry.ctx ().get<das_singleton_storage> ().remove (type_id);
}

uint8_t *
singleton_registry::das_singleton_data (::entt::registry &registry,
                                        ::entt::id_type type_id) const
{
  const das_singleton_storage *storage = try_storage_for (registry);
  if (storage == nullptr) {
    return nullptr;
  }
  const das_singleton_storage::block *block = storage->data (type_id);
  return block != nullptr ? const_cast<uint8_t *> (block->data ()) : nullptr;
}

std::string
singleton_registry::das_singleton_hex (const ::entt::registry &registry,
                                       ::entt::id_type type_id) const
{
  const das_singleton_storage *storage = try_storage_for (registry);
  if (storage == nullptr) {
    return {};
  }
  const das_singleton_storage::block *block = storage->data (type_id);
  if ((block == nullptr) || (block->data () == nullptr) || (block->size == 0)) {
    return {};
  }

  static constexpr char digits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve (block->size * 2);
  for (std::size_t i = 0; i < block->size; ++i) {
    hex.push_back (digits[(block->data ()[i] >> 4U) & 0x0FU]);
    hex.push_back (digits[block->data ()[i] & 0x0FU]);
  }
  return hex;
}

bool
singleton_registry::das_singleton_load_hex (::entt::registry &registry,
                                            ::entt::id_type type_id,
                                            std::string_view hex) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || !desc->is_das_singleton
      || (desc->das_struct_size <= 0)) {
    return false;
  }

  const auto nibble = [] (char c) -> int {
    if ((c >= '0') && (c <= '9')) {
      return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
      return 10 + (c - 'a');
    }
    if ((c >= 'A') && (c <= 'F')) {
      return 10 + (c - 'A');
    }
    return -1;
  };

  const std::size_t expected = static_cast<std::size_t> (desc->das_struct_size);
  if (hex.size () != expected * 2) {
    return false;
  }

  std::vector<uint8_t> decoded (expected);
  for (std::size_t i = 0; i < expected; ++i) {
    const int hi = nibble (hex[i * 2]);
    const int lo = nibble (hex[i * 2 + 1]);
    if ((hi < 0) || (lo < 0)) {
      return false;
    }
    decoded[i] = static_cast<uint8_t> ((hi << 4) | lo);
  }

  das_singleton_storage &storage = storage_for (registry);
  if (!storage.contains (type_id)
      && (storage.emplace_default (type_id, expected) == nullptr)) {
    return false;
  }
  uint8_t *out = das_singleton_data (registry, type_id);
  if (out == nullptr) {
    return false;
  }
  std::memcpy (out, decoded.data (), expected);
  return true;
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
singleton_registry::save_singleton_binary (serialize::binary_writer &writer,
                                           ::entt::registry &registry,
                                           ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->save_binary == nullptr)) {
    return false;
  }

  desc->save_binary (writer, registry);
  return true;
}

bool
singleton_registry::load_singleton_binary (serialize::binary_reader &reader,
                                           ::entt::registry &registry,
                                           ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->load_binary == nullptr)) {
    return false;
  }

  desc->load_binary (reader, registry);
  return true;
}

bool
singleton_registry::save_singleton_json (serialize::json_writer &writer,
                                         ::entt::registry &registry,
                                         ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->save_json == nullptr)) {
    return false;
  }

  desc->save_json (writer, registry);
  return true;
}

bool
singleton_registry::load_singleton_json (serialize::json_reader &reader,
                                         ::entt::registry &registry,
                                         ::entt::id_type type_id) const
{
  const descriptor *desc = find_singleton_component (type_id);
  if ((desc == nullptr) || (desc->load_json == nullptr)) {
    return false;
  }

  desc->load_json (reader, registry);
  return true;
}

} // namespace reg

} // namespace wsl
