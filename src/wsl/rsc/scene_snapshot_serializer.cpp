#include "scene_snapshot_serializer.hpp"

#include "../log/log.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <entt/core/fwd.hpp>
#include <unordered_set>
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
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "../comp/transform.hpp"
#include "../comp/singl/physics_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/area3d.hpp"
#include "../comp/character_body.hpp"
#include "../comp/rigid_body.hpp"
#include "../comp/world_transform.hpp"
#include "../phys/physics_engine.hpp"
#include "../reg/component_registry.hpp"
#include "resource_manager.hpp"
#include "resource_ref.hpp"
#include "scene.hpp"
#include "../reg/singleton_registry.hpp"
#include "../reg/system_factory_registry.hpp"
#include "../serialize/serialize.hpp"
#include "../sys/system.hpp"

namespace wsl
{

namespace rsc
{

namespace io
{

namespace
{

/** Component-block format that carries an explicit document manifest. */
constexpr uint32_t tagged_component_stream_version = 1;

/**
 * Manifest document written immediately after the scene-data document.
 *
 * The manifest lives in its own document rather than in `scene_header`
 * because rfl treats plain struct fields as required: adding a field to
 * `scene_header` would make every previously saved scene fail to parse,
 * leaving the whole header (entity names, entities, systems) empty. A
 * separate document is simply absent from legacy files, which are then
 * recognized as untagged.
 */
struct component_manifest
{
  /** Discriminates the manifest from a component document. */
  uint32_t version = tagged_component_stream_version;
  /** Ordered world component type names present in the component block. */
  std::vector<std::string> component_order;
};

/**
 * World components that existed when the untagged (v1) component-block
 * format was introduced.
 *
 * A v1 file contains exactly one document per component registered at the
 * time it was written, ordered by stable type id, and the documents carry no
 * type tag. Such a file can therefore only be replayed by consuming one
 * document per component *of that same set*: consuming a document for a
 * component added later shifts every following document and silently drops
 * the real data. Components introduced after v1 must never be added here --
 * new files record a manifest in `scene_header::component_order` instead.
 */
constexpr std::array<std::string_view, 17> legacy_v1_world_components = {
  "hierarchy",
  "world_transform",
  "transform",
  "model_instance_3d",
  "camera",
  "camera_2d",
  "point_light",
  "spot_light",
  "directional_light",
  "rigid_body",
  "area",
  "character_body",
  "audio",
  "prefab_instance",
  "sprite_2d",
  "subviewport",
  "transform_2d",
};

/**
 * Returns the trailing `short_name` of a fully qualified type name, with the
 * same trailing-artifact trimming that `comp::stable_type_id` applies, so the
 * comparison is stable across compilers.
 */
std::string_view
normalized_short_type_name (std::string_view type_name)
{
  std::size_t const separator = type_name.rfind ("::");
  std::string_view name = separator == std::string_view::npos
                              ? type_name
                              : type_name.substr (separator + 2);
  while (!name.empty ()
         && (name.back () == ']' || name.back () == ' ' || name.back () == '\n'
             || name.back () == '\r')) {
    name.remove_suffix (1);
  }
  return name;
}

/** Whether \p type_name belongs to the frozen v1 component set. */
bool
is_legacy_v1_component (std::string_view type_name)
{
  std::string_view const name = normalized_short_type_name (type_name);
  return std::find (legacy_v1_world_components.begin (),
                    legacy_v1_world_components.end (), name)
         != legacy_v1_world_components.end ();
}

} // namespace

scene_snapshot_serializer::scene_snapshot_serializer (
    comp::singl::runtime_context *runtime_ctx, scene &scene)
    : scene_ref (scene), runtime_ctx (runtime_ctx)
{
}

void
scene_snapshot_serializer::save (serialize::json_writer &writer) const
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  scene_header header;
  header.scene_name = scene_ref.get_name ();
  header.is_prefab = is_prefab;

  for (const std::unique_ptr<sys::ecs_system> &sys : scene_ref.systems) {
    header.systems.push_back (sys->get_name ());
  }

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

