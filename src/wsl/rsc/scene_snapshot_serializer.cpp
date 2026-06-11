#include "scene_snapshot_serializer.hpp"
#include "wsl/log/log.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cstdint>
#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <exception>
#include <fstream>
#include <glm/ext/vector_float3.hpp>
#include <ios>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <entt/entity/snapshot.hpp>

#include "../comp/transform.hpp"
#include "../comp/singl/physics_manager.hpp"
#include "comp/area3d.hpp"
#include "comp/character_body.hpp"
#include "comp/rigid_body.hpp"
#include "comp/world_transform.hpp"
#include "phys/physics_engine.hpp"
#include "../reg/component_registry.hpp"
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

namespace
{

// Serialization helper functions have been moved to the registries.

} // namespace

scene_snapshot_serializer::scene_snapshot_serializer (
    comp::singl::runtime_context *runtime_ctx, scene &scene)
    : scene_ref (scene), runtime_ctx (runtime_ctx)
{
}

void
entity_snapshot_wrapper::save_json (cereal::JSONOutputArchive &ar) const
{
  auto &storage = registry.storage<entt::entity> ();

  ar (cereal::make_nvp ("alive_count",
                        static_cast<std::size_t> (storage.size ())));
  ar (cereal::make_nvp ("free_list_count",
                        static_cast<std::size_t> (storage.free_list ())));

  std::vector<entt::entity> entities;
  entities.reserve (storage.size ());
  for (auto it = storage.rbegin (), last = storage.rend (); it != last; ++it) {
    entities.push_back (*it);
  }
  ar (cereal::make_nvp ("entities", entities));
}

void
entity_snapshot_wrapper::load_json (cereal::JSONInputArchive &ar)
{
  auto &storage = registry.storage<entt::entity> ();

  std::size_t alive_count{};
  std::size_t free_list_count{};
  std::vector<entt::entity> entities;

  ar (cereal::make_nvp ("alive_count", alive_count));
  ar (cereal::make_nvp ("free_list_count", free_list_count));
  ar (cereal::make_nvp ("entities", entities));

  storage.reserve (alive_count);

  entt::entity placeholder{};
  for (entt::entity e : entities) {
    storage.generate (e);
    placeholder = (e > placeholder) ? e : placeholder;
  }

  entt::entity next_after_last = entt::entity{ static_cast<entt::id_type> (
      entt::to_integral (placeholder) + 1u) };
  storage.start_from (next_after_last);
  storage.free_list (free_list_count);
}

template <typename Archive>
void
scene_snapshot_serializer::save_scene (Archive &archive) const
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager;

  scene_header header;
  header.scene_name = scene_ref.get_name ();
  header.is_prefab = is_prefab;

  for (const std::unique_ptr<sys::ecs_system> &sys : scene_ref.systems) {
    header.systems.push_back (sys->get_name ());
  }

  for (const std::pair<const entt::entity, std::string> &entry :
       scene_ref.get_entity_names ()) {
    header.entity_names.emplace_back (
        static_cast<uint32_t> (entt::to_integral (entry.first)), entry.second);
  }

  header.connections = runtime_ctx->signal_hub.get_all_connections ();

  for (const resource_ref &ref : scene_ref.get_load_list ()) {
    std::string path = runtime_ctx->resource_manager.get_path (ref);
    wsl::log::rsc ()->trace ("Serializing autoload type={} path={}",
                             (int)ref.type, path);
    header.autoload.push_back ({ ref.type, path });
  }

  header.camera = static_cast<uint32_t> (entt::to_integral (scene_ref.camera));

  archive (cereal::make_nvp ("header", header));

  entt::registry &registry = scene_ref.get_registry ();

  if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
    archive (
        cereal::make_nvp ("entities", entity_snapshot_wrapper{ registry }));
  } else {
    entt::snapshot const snapshot{ registry };
    snapshot.get<entt::entity> (archive);
  }

  for (const reg::component_registry::descriptor *desc :
       runtime_ctx->component_registry.get_world_components (
           reg::world_component_order::type_id)) {
    if (!desc) {
      continue;
    }

    if constexpr (std::is_same_v<Archive, cereal::BinaryOutputArchive>) {
      runtime_ctx->component_registry.save_world_component_binary (
          archive, registry, desc->type_id);
    } else if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
      runtime_ctx->component_registry.save_world_component_json (
          archive, registry, desc->type_id);
    }
  }

  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry.get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    const bool has = desc && desc->contains ? desc->contains (registry) : false;
    std::string const has_name = "has_singleton_" + desc->type_name;
    archive (cereal::make_nvp (has_name, has));

    if (has && !header.is_prefab) {
      if constexpr (std::is_same_v<Archive, cereal::BinaryOutputArchive>) {
        runtime_ctx->singleton_registry.save_singleton_binary (
            archive, registry, desc->type_id);
      } else if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
        runtime_ctx->singleton_registry.save_singleton_json (archive, registry,
                                                             desc->type_id);
      }
    }
  }

  resource_manager::serialization_context::get () = nullptr;
}

