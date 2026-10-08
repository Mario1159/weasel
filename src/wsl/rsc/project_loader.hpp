#pragma once

#include "project.hpp"

#ifndef IN_MODULE_INTERFACE
#include <memory>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string_view>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl
{

namespace comp
{
namespace singl
{
class runtime_context;
}
}

namespace rsc
{

/** Aggregates lists of asset file paths categorized by type. */
struct project_assets
{
  /** List of 3D model file paths. */
  std::vector<std::string> models;
  /** List of image file paths. */
  std::vector<std::string> images;
  /** List of cubemap archive or image file paths. */
  std::vector<std::string> cubemaps;
  /** List of scene file paths. */
  std::vector<std::string> scenes;
  /** List of audio file paths. */
  std::vector<std::string> audio;
  /** List of UI layout file paths. */
  std::vector<std::string> ui_layouts;
  /** List of font file paths. */
  std::vector<std::string> fonts;
  /** List of shader file paths. */
  std::vector<std::string> shaders;
  /** List of material file paths. */
  std::vector<std::string> materials;
  /** List of runtime skeleton files (`.skel.ozz`) produced by gltf2ozz. */
  std::vector<std::string> skeletons;
  /** List of runtime animation clip files (`.anim.ozz`). */
  std::vector<std::string> animations;
};

/**
 * Minimal JSON document helpers used by the scene format.
 *
 * The scene file is a single JSON object whose per-component payloads are
 * produced by each component's own serializer. These helpers split an object
 * back into its members without a reflection pass, which is what lets the
 * loader route each value to the right component.
 */
namespace json_document
{
/** One `"key": value` pair of a JSON object, holding the value's raw text. */
struct member
{
  std::string key;
  std::string_view value;
};

/** Splits a JSON object into its members; malformed input yields what parsed.
 */
std::vector<member> object_members (std::string_view object);

/** Finds a member by key, or returns `nullptr` when it is absent. */
const member *find (const std::vector<member> &members, std::string_view key);

/** Index one past the end of the first complete JSON value at \p start. */
std::size_t value_end (std::string_view text, std::size_t start);
} // namespace json_document

/**
 * Scene file naming.
 *
 * Scenes are single valid JSON documents and use the `.wscn` extension. The
 * older `.wscn.json` name and the newline-delimited document stream it held are
 * no longer read: a file without a top-level `format_version` member is
 * rejected rather than partially parsed.
 */
namespace scene_file
{
/** Canonical scene file extension, including the leading dot. */
inline constexpr const char *extension = ".wscn";

/** Strips the scene extension from \p name, if present. */
std::string strip_extension (std::string_view name);

/** Ensures \p name ends in the `.wscn` extension. */
std::string with_extension (std::string_view name);
} // namespace scene_file

/**
 * Project path normalisation.
 *
 * `std::filesystem::absolute()` makes a path absolute but does not remove `.`
 * or `..` components, so a manifest created from `./orbhunt/OrbHunt` records
 * `.../examples/./orbhunt/OrbHunt`. That string is then compared literally
 * against paths built from other inputs, and the mismatch shows up much later
 * as "file not found" against a path that visibly exists.
 */
namespace project_path
{
/**
 * Returns \p path as a normalised absolute path with no `.` or `..` segments
 * and no trailing separator.
 *
 * Unlike `fs::canonical`, this works for a path that does not exist yet, so it
 * is safe to call before creating a project directory. Symlinks in the
 * existing prefix are resolved, so the same directory always yields the same
 * string.
 */
std::string normalize (const std::filesystem::path &path);
} // namespace project_path

/** Responsible for creating, loading, and scanning Weasel projects. */
class project_loader
{
public:
  /** The default filename for project manifest files. */
  static constexpr const char *manifest_file = "wslpro.json";

  /**
   * Constructs a project loader.
   * :param runtime_ctx: Pointer to the runtime context.
   */
  explicit project_loader (comp::singl::runtime_context *runtime_ctx = nullptr)
      : m_runtime_ctx (runtime_ctx)
  {
  }

  /**
   * Creates a new project on disk based on the provided configuration.
   * :param proj: The project configuration to create.
   * :return: `true` if creation succeeded, otherwise `false`.
   */
  bool create (const project &proj) const;

  /**
   * Loads a project configuration from the specified path.
   * :param path: Path to the project manifest file or project directory.
   * :return: Shared pointer to the loaded project, or `nullptr` if loading
   * failed.
   */
  static std::shared_ptr<project> load (const std::string &path);

  /**
   * Scans the project's resource directories for available assets.
   * :param proj: The project configuration to scan.
   * :return: A `project_assets` object containing the discovered asset paths.
   */
  static project_assets scan_assets (const project &proj);

private:
  comp::singl::runtime_context *m_runtime_ctx;
};

} // namespace rsc

} // namespace wsl