  // Only persist connections for systems in this scene (plus global
  // non-scene-system handlers such as runtime_context).  Saving all
  // global connections causes spurious warnings when loading scenes
  // that do not contain the referenced systems.
  std::unordered_set<entt::id_type> scene_system_ids;
  for (const auto &sys : scene_ref.systems) {
    scene_system_ids.insert (sys->get_type_id ());
  }
  header.connections = runtime_ctx->event_hub ().get_connections_for_systems (
      scene_system_ids);

  for (const resource_ref &ref : scene_ref.get_load_list ()) {
    std::string path = runtime_ctx->resource_manager ().get_path (ref);
    header.autoload.push_back ({ ref.type, path });
  }

  header.camera = static_cast<uint32_t> (entt::to_integral (scene_ref.camera));

  entt::registry &registry = scene_ref.get_registry ();

  struct scene_data
  {
    scene_header header;
    std::vector<entt::entity> entities;
  };

  scene_data data;
  data.header = std::move (header);

  auto &storage = registry.storage<entt::entity> ();
  data.entities.reserve (storage.size ());
  for (auto it = storage.rbegin (), last = storage.rend (); it != last; ++it) {
    data.entities.push_back (*it);
  }

  // Collect the component list once: the manifest must describe exactly the
  // documents that are written below, in the same order.
  const std::vector<const reg::component_registry::descriptor *>
      world_components
      = runtime_ctx->component_registry ().get_world_components (
          reg::world_component_order::type_id);

  component_manifest manifest;
  manifest.component_order.reserve (world_components.size ());
  for (const reg::component_registry::descriptor *desc : world_components) {
    if (desc == nullptr) {
      continue;
    }
    manifest.component_order.push_back (desc->type_name);
  }

  writer.write (data);
  writer.write (manifest);

  for (const reg::component_registry::descriptor *desc : world_components) {
    if (!desc) {
      continue;
    }
    runtime_ctx->component_registry ().save_world_component_json (
        writer, registry, desc->type_id);
  }

  runtime_ctx->component_registry ().save_das_components_json (writer,
                                                               registry);

  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    const bool has = desc && desc->contains ? desc->contains (registry) : false;
    if (has && !data.header.is_prefab) {
      runtime_ctx->singleton_registry ().save_singleton_json (writer, registry,
                                                              desc->type_id);
    }
  }

  resource_manager::serialization_context::get () = nullptr;
}

void
scene_snapshot_serializer::load (serialize::json_reader &reader)
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  wsl::log::rsc ()->trace ("Loading scene");
  scene_ref.stop_and_clear ();
  runtime_ctx->event_hub ().clear_connections ();

  struct scene_data
  {
    scene_header header;
    std::vector<entt::entity> entities;
  };

  scene_data data;
  reader.read (data);

  scene_header &header = data.header;

  scene_ref.set_name (header.scene_name);
  scene_ref.camera = entt::entity{ static_cast<entt::id_type> (header.camera) };

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
  storage.reserve (data.entities.size ());
  entt::entity placeholder{};
  for (entt::entity e : data.entities) {
    storage.generate (e);
    placeholder = (e > placeholder) ? e : placeholder;
  }
  entt::entity next_after_last = entt::entity{ static_cast<entt::id_type> (
      entt::to_integral (placeholder) + 1U) };
  storage.start_from (next_after_last);

