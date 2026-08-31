#include "scene_snapshot_serializer.hpp"
#include "wsl/log/log.hpp"

#include <cstdint>
#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <fstream>
#include <glm/ext/vector_float3.hpp>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../comp/transform.hpp"
#include "../comp/singl/physics_manager.hpp"
#include "comp/area3d.hpp"
#include "comp/character_body.hpp"
#include "comp/rigid_body.hpp"
#include "comp/world_transform.hpp"
#include "phys/physics_engine.hpp"
#include "../reg/component_registry.hpp"
#include "../serialize/component_adapters.hpp"
#include "../serialize/serialize.hpp"
#include "rsc/resource_manager.hpp"
#include "rsc/resource_ref.hpp"
#include "rsc/scene.hpp"
#include "../reg/singleton_registry.hpp"
#include "../reg/system_factory_registry.hpp"
#include "sys/system.hpp"

namespace wsl
{

namespace rsc
{

namespace io
{

scene_snapshot_serializer::scene_snapshot_serializer (
    comp::singl::runtime_context *runtime_ctx, scene &scene)
    : scene_ref (scene), runtime_ctx (runtime_ctx)
{
}

// ------------------------------------------------------------------
// Header
// ------------------------------------------------------------------

scene_header
scene_snapshot_serializer::build_header () const
{
  scene_header header;
  header.scene_name = scene_ref.get_name ();
  header.is_prefab = is_prefab;

  for (const std::unique_ptr<sys::ecs_system> &sys : scene_ref.systems) {
    header.systems.push_back (sys->get_name ());
  }

  // Defensive purge: destroyed entities must not leave stale names behind
  // in the saved header, even if a destroy path missed remove_entity_name.
  for (auto it = scene_ref.get_entity_names ().begin ();
       it != scene_ref.get_entity_names ().end ();) {
    if (!scene_ref.get_registry ().valid (it->first))
      it = scene_ref.get_entity_names ().erase (it);
    else
      ++it;
  }

  for (const std::pair<const entt::entity, std::string> &entry :
       scene_ref.get_entity_names ()) {
    header.entity_names.emplace_back (
        static_cast<uint32_t> (entt::to_integral (entry.first)), entry.second);
  }

  header.connections = runtime_ctx->event_hub ().get_all_connections ();

  for (const resource_ref &ref : scene_ref.get_load_list ()) {
    std::string path = runtime_ctx->resource_manager ().get_path (ref);
    wsl::log::rsc ()->trace ("Serializing autoload type={} path={}",
                             (int)ref.type, path);
    header.autoload.push_back ({ ref.type, path });
  }

  header.camera = static_cast<uint32_t> (entt::to_integral (scene_ref.camera));

  return header;
}

// ------------------------------------------------------------------
// Entity id storage (shared semantics for both formats)
// ------------------------------------------------------------------

namespace
{

void
save_entity_ids (serialize::json_writer &writer, entt::registry &registry)
{
  auto &storage = registry.storage<entt::entity> ();

  writer.begin_object ("entities");
  writer.write_u64 ("alive_count", storage.size ());
  writer.write_u64 ("free_list_count", storage.free_list ());

  writer.begin_array ("ids");
  for (auto it = storage.rbegin (), last = storage.rend (); it != last; ++it) {
    writer.append_u64 (static_cast<std::uint64_t> (entt::to_integral (*it)));
  }
  writer.end_array ();
  writer.end_object ();
}

void
load_entity_ids (serialize::json_reader &reader, entt::registry &registry)
{
  auto &storage = registry.storage<entt::entity> ();

  if (!reader.enter_object ("entities")) {
    return;
  }

  std::uint64_t alive_count = 0;
  std::uint64_t free_list_count = 0;
  reader.read_u64 ("alive_count", alive_count);
  reader.read_u64 ("free_list_count", free_list_count);

  storage.reserve (static_cast<std::size_t> (alive_count));

  entt::entity placeholder{};
  const std::size_t id_count = reader.array_size ("ids");
  if (reader.enter_array ("ids")) {
    for (std::size_t i = 0; i < id_count; ++i) {
      std::uint64_t raw = 0;
      if (!reader.element_u64 (i, raw)) {
        continue;
      }
      entt::entity const e{ static_cast<entt::id_type> (raw) };
      storage.generate (e);
      placeholder = (e > placeholder) ? e : placeholder;
    }
    reader.leave ();
  }

  entt::entity next_after_last = entt::entity{ static_cast<entt::id_type> (
      entt::to_integral (placeholder) + 1U) };
  storage.start_from (next_after_last);
  storage.free_list (static_cast<std::size_t> (free_list_count));

  reader.leave ();
}

} // namespace

// ------------------------------------------------------------------
// JSON
// ------------------------------------------------------------------

void
scene_snapshot_serializer::save_json_doc (serialize::json_writer &writer) const
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  entt::registry &registry = scene_ref.get_registry ();
  const scene_header header = build_header ();

