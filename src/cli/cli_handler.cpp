#include "cli_handler.hpp"
#include "wsl/log/log.hpp"

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
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

namespace wsl::cli
{

namespace
{
class dummy_system : public wsl::sys::ecs_system_t<dummy_system>
{
public:
  using ecs_system_t::ecs_system_t;
};

void
register_all (wsl::comp::singl::runtime_context &rtc)
{
  wsl::comp::singl::runtime_context::register_meta ();
  wsl::comp::singl::editor_context::register_meta ();
  wsl::comp::singl::ui_manager::register_meta ();

  wsl::comp::register_component_meta<
      wsl::comp::hierarchy, wsl::comp::world_transform, wsl::comp::transform,
      wsl::math::vec3f, wsl::math::quatf, wsl::math::mat33f, wsl::math::mat44f,
      wsl::rsc::model_id, wsl::comp::model_instance_3d, wsl::comp::camera,
      wsl::comp::point_light, wsl::comp::spot_light,
      wsl::comp::directional_light, wsl::comp::rigid_body, wsl::comp::area,
      wsl::comp::character_body, wsl::rsc::scene_manager,
      wsl::rsc::resource_manager_view> ();

  wsl::comp::for_each_type<wsl::comp::component_types>::apply (
      [&rtc]<typename T> () {
        rtc.component_registry.register_world_component<T> ();
      });

  rtc.singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
          { "Runtime Context", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::editor_context> (
          { "Editor Context", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::rsc::scene_manager> (
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

std::string
quote_repl_arg (std::string_view arg)
{
  const bool needs_quotes
      = arg.empty ()
        || std::any_of (arg.begin (), arg.end (), [] (unsigned char ch) {
             return std::isspace (ch) || ch == '"' || ch == '\'';
           });
  if (!needs_quotes) {
    return std::string (arg);
  }

  std::string out;
  out.reserve (arg.size () + 2);
  out.push_back ('"');
  for (char ch : arg) {
    if (ch == '"' || ch == '\\') {
      out.push_back ('\\');
    }
    out.push_back (ch);
  }
  out.push_back ('"');
  return out;
}

std::string
build_repl_command (const std::vector<std::string> &args)
{
  std::ostringstream output;
  for (std::size_t i = 0; i < args.size (); ++i) {
    if (i > 0) {
      output << ' ';
    }
    output << quote_repl_arg (args[i]);
  }
  return output.str ();
}
}

cli_handler::result
cli_handler::parse (int argc, char **argv)
{
  CLI::App app{ "Weasel Engine Editor" };
  app.allow_extras ();
  app.get_formatter ()->column_width (42);

  std::string project_to_load;
  auto *project_opt = app.add_option ("--project", project_to_load,
                                      "Path to the project to load");

  std::string scene_to_load;
  auto *scene_opt
      = app.add_option ("--scene", scene_to_load,
                        "Path to the scene to load before running a command");
  scene_opt->needs (project_opt);

  bool interactive = false;
  app.add_flag ("-i,--interactive", interactive, "Start the interactive REPL");

  bool attach = false;
  auto *attach_flag = app.add_flag (
      "-a,--attach", attach, "Attach to running editor server for the project");
  attach_flag->needs (project_opt);

  auto *create_project
      = app.add_subcommand ("create-project", "Create a new project");
  create_project->alias ("--create-project");
  std::string cp_path, cp_name;
  bool no_subdir = false;
  create_project->add_option ("path", cp_path, "Project parent path");
  create_project->add_option ("name", cp_name, "Project name");
  create_project->add_flag ("--no-subdir", no_subdir,
                            "Don't create a project-name subdirectory");

  auto *create_scene
      = app.add_subcommand ("create-scene", "Create a new scene");
  create_scene->alias ("--create-scene");
  std::string cs_path, cs_name;
  std::vector<std::string> cs_systems;
  create_scene->add_option ("path", cs_path, "Scene path")->required ();
  create_scene->add_option ("name", cs_name, "Scene name")->required ();
  create_scene->add_option ("systems", cs_systems, "Systems to add");

  auto *validate_project
      = app.add_subcommand ("validate-project", "Validate a project");
  validate_project->alias ("--validate-project");
  std::string vp_path;
  validate_project->add_option ("path", vp_path, "Path to wslpro.json")
      ->required ();

  auto *validate_scene
      = app.add_subcommand ("validate-scene", "Validate a scene");
  validate_scene->alias ("--validate-scene");
  std::string vs_scene_path, vs_proj_path;
  validate_scene
      ->add_option ("scene_path", vs_scene_path, "Path to scene.wscn.json")
      ->required ();
  validate_scene->add_option ("proj_path", vs_proj_path, "Path to wslpro.json")
      ->required ();

  auto *proj_cmd = app.add_subcommand ("proj", "Manage project configuration");
  auto *proj_new = proj_cmd->add_subcommand ("new", "Create a project");
  std::string proj_new_path, proj_new_name;
  proj_new->add_option ("path", proj_new_path, "Project path")->required ();
  proj_new->add_option ("name", proj_new_name, "Project name")->required ();

  auto *proj_load = proj_cmd->add_subcommand (
      "load", "Load a project into a non-interactive runtime");
  std::string proj_load_path;
  proj_load
      ->add_option ("path", proj_load_path,
                    "Path to wslpro.json or project root")
      ->required ();

  auto *proj_info = proj_cmd->add_subcommand (
      "info", "Show information about the loaded project");
  auto *proj_save
      = proj_cmd->add_subcommand ("save", "Save the loaded project");

  auto *scene_cmd
      = app.add_subcommand ("scene", "Manage scenes in the loaded project");
  auto *scene_new
      = scene_cmd->add_subcommand ("new", "Create a new empty active scene");
  std::string scene_new_name;
  scene_new->add_option ("name", scene_new_name, "Scene name")->required ();

  auto *scene_load = scene_cmd->add_subcommand (
      "load", "Load a scene into the active runtime");
  std::string scene_load_path;
  scene_load->add_option ("path", scene_load_path, "Path to the scene file")
      ->required ();

  auto *scene_save
      = scene_cmd->add_subcommand ("save", "Save the active scene");
  std::string scene_save_path;
  scene_save->add_option ("path", scene_save_path,
                          "Optional override save path");

  auto *scene_ls
      = scene_cmd->add_subcommand ("ls", "List scenes in the loaded project");
  auto *scene_status
      = scene_cmd->add_subcommand ("status", "Show active scene status");

  auto *ent_cmd
      = app.add_subcommand ("ent", "Manage entities in the active scene");
  auto *ent_new = ent_cmd->add_subcommand (
      "new", "Create a new entity with default components (Transform, "
             "WorldTransform, Hierarchy)");
  std::string ent_new_name;
  bool ent_new_empty = false;
  ent_new->add_option ("name", ent_new_name, "Optional entity name");
  ent_new->add_flag ("--empty", ent_new_empty,
                     "Create an empty entity without default components");

  auto *ent_ls
      = ent_cmd->add_subcommand ("ls", "List entities in the active scene");

  auto *ent_rm = ent_cmd->add_subcommand (
      "rm", "Remove an entity from the active scene");
  std::string ent_rm_id;
  ent_rm->add_option ("id", ent_rm_id, "Entity id")->required ();

  auto *ent_ren
      = ent_cmd->add_subcommand ("ren", "Rename an entity in the active scene");
  std::string ent_ren_id, ent_ren_name;
  ent_ren->add_option ("id", ent_ren_id, "Entity id")->required ();
  ent_ren->add_option ("new_name", ent_ren_name, "New entity name")
      ->required ();

  auto *ent_inspect = ent_cmd->add_subcommand (
      "inspect", "Inspect an entity in the active scene");
  std::string ent_inspect_id;
  ent_inspect->add_option ("id", ent_inspect_id, "Entity id")->required ();

  auto *comp_cmd = app.add_subcommand ("comp", "Manage components on entities");
  auto *comp_ls = comp_cmd->add_subcommand (
      "ls", "List registered components or an entity's components");
  std::string comp_ls_entity_id;
  comp_ls->add_option ("entity_id", comp_ls_entity_id, "Optional entity id");

  auto *comp_avail
      = comp_cmd->add_subcommand ("avail", "List registered components");

  auto *comp_add
      = comp_cmd->add_subcommand ("add", "Add a component to an entity");
  std::string comp_add_entity_id, comp_add_type;
  comp_add->add_option ("entity_id", comp_add_entity_id, "Entity id")
      ->required ();
  comp_add->add_option ("type", comp_add_type, "Component type name")
      ->required ();

  auto *comp_rm
      = comp_cmd->add_subcommand ("rm", "Remove a component from an entity");
  std::string comp_rm_entity_id, comp_rm_type;
  comp_rm->add_option ("entity_id", comp_rm_entity_id, "Entity id")
      ->required ();
  comp_rm->add_option ("type", comp_rm_type, "Component type name")
      ->required ();

  auto *comp_set = comp_cmd->add_subcommand ("set", "Set a component property");
  std::string comp_set_entity_id, comp_set_type, comp_set_prop, comp_set_val;
  comp_set->add_option ("entity_id", comp_set_entity_id, "Entity id")
      ->required ();
  comp_set->add_option ("type", comp_set_type, "Component type name")
      ->required ();
  comp_set->add_option ("property", comp_set_prop, "Property path")
      ->required ();
  comp_set->add_option ("value", comp_set_val, "Value (JSON or plain)")
      ->required ();

  auto *singl_cmd = app.add_subcommand ("singl", "Manage singleton components");
  auto *singl_ls = singl_cmd->add_subcommand (
      "ls", "List singleton components in the scene");
  auto *singl_add = singl_cmd->add_subcommand (
      "add", "Add a singleton to the active scene");
  std::string singl_add_name;
  singl_add->add_option ("name", singl_add_name, "Singleton name")->required ();
  auto *singl_create = singl_cmd->add_subcommand (
      "create", "Generate a singleton component template");
  std::string singl_create_name;
  bool singl_create_source = false;
  singl_create->add_option ("name", singl_create_name, "Singleton name")
      ->required ();
  singl_create->add_flag ("--source", singl_create_source,
                          "Also generate a .cpp source file");
  auto *singl_set
      = singl_cmd->add_subcommand ("set", "Set a singleton component property");
  std::string singl_set_name, singl_set_prop, singl_set_val;
  singl_set->add_option ("name", singl_set_name, "Singleton name")->required ();
  singl_set->add_option ("property", singl_set_prop, "Property path")
      ->required ();
  singl_set->add_option ("value", singl_set_val, "Value (JSON or plain)")
      ->required ();

  auto *sys_cmd = app.add_subcommand (
      "sys", "Manage systems attached to the active scene");
  auto *sys_ls

      = sys_cmd->add_subcommand (
          "ls", "List all systems in the active scene (core + user-defined)");
  auto *sys_avail

      = sys_cmd->add_subcommand ("avail",
                                 "List registered system types (core + user)");
  auto *sys_add

      = sys_cmd->add_subcommand (
          "add", "Add a user-defined system to the active scene");
  std::string sys_add_name;
  sys_add->add_option ("name", sys_add_name, "System name")->required ();

  try {
    app.parse (argc, argv);
  } catch (const CLI::ParseError &e) {
    return { true, app.exit (e), std::nullopt };
  }

  if (*create_project) {
    if (cp_path.empty () && cp_name.empty ()) {
      wsl::log::cli ()->error ("Project name is required");
      return { true, 1, std::nullopt };
    }
    if (!cp_path.empty () && cp_name.empty ()) {
      cp_name = cp_path;
      cp_path.clear ();
    }

    std::filesystem::path project_root;
    if (cp_path.empty ()) {
      if (no_subdir) {
        project_root = std::filesystem::current_path ();
      } else {
        project_root = std::filesystem::current_path () / cp_name;
      }
    } else {
      if (no_subdir) {
        project_root = std::filesystem::path (cp_path);
      } else {
        project_root = std::filesystem::path (cp_path) / cp_name;
      }
    }

    wsl::rsc::project proj;
    proj.name = cp_name;
    proj.author = "Unknown";
    proj.root_path = std::filesystem::absolute (project_root).string ();
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

    wsl::comp::singl::runtime_context rtc{ "Project Generator", 0, 0,
                                           default_engine_resource_path (),
                                           true };
    rtc.set_editor_ctx (nullptr);
    register_all (rtc);

    wsl::rsc::project_loader const loader (&rtc);
    if (loader.create (proj)) {
      wsl::log::cli ()->info ("Project created successfully at {}",
                              proj.root_path);
      return { true, 0, std::nullopt };
    }
    wsl::log::cli ()->error ("Failed to create project");
    return { true, 1, std::nullopt };
  }

  if (*create_scene) {
    if (cs_path.find (".wscn.json") == std::string::npos) {
      if (cs_path.ends_with (".json")) {
        cs_path.replace (cs_path.find (".json"), 5, ".wscn.json");
      } else {
        cs_path += ".wscn.json";
      }
    }

    wsl::comp::singl::runtime_context rtc{ "Scene Generator", 0, 0,
                                           default_engine_resource_path (),
                                           true };
    rtc.set_editor_ctx (nullptr);
    register_all (rtc);

    wsl::rsc::scene scene{ &rtc, nullptr, cs_name };

    for (const auto &sys_name : cs_systems) {
      rtc.system_factory_registry.register_system (
          sys_name.c_str (), [sys_name] (wsl::rsc::scene &) {
            return std::make_unique<dummy_system> (sys_name);
          });
      if (auto sys = rtc.system_factory_registry.create (sys_name, scene)) {
        scene.add_system_instance (std::move (sys), false);
      }
    }

    wsl::rsc::io::scene_snapshot_serializer const serializer{ &rtc, scene };
    if (serializer.save_json (cs_path)) {
      wsl::log::cli ()->info ("Scene created successfully at {}", cs_path);
      return { true, 0, std::nullopt };
    }
    wsl::log::cli ()->error ("Failed to create scene");
    return { true, 1, std::nullopt };
  }

  if (*validate_project) {
    try {
      wsl::rsc::project_loader const loader;
      auto proj = loader.load (vp_path);
      if (proj) {
        wsl::log::cli ()->info ("Project is valid: {}", proj->name);
        return { true, 0, std::nullopt };
      }
      wsl::log::cli ()->error ("Failed to load project (returned null)");
      return { true, 1, std::nullopt };
    } catch (const std::exception &e) {
      wsl::log::cli ()->error ("Project validation failed: {}", e.what ());
      return { true, 1, std::nullopt };
    }
  }

  if (*validate_scene) {
    try {
      wsl::rsc::project_loader const proj_loader;
      auto proj = proj_loader.load (vs_proj_path);
      if (!proj) {
        wsl::log::cli ()->error (
            "Failed to load project context for scene validation");
        return { true, 1, std::nullopt };
      }

      wsl::comp::singl::runtime_context rtc{ "Validator", 0, 0,
                                             default_engine_resource_path (),
                                             true };
      register_all (rtc);

      const std::filesystem::path project_root = proj->root_path;
      const bool has_runtime_code
          = std::filesystem::exists (project_root / proj->systems_path)
            || std::filesystem::exists (project_root / proj->components_path)
            || std::filesystem::exists (project_root / proj->singletons_path);
      if (has_runtime_code
          && !rtc.runtime_project_module.compile_and_load (*proj)) {
        wsl::log::cli ()->error ("Runtime module validation failed: {}",
                                 rtc.runtime_project_module.last_status ());
        return { true, 1, std::nullopt };
      }

      wsl::rsc::scene scene{ &rtc, nullptr, "ValidationScene" };
      wsl::rsc::io::scene_snapshot_serializer serializer{ &rtc, scene };
      if (serializer.load_json (vs_scene_path)) {
        wsl::log::cli ()->info ("Scene is valid: {}", vs_scene_path);
        return { true, 0, std::nullopt };
      }
      wsl::log::cli ()->error ("Failed to load scene");
      return { true, 1, std::nullopt };
    } catch (const std::exception &e) {
      wsl::log::cli ()->error ("Scene validation failed: {}", e.what ());
      return { true, 1, std::nullopt };
    }
  }

  std::optional<std::string> repl_command;
  if (*proj_new) {
    repl_command
        = build_repl_command ({ "proj", "new", proj_new_path, proj_new_name });
  } else if (*proj_load) {
    repl_command = build_repl_command ({ "proj", "load", proj_load_path });
  } else if (*proj_info) {
    repl_command = build_repl_command ({ "proj", "info" });
  } else if (*proj_save) {
    repl_command = build_repl_command ({ "proj", "save" });
  } else if (*scene_new) {
    repl_command = build_repl_command ({ "scene", "new", scene_new_name });
  } else if (*scene_load) {
    repl_command = build_repl_command ({ "scene", "load", scene_load_path });
  } else if (*scene_save) {
    std::vector<std::string> args{ "scene", "save" };
    if (!scene_save_path.empty ()) {
      args.push_back (scene_save_path);
    }
    repl_command = build_repl_command (args);
  } else if (*scene_ls) {
    repl_command = build_repl_command ({ "scene", "ls" });
  } else if (*scene_status) {
    repl_command = build_repl_command ({ "scene", "status" });
  } else if (*ent_new) {
    std::vector<std::string> args{ "ent", "new" };
    if (ent_new_empty) {
      args.push_back ("--empty");
    }
    if (!ent_new_name.empty ()) {
      args.push_back (ent_new_name);
    }
    repl_command = build_repl_command (args);
  } else if (*ent_ls) {
    repl_command = build_repl_command ({ "ent", "ls" });
  } else if (*ent_rm) {
    repl_command = build_repl_command ({ "ent", "rm", ent_rm_id });
  } else if (*ent_ren) {
    repl_command
        = build_repl_command ({ "ent", "ren", ent_ren_id, ent_ren_name });
  } else if (*ent_inspect) {
    repl_command = build_repl_command ({ "ent", "inspect", ent_inspect_id });
  } else if (*comp_ls) {
    std::vector<std::string> args{ "comp", "ls" };
    if (!comp_ls_entity_id.empty ()) {
      args.push_back (comp_ls_entity_id);
    }
    repl_command = build_repl_command (args);
  } else if (*comp_avail) {
    repl_command = build_repl_command ({ "comp", "avail" });
  } else if (*comp_add) {
    repl_command = build_repl_command (
        { "comp", "add", comp_add_entity_id, comp_add_type });
  } else if (*comp_rm) {
    repl_command = build_repl_command (
        { "comp", "rm", comp_rm_entity_id, comp_rm_type });
  } else if (*comp_set) {
    repl_command
        = build_repl_command ({ "comp", "set", comp_set_entity_id,
                                comp_set_type, comp_set_prop, comp_set_val });
  } else if (*singl_ls) {
    repl_command = build_repl_command ({ "singl", "ls" });
  } else if (*singl_add) {
    repl_command = build_repl_command ({ "singl", "add", singl_add_name });
  } else if (*singl_create) {
    std::vector<std::string> args{ "singl", "create", singl_create_name };
    if (singl_create_source) {
      args.push_back ("--source");
    }
    repl_command = build_repl_command (args);
  } else if (*singl_set) {
    repl_command = build_repl_command (
        { "singl", "set", singl_set_name, singl_set_prop, singl_set_val });
  } else if (*sys_ls) {
    repl_command = build_repl_command ({ "sys", "ls" });
  } else if (*sys_avail) {
    repl_command = build_repl_command ({ "sys", "avail" });
  } else if (*sys_add) {
    repl_command = build_repl_command ({ "sys", "add", sys_add_name });
  }

  auto extras = app.remaining ();
  if (repl_command && !extras.empty ()) {
    wsl::log::cli ()->error ("Unexpected extra arguments after CLI subcommand");
    return { true, 1, std::nullopt };
  }

  // note: proj info/save and scene load/save previously required
  // --interactive or --attach but now work standalone (with auto-save
  // in one-shot mode).

  result res;
  if (!project_to_load.empty ()) {
    res.project_to_load = project_to_load;
  }
  if (!scene_to_load.empty ()) {
    res.scene_to_load = scene_to_load;
  }
  res.interactive = interactive;
  res.attach = attach;

  if (repl_command) {
    res.command = *repl_command;
  } else if (!extras.empty ()) {
    res.command = build_repl_command (extras);
  }

  if (res.interactive && res.command) {
    wsl::log::cli ()->error (
        "Choose either --interactive or a non-interactive command");
    return { true, 1, std::nullopt };
  }

  return res;
}

std::string
cli_handler::default_engine_resource_path ()
{
  const char *base_path = SDL_GetBasePath ();
  if (base_path != nullptr) {
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
