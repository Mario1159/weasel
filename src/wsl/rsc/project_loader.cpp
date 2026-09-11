#include "project_loader.hpp"

#include <algorithm>
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
#include "wsl/comp/directional_light.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/model_instance_3d.hpp"

#include "../serialize/serialize.hpp"

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
  create_dir (proj.materials_path);
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
        = m_runtime_ctx->resource_manager ().register_model ("builtin://cube");
    const rsc::model_id builtin_sphere_id
        = m_runtime_ctx->resource_manager ().register_model (
            "builtin://sphere");
    const rsc::cubemap_id builtin_skybox_id
        = m_runtime_ctx->resource_manager ().register_cubemap (
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
    cam_world_transform.value () = cam_transform.model ();
    reg.emplace<comp::camera> (cam_entity);
    temp_scene.camera = cam_entity;

    // Add a default directional light entity
    auto sun_entity = reg.create ();
    temp_scene.set_entity_name (sun_entity, "Sunlight");
    reg.emplace<comp::hierarchy> (sun_entity);
    auto &sun_transform = reg.emplace<comp::transform> (
        sun_entity, glm::vec3 (0.0F, 5.0F, 0.0F));
    sun_transform.set_rotation_xyz (wsl::math::vec3f{ 145.0F, -45.0F, 180.0F });
    reg.emplace<comp::world_transform> (sun_entity).value ()
        = sun_transform.model ();
    reg.emplace<comp::directional_light> (sun_entity);

    io::scene_snapshot_serializer const serializer (m_runtime_ctx, temp_scene);
    serializer.save_json (scene_file.string ());
  }
  project project_copy = proj;
  project_copy.default_scene_path = default_scene_rel;
  std::string json_str = serialize::json_write (project_copy);
  file << json_str;

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

  // Generate xmake.lua
  const fs::path xmake_file = fs::path (proj.root_path) / "xmake.lua";
  std::ofstream xmake_out (xmake_file);
  if (xmake_out) {
    xmake_out
        << "set_project(\"" << proj.name << "\")\n"
        << "set_version(\"0.1.0\")\n"
        << "set_xmakever(\"3.0.0\")\n"
        << "set_languages(\"c++20\")\n"
        << "add_rules(\"mode.debug\", \"mode.release\")\n\n"
        << "-- Weasel Engine dependency\n"
        << "-- Configure with: xmake f --weasel_dir=/path/to/weasel (or set "
           "$WEASEL_DIR)\n"
        << "option(\"weasel_dir\", {showmenu = true, default = "
           "os.getenv(\"WEASEL_DIR\") or \"\", description = \"Path to the "
           "Weasel Engine source tree\"})\n\n"
        << "target(\"" << proj.name << "\")\n"
        << "    set_kind(\"binary\")\n"
        << "    add_files(\"src/*.cpp\", \"" << proj.components_path
        << "/*.cpp\", \"" << proj.systems_path << "/*.cpp\", \""
        << proj.singletons_path << "/*.cpp\")\n"
        << "    -- TODO: AOT-compile .das files (weasel_aot_das equivalent)\n"
        << "    add_includedirs(\"$(weasel_dir)/src\", "
           "\"$(weasel_dir)/src/wsl\")\n"
        << "    add_linkdirs(\"$(weasel_dir)/build/$(plat)/$(arch)/$(mode)\")\n"
        << "    add_links(\"wsl\")\n"
        << "    add_defines(\"WSL_RESOURCE_PATH=\\\"$(weasel_dir)\\\"\")\n\n"
        << "-- --- Installation and Packaging ---\n"
        << "add_installfiles(\"src\", {prefixdir = \"share/" << proj.name
        << "\"})\n";
  }

  // Generate AGENTS.md so AI agents working in the project get engine
  // context by default.
  const fs::path agents_file = fs::path (proj.root_path) / "AGENTS.md";
  std::ofstream agents_out (agents_file);
  if (agents_out) {
    agents_out
        << "# AGENTS.md\n\n"
        << "Guidance for AI agents working in this repository.\n\n"
        << "## What this is\n\n"
        << "This is a game project for the Weasel Engine, an ECS 2D & 3D "
           "game engine\n"
        << "for C++ and Daslang. Scenes follow the Entity Component System "
           "model:\n"
        << "entities hold components (Transform, RigidBody, Camera,\n"
        << "ModelInstance3D, ...), and behavior lives in systems.\n\n"
        << "## Project layout\n\n"
        << "- `wslpro.json` - project manifest (paths, default scene)\n"
        << "- `" << proj.scenes_path
        << "/` - scenes stored as `.wscn.json` files\n"
        << "- `" << proj.components_path << "/`, `" << proj.systems_path
        << "/`, `" << proj.singletons_path << "/` - runtime code\n"
        << "- `src/main.cpp` - standalone game entry point\n"
        << "- Asset folders: models, images, audio, fonts, shaders, "
           "materials\n\n"
        << "## Making engine calls\n\n"
        << "Prefer attaching to a running Weasel editor instance:\n\n"
        << "    weasel-cli -a ent new Player\n"
        << "    weasel-cli -a comp set 0 transform position '[0, 2, 0]'\n\n"
        << "`-a` attaches to the first running editor and uses its open\n"
        << "project. Without an editor it warns and falls back to standalone\n"
        << "execution (add `--project <path>` to select one). Mutations\n"
        << "auto-save when running standalone; when attached, saving is done\n"
        << "in the editor.\n\n"
        << "For interactive exploration use the REPL: `weasel-cli -i -a`\n\n"
        << "The same functionality is also exposed as MCP tools through\n"
        << "`weasel-mcp-server`. When connected to it, prefer those tools\n"
        << "(list_commands, describe_command, list_components,\n"
        << "describe_component, describe_namespace) over shelling out.\n\n"
        << "## Notes\n\n"
        << "- Core systems (Transform, Physics, 3D Render, ...) exist in\n"
        << "  every scene automatically; do not create them manually.\n"
        << "- Validate structural changes with:\n"
        << "  `weasel-cli validate-project wslpro.json`\n";
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

  std::string file_content ((std::istreambuf_iterator<char> (file)),
                            std::istreambuf_iterator<char> ());

  if (path.ends_with (".json")) {
    try {
      if (!serialize::json_read (file_content, *proj)) {
        return {};
      }
    } catch (const std::exception &e) {
      wsl::log::rsc ()->error ("Failed to parse project file '{}': {}", path,
                               e.what ());
      return {};
    }
  } else {
    try {
      std::vector<std::uint8_t> bytes (file_content.begin (),
                                       file_content.end ());
      if (!serialize::msgpack_read (bytes, *proj)) {
        return {};
      }
    } catch (const std::exception &e) {
      wsl::log::rsc ()->error ("Failed to parse project file '{}': {}", path,
                               e.what ());
      return {};
    }
  }

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
  scan_dir (resolve (proj.materials_path), { ".wslmat", ".wslgraph" },
            assets.materials);

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