  serialize::json_write (writer, "header", header);
  save_entity_ids (writer, registry);

  writer.begin_object ("components");
  for (const reg::component_registry::descriptor *desc :
       runtime_ctx->component_registry ().get_world_components (
           reg::world_component_order::type_id)) {
    if (!desc) {
      continue;
    }
    runtime_ctx->component_registry ().save_world_component_json (
        writer, registry, desc->type_id);
  }
  writer.end_object ();

  writer.begin_object ("das");
  runtime_ctx->component_registry ().save_das_components_json (writer,
                                                               registry);
  writer.end_object ();

  writer.begin_object ("singletons");
  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    const bool has = desc->contains ? desc->contains (registry) : false;
    writer.write_bool ("has_" + desc->type_name, has);

    if (has && !header.is_prefab) {
      runtime_ctx->singleton_registry ().save_singleton_json (writer, registry,
                                                              desc->type_id);
    }
  }
  writer.end_object ();

  resource_manager::serialization_context::get () = nullptr;
}

void
scene_snapshot_serializer::load_json_doc (serialize::json_reader &reader)
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  wsl::log::rsc ()->trace ("Loading scene");
  scene_ref.stop_and_clear ();
  runtime_ctx->event_hub ().clear_connections ();

  scene_header header;

  wsl::log::rsc ()->trace ("Loading header");
  if (!serialize::json_read (reader, "header", header)) {
    wsl::log::rsc ()->error ("Scene file is missing its header");
    resource_manager::serialization_context::get () = nullptr;
    return;
  }

  // Restore the scene name from the saved header
  scene_ref.set_name (header.scene_name);

  // Restore the active camera entity
  scene_ref.camera = entt::entity{ static_cast<entt::id_type> (header.camera) };

  // ---- RESTORE SYSTEMS ----
  wsl::log::rsc ()->trace ("Restoring {} systems", header.systems.size ());
  scene_ref.systems.clear ();

  reg::system_factory_registry &factory
      = runtime_ctx->system_factory_registry ();

  for (const std::string &sys_name : header.systems) {
    if (std::unique_ptr<sys::ecs_system> sys
        = factory.create (sys_name, scene_ref)) {
      scene_ref.add_system_instance (std::move (sys), false);
    } else {
      wsl::log::rsc ()->warn ("Unknown system in scene: {}", sys_name);
    }
  }

  wsl::log::rsc ()->trace ("Loading entities");
  entt::registry &registry = scene_ref.get_registry ();

  load_entity_ids (reader, registry);

  wsl::log::rsc ()->trace ("Loading components");
  if (reader.enter_object ("components")) {
    for (const reg::component_registry::descriptor *desc :
         runtime_ctx->component_registry ().get_world_components (
             reg::world_component_order::type_id)) {
      if (!desc) {
        continue;
      }

      /* Missing component data is expected for empty/new scenes: the load
         callback returns early when its key is absent. */
      runtime_ctx->component_registry ().load_world_component_json (
          reader, registry, desc->type_id);
    }
    reader.leave ();
  }

  // Load das component data (after regular components, before singletons).
  wsl::log::rsc ()->trace ("Loading das components");
  if (reader.enter_object ("das")) {
    runtime_ctx->component_registry ().load_das_components_json (reader,
                                                                 registry);
    reader.leave ();
  }

  wsl::log::rsc ()->trace ("Loading singletons");
  entt::registry &scene_registry = scene_ref.get_registry ();
  if (reader.enter_object ("singletons")) {
    for (const reg::singleton_registry::descriptor *desc :
         runtime_ctx->singleton_registry ().get_singleton_components (
             reg::singleton_component_order::type_id)) {
      if (!desc || !desc->serialize_with_scene) {
        continue;
      }

      bool has = false;
      std::string const has_name = "has_" + desc->type_name;
      reader.read_bool (has_name, has);

      if (has && !header.is_prefab) {
        runtime_ctx->singleton_registry ().load_singleton_json (
            reader, scene_registry, desc->type_id);
      }
    }
    reader.leave ();
  }

  post_load_finalize (header);
}

// ------------------------------------------------------------------
// Binary (same structure; payloads are msgpack blobs)
// ------------------------------------------------------------------

