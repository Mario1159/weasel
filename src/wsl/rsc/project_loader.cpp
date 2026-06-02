#include "project_loader.hpp"

#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include "comp/singl/rendering_manager.hpp"
#include "rsc/project.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "rsc/resource_ref.hpp"
#include <string>
#include <vector>
#include "wsl/log/log.hpp"

#include "scene_snapshot_serializer.hpp"
#include "scene.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/model_instance_3d.hpp"

namespace wsl
{

namespace fs = std::filesystem;

bool
rsc::project_loader::create (const project &proj) const
{
  fs::create_directories (proj.root_path);

  const auto create_dir = [&] (const std::string &relative_path) {
    fs::create_directories (fs::path (proj.root_path) / relative_path);
  };

  create_dir (proj.models_path);
  create_dir (proj.images_path);
  create_dir (proj.scenes_path);
  create_dir (proj.cubemaps_path);
  create_dir (proj.systems_path);
  create_dir (proj.components_path);
  create_dir (proj.singletons_path);
  create_dir (proj.audio_path);
  create_dir (proj.ui_layouts_path);
  create_dir (proj.fonts_path);
  create_dir (proj.shaders_path);
  fs::create_directories (fs::path (proj.root_path) / "src");

  const fs::path project_file = fs::path (proj.root_path) / manifest_file;
  std::ofstream file (project_file);
  if (!file) {
    wsl::log::rsc ()->error ("Failed to create project file: {}",
                             project_file.string ());
    return false;
  }

  // Generate default scene
  const std::string default_scene_rel = proj.scenes_path + "/main.wscn.json";
  const fs::path scene_file = fs::path (proj.root_path) / default_scene_rel;

  if (m_runtime_ctx != nullptr) {
    rsc::scene temp_scene (m_runtime_ctx, nullptr, "Main Scene");

    auto &reg = temp_scene.get_registry ();
    auto &rendering = reg.ctx ().emplace<comp::singl::rendering_manager> ();
    rendering.skybox = { rsc::builtin_skybox_procedural };

    const rsc::model_id builtin_cube_id
        = m_runtime_ctx->resource_manager.register_model ("builtin://cube");
    const rsc::model_id builtin_sphere_id
        = m_runtime_ctx->resource_manager.register_model ("builtin://sphere");
    const rsc::cubemap_id builtin_skybox_id
        = m_runtime_ctx->resource_manager.register_cubemap (
            "builtin/skybox_procedural");
    temp_scene.add_resource (io::resource_type::model, builtin_cube_id.value);
    temp_scene.add_resource (io::resource_type::model, builtin_sphere_id.value);
    temp_scene.add_resource (io::resource_type::cubemap,
                             builtin_skybox_id.value);

    auto sample_cube = reg.create ();
    temp_scene.set_entity_name (sample_cube, "Sample Cube");
    reg.emplace<comp::hierarchy> (sample_cube);
    reg.emplace<comp::transform> (sample_cube, glm::vec3 (0.0F, 0.0F, 0.0F));
    reg.emplace<comp::world_transform> (sample_cube);
    auto &sample_cube_model
        = reg.emplace<comp::model_instance_3d> (sample_cube);
    sample_cube_model.id = builtin_cube_id;
    sample_cube_model.scene_index = 0;

    // Add a default camera entity
    auto cam_entity = reg.create ();
    temp_scene.set_entity_name (cam_entity, "Scene Default Camera");
    reg.emplace<comp::hierarchy> (cam_entity);
    auto &cam_transform = reg.emplace<comp::transform> (
        cam_entity, glm::vec3 (0.0F, 0.0F, 5.0F));
    auto &cam_world_transform = reg.emplace<comp::world_transform> (cam_entity);
    cam_world_transform.value = cam_transform.model ();
    reg.emplace<comp::camera> (cam_entity);
    temp_scene.camera = cam_entity;

    io::scene_snapshot_serializer const serializer (m_runtime_ctx, temp_scene);
    serializer.save_json (scene_file.string ());
  }
  project project_copy = proj;
  project_copy.default_scene_path = default_scene_rel;
  cereal::JSONOutputArchive archive (file);
  archive (cereal::make_nvp ("project", project_copy));

  // Generate src/main.cpp
  const fs::path main_file = fs::path (proj.root_path) / "src/main.cpp";
  std::ofstream main_out (main_file);
  if (main_out) {
    std::string sanitized_name = proj.name;
    std::replace (sanitized_name.begin (), sanitized_name.end (), '-', '_');

    main_out << "#include <wsl/app.hpp>\n"
             << "#include <wsl/rsc/project_loader.hpp>\n\n"
             << "class " << sanitized_name << "_app : public wsl::app {\n"
             << "public:\n"
             << "    " << sanitized_name << "_app() : wsl::app(\"" << proj.name
             << "\", 1280, 720, \n"
             << "#ifdef WSL_RESOURCE_PATH\n"
             << "        WSL_RESOURCE_PATH\n"
             << "#else\n"
             << "        \".\"\n"
             << "#endif\n"
             << "    ) {}\n\n"
             << "protected:\n"
             << "    void on_init() override {\n"
             << "        set_project_path(\"wslpro.json\");\n"
             << "    }\n"
             << "};\n\n"
             << "int main(int argc, char** argv) {\n"
             << "    " << sanitized_name << "_app app;\n"
             << "    return app.run();\n"
             << "}\n";
  }

  // Generate CMakeLists.txt
  const fs::path cmake_file = fs::path (proj.root_path) / "CMakeLists.txt";
  std::ofstream cmake_out (cmake_file);
  if (cmake_out) {
    cmake_out
        << "cmake_minimum_required(VERSION 3.22)\n"
        << "project(" << proj.name << " LANGUAGES C CXX)\n\n"
        << "set(CMAKE_CXX_STANDARD 20)\n"
        << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        << "# Weasel Engine dependency\n"
        << "# CMake looks for WeaselConfig.cmake in standard install "
           "prefixes.\n"
        << "# If the engine is installed elsewhere, set Weasel_DIR to the "
           "directory\n"
        << "# containing WeaselConfig.cmake (e.g. "
           "/path/to/prefix/lib/cmake/Weasel).\n"
        << "find_package(Weasel REQUIRED)\n\n"
        << "file(GLOB_RECURSE SOURCES\n"
        << "    \"src/*.cpp\"\n"
        << "    \"" << proj.components_path << "/*.cpp\"\n"
        << "    \"" << proj.systems_path << "/*.cpp\"\n"
        << "    \"" << proj.singletons_path << "/*.cpp\"\n"
        << ")\n\n"
        << "add_executable(${PROJECT_NAME} ${SOURCES})\n"
        << "target_link_libraries(${PROJECT_NAME} PRIVATE wsl)\n\n"
        << "if(NOT DEFINED Weasel_RESOURCE_PATH OR Weasel_RESOURCE_PATH "
           "STREQUAL \"\")\n"
        << "  set(Weasel_RESOURCE_PATH \"${Weasel_DIR}\")\n"
        << "endif()\n"
        << "target_compile_definitions(${PROJECT_NAME} PRIVATE "
           "WSL_RESOURCE_PATH=\"${Weasel_RESOURCE_PATH}\")\n\n"
        << "# --- Installation and Packaging ---\n"
        << "install(TARGETS ${PROJECT_NAME}\n"
        << "    RUNTIME DESTINATION bin\n"
        << ")\n\n"
        << "# Install project resources\n"
        << "install(DIRECTORY src DESTINATION share/${PROJECT_NAME})\n"
        << "install(DIRECTORY audio DESTINATION share/${PROJECT_NAME} "
           "OPTIONAL)\n"
        << "install(DIRECTORY textures DESTINATION share/${PROJECT_NAME} "
           "OPTIONAL)\n"
        << "install(DIRECTORY rml DESTINATION share/${PROJECT_NAME} OPTIONAL)\n"
        << "install(DIRECTORY otf DESTINATION share/${PROJECT_NAME} OPTIONAL)\n"
        << "install(DIRECTORY scenes DESTINATION share/${PROJECT_NAME} "
           "OPTIONAL)\n"
        << "install(DIRECTORY shaders DESTINATION share/${PROJECT_NAME} "
           "OPTIONAL)\n\n"
        << "set(CPACK_PACKAGE_NAME \"${PROJECT_NAME}\")\n"
        << "set(CPACK_PACKAGE_VERSION \"0.1.0\")\n"
        << "set(CPACK_GENERATOR \"TGZ;DEB;RPM\")\n\n"
        << "include(CPack)\n";
  }

  wsl::log::rsc ()->debug ("Created project manifest at {}",
                           project_file.string ());
  return true;
}

std::shared_ptr<rsc::project>
rsc::project_loader::load (const std::string &path)
{

  std::shared_ptr<project> proj = std::make_shared<project> ();

  std::ifstream file (path, std::ios::binary);
  if (!file) {
    wsl::log::rsc ()->error ("Failed to open project file: {}", path);
    return {};
  }

  if (path.ends_with (".json")) {
    try {
      cereal::JSONInputArchive archive (file);
      archive (cereal::make_nvp ("project", *proj));
    } catch (const std::exception &e) {
      wsl::log::rsc ()->error ("Failed to parse project file '{}': {}", path,
                               e.what ());
      return {};
    }
  } else {
    try {
      cereal::BinaryInputArchive archive (file);
      archive (cereal::make_nvp ("project", *proj));
    } catch (const std::exception &e) {
      wsl::log::rsc ()->error ("Failed to parse project file '{}': {}", path,
                               e.what ());
      return {};
    }
  }

  // Ensure root_path is absolute and points to the manifest's directory
  auto manifest_dir = fs::path (path).parent_path ();
  if (manifest_dir.empty ())
    manifest_dir = fs::current_path ();
  proj->root_path = fs::absolute (manifest_dir).string ();

  wsl::log::rsc ()->debug ("Loaded project: {}", proj->name);
  return proj;
}
rsc::project_assets
rsc::project_loader::scan_assets (const project &proj)
{

  project_assets assets;

  const auto resolve = [&] (const std::string &rel) {
    return fs::path (proj.root_path) / rel;
  };

  const auto scan_dir
      = [] (const fs::path &dir, const std::vector<std::string> &exts,
            std::vector<std::string> &out) {
          if (!fs::exists (dir)) {
            return;
          }

          for (const fs::directory_entry &entry :
               fs::recursive_directory_iterator (dir)) {
            if (!entry.is_regular_file ()) {
              continue;
            }

            std::string const ext = entry.path ().extension ().string ();

            for (const std::string &allowed : exts) {
              if (ext == allowed) {
                out.push_back (entry.path ().string ());
                break;
              }
            }
          }
        };

  scan_dir (resolve (proj.models_path), { ".gltf", ".glb" }, assets.models);
  scan_dir (resolve (proj.images_path), { ".png", ".jpg", ".jpeg" },
            assets.images);
  scan_dir (resolve (proj.cubemaps_path), { ".tar", ".hdr", ".png" },
            assets.cubemaps);
  scan_dir (resolve (proj.scenes_path),
            { ".wscn.json", ".scene", ".json", ".prefab" }, assets.scenes);
  scan_dir (resolve (proj.audio_path), { ".wav", ".mp3", ".ogg" },
            assets.audio);
  scan_dir (resolve (proj.ui_layouts_path), { ".rml", ".rcss" },
            assets.ui_layouts);
  scan_dir (resolve (proj.fonts_path), { ".otf", ".ttf" }, assets.fonts);
  scan_dir (resolve (proj.shaders_path), { ".hlsl", ".spv", ".dxil", ".metal" },
            assets.shaders);

  std::sort (assets.models.begin (), assets.models.end ());
  std::sort (assets.images.begin (), assets.images.end ());
  std::sort (assets.cubemaps.begin (), assets.cubemaps.end ());
  std::sort (assets.scenes.begin (), assets.scenes.end ());
  std::sort (assets.audio.begin (), assets.audio.end ());
  std::sort (assets.ui_layouts.begin (), assets.ui_layouts.end ());
  std::sort (assets.fonts.begin (), assets.fonts.end ());
  std::sort (assets.shaders.begin (), assets.shaders.end ());

  wsl::log::rsc ()->debug ("Project assets scanned.");
  return assets;
}

} // namespace wsl
