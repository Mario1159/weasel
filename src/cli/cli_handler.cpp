#include "cli_handler.hpp"

#include <SDL3/SDL_filesystem.h>
#include <filesystem>

#include "comp/area3d.hpp"
#include "comp/camera.hpp"
#include "comp/character_body.hpp"
#include "comp/component_meta.hpp"
#include "comp/directional_light.hpp"
#include "comp/hierarchy.hpp"
#include "comp/model_instance_3d.hpp"
#include "comp/point_light.hpp"
#include "comp/rigid_body.hpp"
#include "comp/singl/physics_manager.hpp"
#include "comp/singl/rendering_manager.hpp"
#include "comp/spot_light.hpp"
#include "comp/transform.hpp"
#include "comp/world_transform.hpp"
#include "math/vector.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/rsc/project_loader.hpp"
#include "wsl/rsc/project.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/scene_snapshot_serializer.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/ui_manager.hpp"
#include "wsl/sys/system.hpp"
#include "wsl/comp/components.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace wsl::cli {

namespace {
class dummy_system : public wsl::sys::ecs_system_t<dummy_system> {
public:
    using ecs_system_t::ecs_system_t;
};

void register_all(wsl::comp::singl::runtime_context& rtc) {
    wsl::comp::singl::runtime_context::register_meta();
    wsl::comp::singl::editor_context::register_meta();
    wsl::comp::singl::ui_manager::register_meta();

    wsl::comp::register_component_meta<
      wsl::comp::hierarchy, wsl::comp::world_transform, wsl::comp::transform, wsl::math::vec3f, wsl::math::quatf,
      wsl::rsc::model_id, wsl::comp::model_instance_3d, wsl::comp::camera, wsl::comp::point_light,
      wsl::comp::spot_light, wsl::comp::directional_light,
      wsl::comp::rigid_body, wsl::comp::area, wsl::comp::character_body,
      wsl::rsc::scene_manager, wsl::rsc::resource_manager_view> ();

    wsl::comp::for_each_type<wsl::comp::component_types>::apply ([&rtc]<typename T> () {
      rtc.component_registry.register_world_component<T> ();
    });

    rtc.singleton_registry
        .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
            { "Runtime Context", true });
    rtc.singleton_registry
        .register_bound_singleton_component<wsl::comp::singl::editor_context> (
            { "Editor Context", true });
    rtc.singleton_registry.register_bound_singleton_component<wsl::rsc::scene_manager> (
        { "Scene Manager", true });
    rtc.singleton_registry
        .register_bound_singleton_component<wsl::rsc::resource_manager_view> (
            { "Resource Manager", true });
    rtc.singleton_registry
        .register_bound_singleton_component<wsl::comp::singl::ui_manager> (
            { "UI Manager", true, false, true });
    rtc.singleton_registry
        .register_singleton_component<wsl::comp::singl::rendering_manager> (
            { "Rendering Manager", true });
    rtc.singleton_registry
        .register_singleton_component<wsl::comp::singl::physics_manager> (
            { "Physics Manager", true });
}
}