template <typename Archive>
void
scene_snapshot_serializer::load_scene (Archive &archive)
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager;

  wsl::log::rsc ()->trace ("Loading scene");
  scene_ref.stop_and_clear ();
  runtime_ctx->signal_hub.clear_connections ();

  scene_header header;
  wsl::log::rsc ()->trace ("Loading header");
  archive (cereal::make_nvp ("header", header));

  // Restore the scene name from the saved header
  scene_ref.set_name (header.scene_name);

  // Restore the active camera entity
  scene_ref.camera = entt::entity{ static_cast<entt::id_type> (header.camera) };

  // ---- RESTORE SYSTEMS ----
  wsl::log::rsc ()->trace ("Restoring {} systems", header.systems.size ());
  scene_ref.systems.clear ();

  wsl::log::rsc ()->debug (
      "Registered {} connectable handlers",
      runtime_ctx->signal_hub.db
          ? runtime_ctx->signal_hub.db->connectable_handlers.size ()
          : 0);

  reg::system_factory_registry &factory = runtime_ctx->system_factory_registry;

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

  entt::snapshot_loader loader{ registry };
  if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
    archive (
        cereal::make_nvp ("entities", entity_snapshot_wrapper{ registry }));
  } else {
    loader.get<entt::entity> (archive);
  }

  wsl::log::rsc ()->trace ("Loading components");
  for (const reg::component_registry::descriptor *desc :
       runtime_ctx->component_registry.get_world_components (
           reg::world_component_order::type_id)) {
    if (!desc) {
      continue;
    }

    try {
      if constexpr (std::is_same_v<Archive, cereal::BinaryInputArchive>) {
        runtime_ctx->component_registry.load_world_component_binary (
            archive, loader, desc->type_id);
      } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
        runtime_ctx->component_registry.load_world_component_json (
            archive, scene_ref.get_registry (), desc->type_id);
      }
    } catch (const std::exception &) {
      if constexpr (std::is_same_v<Archive, cereal::BinaryInputArchive>) {
        throw;
      }
      /* JSON: missing component data is expected for empty/new scenes.
         Leave the storage at its default (empty) state silently. */
    }
  }

  wsl::log::rsc ()->trace ("Loading singletons");
  entt::registry &scene_registry = scene_ref.get_registry ();
  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry.get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    try {
      bool has = false;
      std::string const has_name = "has_singleton_" + desc->type_name;
      archive (cereal::make_nvp (has_name, has));

      if (has && !header.is_prefab) {
        if constexpr (std::is_same_v<Archive, cereal::BinaryInputArchive>) {
          runtime_ctx->singleton_registry.load_singleton_binary (
              archive, scene_registry, desc->type_id);
        } else if constexpr (std::is_same_v<Archive,
                                            cereal::JSONInputArchive>) {
          runtime_ctx->singleton_registry.load_singleton_json (
              archive, scene_registry, desc->type_id);
        }
      }
    } catch (const std::exception &) {
      if constexpr (std::is_same_v<Archive, cereal::BinaryInputArchive>) {
        throw;
      }
      /* JSON: missing singleton flag or data is expected for empty/new
         scenes. Leave the singleton at its default state silently. */
    }
  }

  wsl::log::rsc ()->trace ("Restoring {} names", header.entity_names.size ());
  for (const std::pair<uint32_t, std::string> &entry : header.entity_names) {
    entt::entity const e{ static_cast<entt::entity> (entry.first) };
    if (scene_ref.get_registry ().valid (e)) {
      scene_ref.set_entity_name (e, entry.second);
    }
  }

  wsl::log::rsc ()->trace ("Restoring {} connections",
                           header.connections.size ());
  // runtime_ctx->signal_hub.clear_connections (); // DON'T CLEAR ALL, additive
  // or handled by scene replacement
  for (const auto &conn : header.connections) {
    if (!runtime_ctx->signal_hub.connect (
            conn.signal_type_id, conn.system_type_id, conn.handler_name,
            conn.source_entity, conn.target_entity,
            &scene_ref.get_registry ())) {
      wsl::log::rsc ()->warn (
          "Failed to connect signal {} to system {} handler {} "
          "during scene load",
          conn.signal_type_id, conn.system_type_id, conn.handler_name);
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
        glm::mat4 const &wm = wt->value;
        world_pos = glm::vec3 (wm[3]);
        world_rot = glm::quat_cast (wm);
        scale = glm::vec3 (glm::length (glm::vec3 (wm[0])),
                           glm::length (glm::vec3 (wm[1])),
                           glm::length (glm::vec3 (wm[2])));
      } else if (auto *t = scene_registry.try_get<comp::transform> (e); t) {
        world_pos = (glm::vec3)t->position;
        world_rot = (glm::quat)t->rotation;
        scale = glm::vec3 (t->scale.x, t->scale.y, t->scale.z);
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
      glm::vec3 const pos = glm::vec3 (static_cast<glm::mat4> (wt.value)[3]);
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
        glm::mat4 const &wm = wt->value;
        world_pos = glm::vec3 (wm[3]);
        world_rot = glm::quat_cast (wm);
        scale = glm::vec3 (glm::length (glm::vec3 (wm[0])),
                           glm::length (glm::vec3 (wm[1])),
                           glm::length (glm::vec3 (wm[2])));
      } else if (auto *t = scene_registry.try_get<comp::transform> (e); t) {
        world_pos = (glm::vec3)t->position;
        world_rot = (glm::quat)t->rotation;
        scale = glm::vec3 (t->scale.x, t->scale.y, t->scale.z);
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

bool
scene_snapshot_serializer::save_binary (const std::string &path) const
{
  std::ofstream file (path, std::ios::binary);
  if (!file) {
    return false;
  }

  cereal::BinaryOutputArchive archive{ file };
  save_scene (archive);
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

  cereal::BinaryInputArchive archive{ file };
  load_scene (archive);
  return true;
}

bool
scene_snapshot_serializer::save_json (const std::string &path) const
{
  std::ofstream file (path);
  if (!file) {
    return false;
  }

  cereal::JSONOutputArchive archive{ file };
  save_scene (archive);
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

  cereal::JSONInputArchive archive{ file };
  load_scene (archive);
  return true;
}

bool
scene_snapshot_serializer::save_to_binary_string (std::string &out) const
{
  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive{ ss };
    save_scene (archive);
  }
  out = ss.str ();
  return true;
}

bool
scene_snapshot_serializer::load_from_binary_string (const std::string &in)
{
  std::stringstream ss (in);
  {
    cereal::BinaryInputArchive archive{ ss };
    load_scene (archive);
  }
  return true;
}

} // namespace io

} // namespace rsc

} // namespace wsl