  wsl::log::rsc ()->trace ("Loading components");
  {
    reg::component_registry &components = runtime_ctx->component_registry ();

    // A tagged stream carries a manifest document right after the scene data.
    // Legacy files have no such document, so the peeked text is probed first
    // and the stream is only advanced when the manifest actually matched --
    // `read` would consume the document even on a failed parse.
    component_manifest manifest;
    bool tagged = false;
    {
      std::string const probe (reader.peek ());
      if (!probe.empty ()) {
        component_manifest parsed;
        std::string error;
        if (wsl::serialize::json_read (probe, parsed, &error)
            && parsed.version >= tagged_component_stream_version
            && !parsed.component_order.empty ()) {
          manifest = std::move (parsed);
          reader.skip ();
          tagged = true;
        }
      }
    }

    if (tagged) {
      // The file lists its own components, so adding or removing a component
      // in this build cannot shift the stream.
      for (const std::string &name : manifest.component_order) {
        const reg::component_registry::descriptor *desc
            = components.find_world_component (name);
        if (desc == nullptr) {
          // Component type is gone: consume its document so the remaining
          // documents stay aligned.
          if (!reader.skip ()) {
            break;
          }
          continue;
        }

        try {
          components.load_world_component_json (reader, registry,
                                                desc->type_id);
        } catch (const std::exception &) {
          // JSON: missing component data is expected for empty/new scenes.
        }
      }
    } else {
      // Legacy untagged stream: one document per v1 component, in the same
      // type-id order the writer used. Components added after v1 must not
      // consume a document here.
      for (const reg::component_registry::descriptor *desc :
           components.get_world_components (
               reg::world_component_order::type_id)) {
        if (desc == nullptr || !is_legacy_v1_component (desc->type_name)) {
          continue;
        }

        try {
          components.load_world_component_json (reader, registry,
                                                desc->type_id);
        } catch (const std::exception &) {
          // JSON: missing component data is expected for empty/new scenes.
        }
      }
    }
  }

  wsl::log::rsc ()->trace ("Loading das components");
  try {
    runtime_ctx->component_registry ().load_das_components_json (reader,
                                                                 registry);
  } catch (const std::exception &) {
    // Missing das component data is expected
  }

  wsl::log::rsc ()->trace ("Loading singletons");
  entt::registry &scene_registry = scene_ref.get_registry ();
  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    try {
      if (!header.is_prefab) {
        runtime_ctx->singleton_registry ().load_singleton_json (
            reader, scene_registry, desc->type_id);
      }
    } catch (const std::exception &) {
      // JSON: missing singleton data is expected for empty/new scenes.
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

  wsl::log::rsc ()->debug ("Recreating physics objects");
  if (!scene_registry.ctx ().contains<comp::singl::physics_manager> ()) {
    wsl::log::rsc ()->warn (
        "scene_snapshot_serializer: loaded scene is missing its "
        "physics manager; skipping physics object recreation");
    return;
  }

  if (runtime_ctx != nullptr && runtime_ctx->is_headless ()) {
    wsl::log::rsc ()->debug (
        "Headless mode, skipping physics object recreation");
    return;
  }

  comp::singl::physics_manager &physics
      = scene_registry.ctx ().get<comp::singl::physics_manager> ();
  phys::engine &engine = physics.ensure_engine ();

  engine.clear ();

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

      world_pos = world_pos + world_rot * (glm::vec3)rb.position;
      world_rot = world_rot * (glm::quat)rb.rotation;

      rb.create_body (engine, world_pos, world_rot, scale);
    }
  }

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

      world_pos = world_pos + world_rot * (glm::vec3)area.position;
      world_rot = world_rot * (glm::quat)area.rotation;

      area.create_body (engine, world_pos, world_rot, scale);
    }
  }

  scene_ref.init ();

  wsl::log::rsc ()->trace ("Scene load finished");
}

