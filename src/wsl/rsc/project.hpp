#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <string>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <filesystem>
#endif

namespace wsl
{

namespace rsc
{

/**
 * Represents a project configuration, including metadata and resource
 * paths.
 */
struct project
{

  // -------- Metadata --------
  /** The name of the project. */
  std::string name;
  /** The author of the project. */
  std::string author;

  // -------- Resource Paths --------
  /** The absolute root path of the project folder. */
  std::string root_path; // base project folder

  /** Path to the directory containing systems. */
  std::string systems_path;
  /** Path to the directory containing components. */
  std::string components_path;
  /** Path to the directory containing singletons. */
  std::string singletons_path;

  /** Path to the directory containing scene files. */
  std::string scenes_path;
  /** Path to the directory containing 3D models. */
  std::string models_path;
  /** Path to the directory containing image files. */
  std::string images_path;
  /** Path to the directory containing cubemap archives or images. */
  std::string cubemaps_path;
  /** Path to the directory containing audio files. */
  std::string audio_path;
  /** Path to the directory containing UI layout files. */
  std::string ui_layouts_path;
  /** Path to the directory containing font files. */
  std::string fonts_path;
  /** Path to the directory containing shader files. */
  std::string shaders_path;
  /** Path to the directory containing material files. */
  std::string materials_path = "materials";

  // -------- Default Scene --------
  /** Path to the default scene file, relative to `scenes_path`. */
  std::string default_scene_path;

  // -------- Serialization --------
  /**
 * Serializes or deserializes the project configuration.
 * :param Archive: The archive type.
 * :param ar: The archive to use for serialization.
 */
};

} // namespace rsc

} // namespace wsl
