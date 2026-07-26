#pragma once

#include <string>
#include <filesystem>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

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
  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("name", name), cereal::make_nvp ("author", author),
        cereal::make_nvp ("root_path", root_path),
        cereal::make_nvp ("systems_path", systems_path),
        cereal::make_nvp ("components_path", components_path),
        cereal::make_nvp ("singletons_path", singletons_path),
        cereal::make_nvp ("scenes_path", scenes_path),
        cereal::make_nvp ("models_path", models_path),
        cereal::make_nvp ("images_path", images_path),
        cereal::make_nvp ("cubemaps_path", cubemaps_path),
        cereal::make_nvp ("audio_path", audio_path),
        cereal::make_nvp ("ui_layouts_path", ui_layouts_path),
        cereal::make_nvp ("fonts_path", fonts_path),
        cereal::make_nvp ("shaders_path", shaders_path));

    try {
      ar (cereal::make_nvp ("materials_path", materials_path));
    } catch (const cereal::Exception &) {
      // If the field is missing (e.g. old project), keep the default.
    }

    ar (cereal::make_nvp ("default_scene_path", default_scene_path));
  }
};

} // namespace rsc

} // namespace wsl