void
scene_snapshot_serializer::save_binary (serialize::binary_writer &writer) const
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  scene_header header;
  header.scene_name = scene_ref.get_name ();
  header.is_prefab = is_prefab;

  for (const std::unique_ptr<sys::ecs_system> &sys : scene_ref.systems) {
    header.systems.push_back (sys->get_name ());
  }

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

  // Only persist connections for systems in this scene (plus global
  // non-scene-system handlers such as runtime_context).  See save() for
  // rationale.
  std::unordered_set<entt::id_type> scene_system_ids;
  for (const auto &sys : scene_ref.systems) {
    scene_system_ids.insert (sys->get_type_id ());
  }
  header.connections = runtime_ctx->event_hub ().get_connections_for_systems (
      scene_system_ids);

  for (const resource_ref &ref : scene_ref.get_load_list ()) {
    std::string path = runtime_ctx->resource_manager ().get_path (ref);
    header.autoload.push_back ({ ref.type, path });
  }

  header.camera = static_cast<uint32_t> (entt::to_integral (scene_ref.camera));

  entt::registry &registry = scene_ref.get_registry ();

  struct scene_data
  {
    scene_header header;
    std::vector<entt::entity> entities;
  };

  scene_data data;
  data.header = std::move (header);

  auto &storage = registry.storage<entt::entity> ();
  data.entities.reserve (storage.size ());
  for (auto it = storage.rbegin (), last = storage.rend (); it != last; ++it) {
    data.entities.push_back (*it);
  }

  const std::vector<const reg::component_registry::descriptor *>
      world_components
      = runtime_ctx->component_registry ().get_world_components (
          reg::world_component_order::type_id);

  component_manifest manifest;
  manifest.component_order.reserve (world_components.size ());
  for (const reg::component_registry::descriptor *desc : world_components) {
    if (desc == nullptr) {
      continue;
    }
    manifest.component_order.push_back (desc->type_name);
  }

  writer.write (data);
  writer.write (manifest);

  for (const reg::component_registry::descriptor *desc : world_components) {
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

    const bool has = desc && desc->contains ? desc->contains (registry) : false;
    if (has && !data.header.is_prefab) {
      runtime_ctx->singleton_registry ().save_singleton_binary (
          writer, registry, desc->type_id);
    }
  }

  resource_manager::serialization_context::get () = nullptr;
}

void
scene_snapshot_serializer::load_binary (serialize::binary_reader &reader)
{
  resource_manager::serialization_context::get ()
      = &runtime_ctx->resource_manager ();

  wsl::log::rsc ()->trace ("Loading scene");
  scene_ref.stop_and_clear ();
  runtime_ctx->event_hub ().clear_connections ();

  struct scene_data
  {
    scene_header header;
    std::vector<entt::entity> entities;
  };

  scene_data data;
  reader.read (data);

  scene_header &header = data.header;

  scene_ref.set_name (header.scene_name);
  scene_ref.camera = entt::entity{ static_cast<entt::id_type> (header.camera) };

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
  storage.reserve (data.entities.size ());
  entt::entity placeholder{};
  for (entt::entity e : data.entities) {
    storage.generate (e);
    placeholder = (e > placeholder) ? e : placeholder;
  }
  entt::entity next_after_last = entt::entity{ static_cast<entt::id_type> (
      entt::to_integral (placeholder) + 1U) };
  storage.start_from (next_after_last);

  wsl::log::rsc ()->trace ("Loading components");
  {
    reg::component_registry &components = runtime_ctx->component_registry ();

    // Peek before consuming: a legacy stream has no manifest document, and
    // `read` would consume the first component document even on failure.
    component_manifest manifest;
    bool tagged = false;
    {
      auto const [bytes, length] = reader.peek ();
      if (bytes != nullptr && length > 0) {
        serialize::binary_reader probe (bytes, length);
        component_manifest parsed;
        if (probe.read (parsed)
            && parsed.version >= tagged_component_stream_version
            && !parsed.component_order.empty ()) {
          manifest = std::move (parsed);
          reader.skip ();
          tagged = true;
        }
      }
    }

    if (tagged) {
      for (const std::string &name : manifest.component_order) {
        const reg::component_registry::descriptor *desc
            = components.find_world_component (name);
        if (desc == nullptr) {
          if (!reader.skip ()) {
            break;
          }
          continue;
        }

        try {
          components.load_world_component_binary (reader, registry,
                                                  desc->type_id);
        } catch (const std::exception &) {
          throw;
        }
      }
    } else {
      // Legacy untagged stream: only the frozen v1 component set has a
      // document, in type-id order.
      for (const reg::component_registry::descriptor *desc :
           components.get_world_components (
               reg::world_component_order::type_id)) {
        if (desc == nullptr || !is_legacy_v1_component (desc->type_name)) {
          continue;
        }

        try {
          components.load_world_component_binary (reader, registry,
                                                  desc->type_id);
        } catch (const std::exception &) {
          throw;
        }
      }
    }
  }

  wsl::log::rsc ()->trace ("Loading das components");
  try {
    runtime_ctx->component_registry ().load_das_components_binary (reader,
                                                                   registry);
  } catch (const std::exception &) {
    throw;
  }

  wsl::log::rsc ()->trace ("Loading singletons");
  entt::registry &scene_registry = scene_ref.get_registry ();
  for (const reg::singleton_registry::descriptor *desc :
       runtime_ctx->singleton_registry ().get_singleton_components (
           reg::singleton_component_order::type_id)) {
    if (!desc || !desc->serialize_with_scene) {
      continue;
    }

    try {
      if (!header.is_prefab) {
        runtime_ctx->singleton_registry ().load_singleton_binary (
            reader, scene_registry, desc->type_id);
      }
    } catch (const std::exception &) {
      throw;
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

  wsl::log::rsc ()->debug ("Recreating physics objects");
  if (!scene_registry.ctx ().contains<comp::singl::physics_manager> ()) {
    wsl::log::rsc ()->warn (
        "scene_snapshot_serializer: loaded scene is missing its "
        "physics manager; skipping physics object recreation");
    return;
  }

  if (runtime_ctx != nullptr && runtime_ctx->is_headless ()) {
    wsl::log::rsc ()->debug (
        "Headless mode, skipping physics object recreation");
    return;
  }

  comp::singl::physics_manager &physics
      = scene_registry.ctx ().get<comp::singl::physics_manager> ();
  phys::engine &engine = physics.ensure_engine ();

  engine.clear ();

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

      world_pos = world_pos + world_rot * (glm::vec3)rb.position;
      world_rot = world_rot * (glm::quat)rb.rotation;

      rb.create_body (engine, world_pos, world_rot, scale);
    }
  }

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

      world_pos = world_pos + world_rot * (glm::vec3)area.position;
      world_rot = world_rot * (glm::quat)area.rotation;

      area.create_body (engine, world_pos, world_rot, scale);
    }
  }

  scene_ref.init ();

  wsl::log::rsc ()->trace ("Scene load finished");
}

