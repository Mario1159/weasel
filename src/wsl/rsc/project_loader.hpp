#pragma once

#include "project.hpp"

#include <memory>
#include <string>
#include <vector>

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
};

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