cli_handler::result cli_handler::parse(int argc, char** argv) {
  CLI::App app{"Weasel Engine Editor"};
  app.allow_extras();

  std::string project_to_load;
  app.add_option("--project", project_to_load, "Path to the project to load");

  bool interactive = false;
  app.add_flag("-i,--interactive", interactive, "Start the interactive REPL");

  bool attach = false;
  app.add_flag("-a,--attach", attach, "Attach to running editor server for the project");

  auto* create_project = app.add_subcommand("create-project", "Create a new project");
  create_project->alias("--create-project");
  std::string cp_path, cp_name;
  create_project->add_option("path", cp_path, "Project path")->required();
  create_project->add_option("name", cp_name, "Project name")->required();

  auto* create_scene = app.add_subcommand("create-scene", "Create a new scene");
  create_scene->alias("--create-scene");
  std::string cs_path, cs_name;
  std::vector<std::string> cs_systems;
  create_scene->add_option("path", cs_path, "Scene path")->required();
  create_scene->add_option("name", cs_name, "Scene name")->required();
  create_scene->add_option("systems", cs_systems, "Systems to add");

  auto* validate_project = app.add_subcommand("validate-project", "Validate a project");
  validate_project->alias("--validate-project");
  std::string vp_path;
  validate_project->add_option("path", vp_path, "Path to wslpro.json")->required();

  auto* validate_scene = app.add_subcommand("validate-scene", "Validate a scene");
  validate_scene->alias("--validate-scene");
  std::string vs_scene_path, vs_proj_path;
  validate_scene->add_option("scene_path", vs_scene_path, "Path to scene.wscn.json")->required();
  validate_scene->add_option("proj_path", vs_proj_path, "Path to wslpro.json")->required();

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return {true, app.exit(e), std::nullopt};
  }

  if (*create_project) {
    wsl::rsc::project proj;
    proj.name = cp_name;
    proj.author = "Unknown";
    proj.root_path = std::filesystem::absolute (cp_path).string ();
    proj.systems_path = "src/systems";
    proj.components_path = "src/components";
    proj.singletons_path = "src/singletons";
    proj.ui_layouts_path = "src/ui";
    proj.scenes_path = "rsc/scenes";
    proj.models_path = "rsc/models";
    proj.images_path = "rsc/textures";
    proj.cubemaps_path = "rsc/textures/cubemaps";
    proj.audio_path = "rsc/audio";
    proj.fonts_path = "rsc/fonts";
    proj.default_scene_path = "";

    wsl::rsc::project_loader const loader;
    if (loader.create (proj)) {
      std::cout << "Project created successfully at " << proj.root_path << '\n';
      return {true, 0, std::nullopt};
    }
    std::cerr << "Failed to create project" << std::endl;
    return {true, 1, std::nullopt};
  }

  if (*create_scene) {
    if (cs_path.find (".wscn.json") == std::string::npos) {
      if (cs_path.ends_with (".json")) {
        cs_path.replace (cs_path.find (".json"), 5, ".wscn.json");
      } else {
        cs_path += ".wscn.json";
      }
    }

    wsl::comp::singl::runtime_context rtc{
      "Scene Generator", 1, 1, default_engine_resource_path ()
    };
    rtc.set_editor_ctx(nullptr);
    register_all(rtc);

    wsl::rsc::scene scene{&rtc, nullptr, cs_name};
    
    for (const auto& sys_name : cs_systems) {
        rtc.system_factory_registry.register_system(sys_name.c_str(), [sys_name](wsl::rsc::scene&){ 
            return std::make_unique<dummy_system>(sys_name); 
        });
        if (auto sys = rtc.system_factory_registry.create(sys_name, scene)) {
            scene.add_system_instance(std::move (sys), false);
        }
    }

    wsl::rsc::io::scene_snapshot_serializer const serializer{&rtc, scene};
    if (serializer.save_json (cs_path)) {
      std::cout << "Scene created successfully at " << cs_path << '\n';
      return {true, 0, std::nullopt};
    }
    std::cerr << "Failed to create scene" << std::endl;
    return {true, 1, std::nullopt};
  }

  if (*validate_project) {
    try {
      wsl::rsc::project_loader const loader;
      auto proj = loader.load (vp_path);
      if (proj) {
        std::cout << "Project is valid: " << proj->name << '\n';
        return {true, 0, std::nullopt};
      }
      std::cerr << "Failed to load project (returned null)" << std::endl;
      return {true, 1, std::nullopt};
    } catch (const std::exception &e) {
      std::cerr << "Project validation failed: " << e.what () << '\n';
      return {true, 1, std::nullopt};
    }
  }

  if (*validate_scene) {
    try {
      wsl::rsc::project_loader const proj_loader;
      auto proj = proj_loader.load (vs_proj_path);
      if (!proj) {
        std::cerr << "Failed to load project context for scene validation" << '\n';
        return {true, 1, std::nullopt};
      }

      wsl::comp::singl::runtime_context rtc{
        "Validator", 1, 1, default_engine_resource_path ()
      };
      register_all(rtc);

      const std::filesystem::path project_root = proj->root_path;
      const bool has_runtime_code
          = std::filesystem::exists (project_root / proj->systems_path)
            || std::filesystem::exists (project_root / proj->components_path)
            || std::filesystem::exists (project_root
                                        / proj->singletons_path);
      if (has_runtime_code
          && !rtc.runtime_project_module.compile_and_load (*proj)) {
        std::cerr << "Runtime module validation failed: "
                  << rtc.runtime_project_module.last_status () << '\n';
        return {true, 1, std::nullopt};
      }

      wsl::rsc::scene scene{&rtc, nullptr, "ValidationScene"};
      wsl::rsc::io::scene_snapshot_serializer serializer{&rtc, scene};
      if (serializer.load_json (vs_scene_path)) {
        std::cout << "Scene is valid: " << vs_scene_path << '\n';
        return {true, 0, std::nullopt};
      }
      std::cerr << "Failed to load scene" << std::endl;
      return {true, 1, std::nullopt};
    } catch (const std::exception &e) {
      std::cerr << "Scene validation failed: " << e.what () << '\n';
      return {true, 1, std::nullopt};
    }
  }

  result res;
  if (!project_to_load.empty()) {
    res.project_to_load = project_to_load;
  }
  res.interactive = interactive;
  res.attach = attach;

  auto extras = app.remaining();
  if (!extras.empty()) {
    std::string full_cmd;
    for (size_t i = 0; i < extras.size(); ++i) {
      full_cmd += extras[i];
      if (i < extras.size() - 1) full_cmd += " ";
    }
    res.command = full_cmd;
  }

  return res;
}

std::string cli_handler::default_engine_resource_path ()
{
  const char *base_path = SDL_GetBasePath ();
  if (base_path != nullptr)
    {
      std::filesystem::path exe_dir (base_path);

      std::filesystem::path share_dir = exe_dir / ".." / "share" / "weasel";
      if (std::filesystem::exists (share_dir / "compiled_shaders"))
        return share_dir.string ();
    }

#ifdef WEASEL_BUILD_DIR
  return WEASEL_BUILD_DIR;
#elif defined(WEASEL_SOURCE_DIR)
  return WEASEL_SOURCE_DIR;
#else
  return ".";
#endif
}

} // namespace wsl::cli