void
scene_snapshot_serializer::save_binary_stream (
    serialize::binary_writer &writer) const
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  entt::registry &registry = scene_ref.get_registry ();
  const scene_header header = build_header ();

  serialize::msgpack_write (writer, header);

  auto &storage = registry.storage<entt::entity> ();
  const std::uint64_t alive_count = storage.size ();
  const std::uint64_t free_list_count = storage.free_list ();
  writer.write (alive_count);
  writer.write (free_list_count);
  for (auto it = storage.rbegin (), last = storage.rend (); it != last; ++it) {
    const std::uint32_t id
        = static_cast<std::uint32_t> (entt::to_integral (*it));
    writer.write (id);
  }

  for (const reg::component_registry::descriptor *desc :
       runtime_ctx->component_registry ().get_world_components (
           reg::world_component_order::type_id)) {
    if (!desc) {
      continue;
    }
    runtime_ctx->component_registry ().save_world_component_binary (
        writer, registry, desc->type_id);
  }

  runtime_ctx->component_registry ().save_das_components_binary (writer,
                                                                 registry);

  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    const bool has = desc->contains ? desc->contains (registry) : false;
    const std::uint8_t has_raw = has ? 1 : 0;
    writer.write (has_raw);

    if (has && !header.is_prefab) {
      runtime_ctx->singleton_registry ().save_singleton_binary (
          writer, registry, desc->type_id);
    }
  }

  resource_manager::serialization_context::get () = nullptr;
}

void
scene_snapshot_serializer::load_binary_stream (serialize::binary_reader &reader)
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  wsl::log::rsc ()->trace ("Loading scene");
  scene_ref.stop_and_clear ();
  runtime_ctx->event_hub ().clear_connections ();

  scene_header header;

  wsl::log::rsc ()->trace ("Loading header");
  if (!serialize::msgpack_read (reader, header)) {
    wsl::log::rsc ()->error ("Scene file is missing its header");
    resource_manager::serialization_context::get () = nullptr;
    return;
  }

  // Restore the scene name from the saved header
  scene_ref.set_name (header.scene_name);

  // Restore the active camera entity
  scene_ref.camera = entt::entity{ static_cast<entt::id_type> (header.camera) };

  // ---- RESTORE SYSTEMS ----
  wsl::log::rsc ()->trace ("Restoring {} systems", header.systems.size ());
  scene_ref.systems.clear ();

  reg::system_factory_registry &factory
      = runtime_ctx->system_factory_registry ();

  for (const std::string &sys_name : header.systems) {
    if (std::unique_ptr<sys::ecs_system> sys
        = factory.create (sys_name, scene_ref)) {
      scene_ref.add_system_instance (std::move (sys), false);
    } else {
      wsl::log::rsc ()->warn ("Unknown system in scene: {}", sys_name);
    }
  }

  wsl::log::rsc ()->trace ("Loading entities");
  entt::registry &registry = scene_ref.get_registry ();
  auto &storage = registry.storage<entt::entity> ();

  std::uint64_t alive_count = 0;
  std::uint64_t free_list_count = 0;
  reader.read (alive_count);
  reader.read (free_list_count);
  storage.reserve (static_cast<std::size_t> (alive_count));

  entt::entity placeholder{};
  for (std::uint64_t i = 0; i < alive_count; ++i) {
    std::uint32_t raw = 0;
    if (!reader.read (raw)) {
      break;
    }
    entt::entity const e{ static_cast<entt::id_type> (raw) };
    storage.generate (e);
    placeholder = (e > placeholder) ? e : placeholder;
  }

  entt::entity next_after_last = entt::entity{ static_cast<entt::id_type> (
      entt::to_integral (placeholder) + 1U) };
  storage.start_from (next_after_last);
  storage.free_list (static_cast<std::size_t> (free_list_count));

  wsl::log::rsc ()->trace ("Loading components");
  for (const reg::component_registry::descriptor *desc :
       runtime_ctx->component_registry ().get_world_components (
           reg::world_component_order::type_id)) {
    if (!desc) {
      continue;
    }

    runtime_ctx->component_registry ().load_world_component_binary (
        reader, registry, desc->type_id);
  }

  // Load das component data (after regular components, before singletons).
  wsl::log::rsc ()->trace ("Loading das components");
  runtime_ctx->component_registry ().load_das_components_binary (reader,
                                                                 registry);

  wsl::log::rsc ()->trace ("Loading singletons");
  entt::registry &scene_registry = scene_ref.get_registry ();
  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    std::uint8_t has_raw = 0;
    if (!reader.read (has_raw)) {
      break;
    }

    if (has_raw != 0 && !header.is_prefab) {
      runtime_ctx->singleton_registry ().load_singleton_binary (
          reader, scene_registry, desc->type_id);
    }
  }

  post_load_finalize (header);
}

