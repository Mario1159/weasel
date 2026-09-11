// scene_snapshot_serializer.hpp
#pragma once

#ifndef IN_MODULE_INTERFACE
#include <string>
#endif

#ifndef IN_MODULE_INTERFACE
#include <entt/entity/registry.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <type_traits>
#endif

#ifndef IN_MODULE_INTERFACE
#include <entt/core/hashed_string.hpp>
#endif

#include "../comp/component_meta.hpp"
#include "../serialize/types.hpp"
#include "scene.hpp"

namespace wsl
{

namespace rsc
{

namespace io
{

/** Represents a serialized resource reference used within scene files. */
struct resource_ref_serialized
{
  /** The type of the referenced resource. */
  resource_type type;
  /** The path to the resource, relative to the project or engine root. */
  std::string path;
};

/** Contains metadata and structural information for a scene file. */
struct scene_header
{
  /** The name of the scene. */
  std::string scene_name;
  /** Whether this scene should be treated as a prefab. */
  bool is_prefab = false;
  /** List of system names attached to the scene. */
  std::vector<std::string> systems;
  /** List of entity names and their serialized IDs. */
  std::vector<std::pair<uint32_t, std::string>> entity_names;
  /** Signal connections between entities and systems. */
  std::vector<event::event_connection_data> connections;
  /** Resources that should be automatically loaded with the scene. */
  std::vector<resource_ref_serialized> autoload;
  /** The active camera entity in this scene. */
  uint32_t camera = entt::null;
};

/**
 * Handles serialization and deserialization of scene snapshots.
 *
 * This class uses rfl-based serialization to save and load the complete state
 * of a scene, including entities, components, and singletons.
 */
class scene_snapshot_serializer
{
public:
  /*explicit*/ scene_snapshot_serializer (
      comp::singl::runtime_context *runtime_ctx, scene &scene);

  /** Saves the scene to a binary file at the specified path. */
  bool save_binary (const std::string &path) const;
  /** Loads the scene from a binary file at the specified path. */
  bool load_binary (const std::string &path);

  /** Saves the scene to a JSON file at the specified path. */
  bool save_json (const std::string &path) const;
  /** Loads the scene from a JSON file at the specified path. */
  bool load_json (const std::string &path);

  /** Serializes the scene into a binary string. */
  bool save_to_binary_string (std::string &out) const;
  /** Deserializes the scene from a binary string. */
  bool load_from_binary_string (const std::string &in);

  /** Reference to the scene being managed. */
  scene &scene_ref;
  /** Pointer to the runtime context. */
  comp::singl::runtime_context *runtime_ctx;
  /** Whether the scene is being serialized as a prefab. */
  bool is_prefab = false;

private:
  void save (serialize::json_writer &writer) const;
  void load (serialize::json_reader &reader);
  void save_binary (serialize::binary_writer &writer) const;
  void load_binary (serialize::binary_reader &reader);
};

} // namespace io

} // namespace rsc

} // namespace wsl
