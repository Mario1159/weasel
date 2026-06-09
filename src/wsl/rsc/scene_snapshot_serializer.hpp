// scene_snapshot_serializer.hpp
#pragma once

#include <string>

#include <entt/entity/registry.hpp>
#include <entt/entity/snapshot.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <type_traits>

#include <entt/core/hashed_string.hpp>

#include "../comp/component_meta.hpp"
#include "scene.hpp"

namespace wsl
{

namespace rsc
{

namespace io
{

/*!
 * \brief Represents a serialized resource reference used within scene files.
 */
struct resource_ref_serialized
{
  //! The type of the referenced resource.
  resource_type type;
  //! The path to the resource, relative to the project or engine root.
  std::string path;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("type", type), cereal::make_nvp ("path", path));
  }
};

/*!
 * \brief Contains metadata and structural information for a scene file.
 */
struct scene_header
{
  //! The name of the scene.
  std::string scene_name;
  //! Whether this scene should be treated as a prefab.
  bool is_prefab = false;
  //! List of system names attached to the scene.
  std::vector<std::string> systems;
  //! List of entity names and their serialized IDs.
  std::vector<std::pair<uint32_t, std::string>> entity_names;
  //! Signal connections between entities and systems.
  std::vector<reg::sig::signal_connection_data> connections;
  //! Resources that should be automatically loaded with the scene.
  std::vector<resource_ref_serialized> autoload;
  //! The active camera entity in this scene.
  uint32_t camera = entt::null;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("scene_name", scene_name),
        cereal::make_nvp ("is_prefab", is_prefab),
        cereal::make_nvp ("systems", systems),
        cereal::make_nvp ("entity_names", entity_names),
        cereal::make_nvp ("connections", connections),
        cereal::make_nvp ("autoload", autoload));
    uint32_t const camera_default = entt::null;
    wsl::comp::serialize_field_if_diff (ar, "camera", camera, camera_default);
  }
};

/*!
 * \brief Wrapper that adapts entt::snapshot for entt::entity into Cereal
 *        archives with human-readable field names.
 *
 * The default EnTT snapshot writes the entity storage as a flat sequence of
 * unnamed values, which Cereal's JSON output renders as auto-incremented
 * "value0", "value1", ... names. This wrapper re-implements the snapshot
 * protocol for JSON archives so the produced JSON is:
 *
 *   {
 *     "alive_count": <number>,
 *     "free_list_count": <number>,
 *     "entities": [ <id>, <id>, ... ]
 *   }
 *
 * Binary archives continue to use EnTT's snapshot directly.
 */
struct entity_snapshot_wrapper
{
  entt::registry &registry;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    if constexpr (std::is_same_v<Archive, cereal::JSONOutputArchive>) {
      save_json (ar);
    } else if constexpr (std::is_same_v<Archive, cereal::JSONInputArchive>) {
      load_json (ar);
    } else {
      entt::snapshot const snapshot{ registry };
      snapshot.get<entt::entity> (ar);
    }
  }

private:
  void save_json (cereal::JSONOutputArchive &ar) const;
  void load_json (cereal::JSONInputArchive &ar);
};

/*!
 * \brief Handles serialization and deserialization of scene snapshots.
 *
 * This class uses EnTT snapshots and Cereal archives to save and load
 * the complete state of a scene, including entities, components, and
 * singletons.
 */
class scene_snapshot_serializer
{
public:
  /*!
   * \brief Constructs a serializer for a specific scene and runtime context.
   * \param runtime_ctx Pointer to the runtime context.
   * \param scene Reference to the scene to be serialized.
   */
  /*explicit*/ scene_snapshot_serializer (
      comp::singl::runtime_context *runtime_ctx, scene &scene);

  /*! \brief Saves the scene to a binary file at the specified path. */
  bool save_binary (const std::string &path) const;
  /*! \brief Loads the scene from a binary file at the specified path. */
  bool load_binary (const std::string &path);

  /*! \brief Saves the scene to a JSON file at the specified path. */
  bool save_json (const std::string &path) const;
  /*! \brief Loads the scene from a JSON file at the specified path. */
  bool load_json (const std::string &path);

  /*! \brief Serializes the scene into a binary string. */
  bool save_to_binary_string (std::string &out) const;
  /*! \brief Deserializes the scene from a binary string. */
  bool load_from_binary_string (const std::string &in);

  //! Reference to the scene being managed.
  scene &scene_ref;
  //! Pointer to the runtime context.
  comp::singl::runtime_context *runtime_ctx;
  //! Whether the scene is being serialized as a prefab.
  bool is_prefab = false;

  /*! \brief Internal implementation for saving the scene to an archive. */
  template <typename Archive> void save_scene (Archive &archive) const;

  /*! \brief Internal implementation for loading the scene from an archive. */
  template <typename Archive> void load_scene (Archive &archive);
};

} // namespace io

} // namespace rsc

} // namespace wsl