bool
scene_snapshot_serializer::save_binary (const std::string &path) const
{
  serialize::binary_writer writer;
  save_binary (writer);
  std::ofstream file (path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write (reinterpret_cast<const char *> (writer.bytes.data ()),
              static_cast<std::streamsize> (writer.bytes.size ()));
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
  file.seekg (0, std::ios::end);
  auto size = file.tellg ();
  file.seekg (0, std::ios::beg);
  std::vector<std::uint8_t> bytes (static_cast<size_t> (size));
  file.read (reinterpret_cast<char *> (bytes.data ()), size);
  serialize::binary_reader reader{ bytes };
  load_binary (reader);
  return true;
}

bool
scene_snapshot_serializer::save_json (const std::string &path) const
{
  serialize::json_writer writer;
  save (writer);
  std::ofstream file (path);
  if (!file) {
    return false;
  }
  file << writer.json;
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
  // Hold the buffer in a named string: json_reader stores a view into it
  // for the whole load, so a temporary would dangle.
  const std::string content = ss.str ();
  serialize::json_reader reader{ content };
  load (reader);
  return true;
}

bool
scene_snapshot_serializer::save_to_binary_string (std::string &out) const
{
  serialize::binary_writer writer;
  save_binary (writer);
  out.assign (reinterpret_cast<const char *> (writer.bytes.data ()),
              writer.bytes.size ());
  return true;
}

bool
scene_snapshot_serializer::load_from_binary_string (const std::string &in)
{
  std::vector<std::uint8_t> bytes (in.begin (), in.end ());
  serialize::binary_reader reader{ bytes };
  load_binary (reader);
  return true;
}

} // namespace io

} // namespace rsc

} // namespace wsl