// ------------------------------------------------------------------
// Post-load finalization (shared)
// ------------------------------------------------------------------

void
scene_snapshot_serializer::post_load_finalize (const scene_header &header)
{
  wsl::log::rsc ()->trace ("Restoring {} names", header.entity_names.size ());
  for (const std::pair<uint32_t, std::string> &entry : header.entity_names) {
    entt::entity const e{ static_cast<entt::entity> (entry.first) };
    if (scene_ref.get_registry ().valid (e)) {
      scene_ref.set_entity_name (e, entry.second);
    }
  }

  wsl::log::rsc ()->trace ("Restoring {} connections",
                           header.connections.size ());
  for (const auto &conn : header.connections) {
    if (!runtime_ctx->event_hub ().connect (
            conn.event_type_id, conn.system_type_id, conn.handler_name)) {
      wsl::log::rsc ()->warn (
          "Failed to connect signal {} to system {} handler {} "
          "during scene load",
          conn.event_type_id, conn.system_type_id, conn.handler_name);
    }
  }

  for (const resource_ref_serialized &res : header.autoload) {
    entt::id_type const id = entt::hashed_string{ res.path.c_str () };
    wsl::log::rsc ()->trace ("Autoload resource type={} path={}", (int)res.type,
                             res.path);
    scene_ref.add_resource (res.type, id);
  }

  resource_manager::serialization_context::get () = nullptr;

  // -------------------------------------------------
  // RECREATE PHYSICS OBJECTS AFTER LOAD
  // -------------------------------------------------
  wsl::log::rsc ()->debug ("Recreating physics objects");
  entt::registry &scene_registry = scene_ref.get_registry ();
  if (!scene_registry.ctx ().contains<comp::singl::physics_manager> ()) {
    wsl::log::rsc ()->warn (
        "scene_snapshot_serializer: loaded scene is missing its "
        "physics manager; skipping physics object recreation");
    return;
  }

  // Skip physics recreation in headless/data-only contexts (e.g. CLI).
  // The physics engine would be unnecessarily initialized and there are no
  // rendering or simulation loops to consume the bodies.
  if (runtime_ctx != nullptr && runtime_ctx->is_headless ()) {
    wsl::log::rsc ()->debug (
        "Headless mode, skipping physics object recreation");
    return;
  }

  comp::singl::physics_manager &physics
      = scene_registry.ctx ().get<comp::singl::physics_manager> ();
  phys::engine &engine = physics.ensure_engine ();

  // CRITICAL: Clear all existing bodies before recreating.
  // This prevents 'ghost' bodies if previous cleanup was incomplete.
  engine.clear ();

  // Recreate rigid bodies
  {
    auto view = scene_registry.view<comp::rigid_body> ();
    wsl::log::rsc ()->debug ("Recreating {} rigid bodies",
                             std::distance (view.begin (), view.end ()));
    for (entt::entity e : view) {
      comp::rigid_body &rb = view.get<comp::rigid_body> (e);
      wsl::log::rsc ()->trace ("Creating rigid body for entity {}",
                               (uint32_t)e);

      glm::vec3 world_pos{ 0.0F, 0.0F, 0.0F };
      glm::quat world_rot{ 1.0F, 0.0F, 0.0F, 0.0F };
      glm::vec3 scale{ 1.0F, 1.0F, 1.0F };
      if (auto *wt = scene_registry.try_get<comp::world_transform> (e); wt) {
        glm::mat4 const &wm = wt->value ();
        world_pos = glm::vec3 (wm[3]);
        world_rot = glm::quat_cast (wm);
        scale = glm::vec3 (glm::length (glm::vec3 (wm[0])),
                           glm::length (glm::vec3 (wm[1])),
                           glm::length (glm::vec3 (wm[2])));
      } else if (auto *t = scene_registry.try_get<comp::transform> (e); t) {
        world_pos = (glm::vec3)t->position;
        world_rot = (glm::quat)t->rotation;
        scale = glm::vec3 (t->scale.x (), t->scale.y (), t->scale.z ());
      }

      // Apply rigid_body offset to get the final body world position
      world_pos = world_pos + world_rot * (glm::vec3)rb.position;
      world_rot = world_rot * (glm::quat)rb.rotation;

      rb.create_body (engine, world_pos, world_rot, scale);
    }
  }

  // Recreate character controllers
  {
    auto view
        = scene_registry.view<comp::character_body, comp::world_transform> ();
    wsl::log::rsc ()->debug ("Recreating {} characters",
                             std::distance (view.begin (), view.end ()));
    for (entt::entity e : view) {
      comp::character_body &cb = view.get<comp::character_body> (e);
      comp::world_transform &wt = view.get<comp::world_transform> (e);

      wsl::log::rsc ()->trace ("Creating character for entity {}", (uint32_t)e);
      glm::vec3 const pos = glm::vec3 (static_cast<glm::mat4> (wt.value ())[3]);
      cb.recreate (engine, (math::vec3f)pos);
    }
  }

  // Recreate area sensors
  {
    auto view = scene_registry.view<comp::area> ();
    wsl::log::rsc ()->debug ("Recreating {} area sensors",
                             std::distance (view.begin (), view.end ()));
    for (entt::entity e : view) {
      comp::area &area = view.get<comp::area> (e);
      wsl::log::rsc ()->trace ("Creating area for entity {}", (uint32_t)e);

      glm::vec3 world_pos{ 0.0F, 0.0F, 0.0F };
      glm::quat world_rot{ 1.0F, 0.0F, 0.0F, 0.0F };
      glm::vec3 scale{ 1.0F, 1.0F, 1.0F };
      if (auto *wt = scene_registry.try_get<comp::world_transform> (e); wt) {
        glm::mat4 const &wm = wt->value ();
        world_pos = glm::vec3 (wm[3]);
        world_rot = glm::quat_cast (wm);
        scale = glm::vec3 (glm::length (glm::vec3 (wm[0])),
                           glm::length (glm::vec3 (wm[1])),
                           glm::length (glm::vec3 (wm[2])));
      } else if (auto *t = scene_registry.try_get<comp::transform> (e); t) {
        world_pos = (glm::vec3)t->position;
        world_rot = (glm::quat)t->rotation;
        scale = glm::vec3 (t->scale.x (), t->scale.y (), t->scale.z ());
      }

      // Apply area offset
      world_pos = world_pos + world_rot * (glm::vec3)area.position;
      world_rot = world_rot * (glm::quat)area.rotation;

      area.create_body (engine, world_pos, world_rot, scale);
    }
  }
  // After restoring everything, the scene must be marked initialised so
  // that a subsequent resume() (e.g. after hitting Play in the editor)
  // actually re-activates its systems.
  scene_ref.init ();

  wsl::log::rsc ()->trace ("Scene load finished");
}

