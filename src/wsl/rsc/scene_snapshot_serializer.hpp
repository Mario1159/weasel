// scene_snapshot_serializer.hpp
#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <string>
#endif

#if !defined(WSL_MODULE_BUILD)
#include <entt/entity/registry.hpp>
#endif

// Serialization backends are implementation details (see wsl/serialize).
namespace wsl::serialize
{
class json_writer;
class json_reader;
class binary_writer;
class binary_reader;
}

#if !defined(WSL_MODULE_BUILD)
#include <entt/core/hashed_string.hpp>
#endif

#if !defined(WSL_MODULE_BUILD)
#include "scene.hpp"
#endif

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
 * This class saves and loads the complete state of a scene, including
 * entities, components, and singletons. The JSON format is human readable;
 * the binary format mirrors the same structure with msgpack payloads.
 */
class scene_snapshot_serializer
{
public:
  /**
   * Constructs a serializer for a specific scene and runtime context.
   * :param runtime_ctx: Pointer to the runtime context.
   * :param scene: Reference to the scene to be serialized.
   */
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
  /** Serializes the scene into a JSON document. */
  void save_json_doc (serialize::json_writer &writer) const;

  /** Restores the scene from a JSON document. */
  void load_json_doc (serialize::json_reader &reader);

  /** Serializes the scene into a binary stream. */
  void save_binary_stream (serialize::binary_writer &writer) const;

  /** Restores the scene from a binary stream. */
  void load_binary_stream (serialize::binary_reader &reader);

  /** Builds the scene header from the current scene state. */
  scene_header build_header () const;

  /** Shared post-load finalization (names, connections, physics, ...). */
  void post_load_finalize (const scene_header &header);
};

} // namespace io

} // namespace rsc

} // namespace wsl