// ------------------------------------------------------------------
// Public file/stream API
// ------------------------------------------------------------------

bool
scene_snapshot_serializer::save_binary (const std::string &path) const
{
  std::ofstream file (path, std::ios::binary);
  if (!file) {
    return false;
  }

  serialize::binary_writer writer;
  save_binary_stream (writer);
  const std::string &buffer = writer.buffer ();
  file.write (buffer.data (), static_cast<std::streamsize> (buffer.size ()));
  return true;
}

bool
scene_snapshot_serializer::load_binary (const std::string &path)
{
  wsl::log::rsc ()->trace ("Loading binary scene: {}", path);
  std::ifstream file (path, std::ios::binary);
  if (!file) {
    return false;
  }

  std::stringstream ss;
  ss << file.rdbuf ();
  serialize::binary_reader reader (ss.str ());
  load_binary_stream (reader);
  return true;
}

bool
scene_snapshot_serializer::save_json (const std::string &path) const
{
  std::ofstream file (path);
  if (!file) {
    return false;
  }

  serialize::json_writer writer;
  save_json_doc (writer);
  file << writer.to_string ();
  return true;
}

bool
scene_snapshot_serializer::load_json (const std::string &path)
{
  wsl::log::rsc ()->trace ("Loading JSON scene: {}", path);
  std::ifstream file (path);
  if (!file) {
    return false;
  }

  std::stringstream ss;
  ss << file.rdbuf ();
  serialize::json_reader reader (ss.str ());
  if (!reader.valid ()) {
    wsl::log::rsc ()->error ("Failed to parse scene JSON: {}", path);
    return false;
  }
  load_json_doc (reader);
  return true;
}

bool
scene_snapshot_serializer::save_to_binary_string (std::string &out) const
{
  serialize::binary_writer writer;
  save_binary_stream (writer);
  out = writer.buffer ();
  return true;
}

bool
scene_snapshot_serializer::load_from_binary_string (const std::string &in)
{
  serialize::binary_reader reader (in);
  load_binary_stream (reader);
  return true;
}

} // namespace io

} // namespace rsc

} // namespace wsl
