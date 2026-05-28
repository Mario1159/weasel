#include "cli_reference.hpp"

namespace {

const std::vector<cli_command_info> g_cli_reference = {
  // === CLI Subcommands ===
  {
    "--project",
    "cli",
    "Path to the project to load at startup.",
    "weasel-cli --project <path> [--interactive]",
    {"<path> – Path to the project directory or wslpro.json file."},
    {"weasel-cli --project ./mygame --interactive"}
  },
  {
    "--interactive",
    "cli",
    "Start the interactive REPL after loading the project.",
    "weasel-cli --project <path> --interactive",
    {},
    {"weasel-cli --project ./mygame --interactive"}
  },
  {
    "--attach",
    "cli",
    "Attach to a running editor server for the project.",
    "weasel-cli --project <path> --attach",
    {},
    {"weasel-cli --project ./mygame --attach"}
  },
  {
    "--scene",
    "cli",
    "Path to a scene file (.wscn.json) to load before running a command.",
    "weasel-cli --project <path> --scene <scene_path>",
    {"<scene_path> – Path to the .wscn.json scene file."},
    {"weasel-cli --project ./mygame --scene rsc/scenes/level1.wscn.json"}
  },
  {
    "create-project",
    "cli",
    "Create a new Weasel Engine project on disk.",
    "weasel-cli create-project <path> <name> [--no-subdir]",
    {
      "<path> – Directory where the project will be created.",
      "<name> – Human-readable project name.",
      "--no-subdir – Don't create a project-name subdirectory."
    },
    {
      "weasel-cli create-project ./mygame MyGame",
      "weasel-cli --create-project ./mygame MyGame"
    }
  },
  {
    "create-scene",
    "cli",
    "Create a new scene file for a project.",
    "weasel-cli create-scene <path> <name> [systems...]",
    {
      "<path> – File path for the new scene (.wscn.json).",
      "<name> – Scene name.",
      "[systems...] – Optional system names to attach."
    },
    {
      "weasel-cli create-scene ./mygame/rsc/scenes/level1.wscn.json Level1",
      "weasel-cli --create-scene ./mygame/rsc/scenes/level1.wscn.json Level1"
    }
  },
  {
    "validate-project",
    "cli",
    "Validate a project file (wslpro.json).",
    "weasel-cli validate-project <path>",
    {"<path> – Path to the project's wslpro.json file."},
    {"weasel-cli validate-project ./mygame/wslpro.json", "weasel-cli --validate-project ./mygame/wslpro.json"}
  },
  {
    "validate-scene",
    "cli",
    "Validate a scene file against a project.",
    "weasel-cli validate-scene <scene_path> <proj_path>",
    {
      "<scene_path> – Path to the scene .wscn.json file.",
      "<proj_path> – Path to the project's wslpro.json file."
    },
    {"weasel-cli validate-scene ./mygame/rsc/scenes/main.wscn.json ./mygame/wslpro.json", "weasel-cli --validate-scene ./mygame/rsc/scenes/main.wscn.json ./mygame/wslpro.json"}
  },

  {
    "proj",
    "cli",
    "Run a project REPL command without entering the interactive shell.",
    "weasel-cli proj <new|load|info|save> [args...]",
    {
      "<new|load|info|save> – Project subcommand."
    },
    {"weasel-cli proj load ./mygame", "weasel-cli proj info"}
  },
  {
    "scene",
    "cli",
    "Run a scene REPL command without entering the interactive shell.",
    "weasel-cli scene <new|load|save|ls|status> [args...]",
    {
      "<new|load|save|ls|status> – Scene subcommand."
    },
    {"weasel-cli scene ls", "weasel-cli scene status"}
  },
  {
    "ent",
    "cli",
    "Run an entity REPL command without entering the interactive shell.",
    "weasel-cli ent <new|ls|rm|ren|inspect> [args...]",
    {
      "<new|ls|rm|ren|inspect> – Entity subcommand."
    },
    {"weasel-cli ent ls", "weasel-cli ent new Player"}
  },
  {
    "comp",
    "cli",
    "Run a component REPL command without entering the interactive shell.",
    "weasel-cli comp <ls|avail|add|rm|set> [args...]",
    {
      "<ls|avail|add|rm|set> – Component subcommand."
    },
    {"weasel-cli comp avail", "weasel-cli comp add 42 transform", "weasel-cli comp rm 42 rigid_body", "weasel-cli comp set 42 transform position '[1,2,3]'"}
  },
  {
    "sys",
    "cli",
    "Run a system REPL command without entering the interactive shell.",
    "weasel-cli sys <ls|avail|add>",
    {
      "<ls|avail> – List registered systems.",
      "add <name> – Add a system to the active scene."
    },
    {"weasel-cli sys ls", "weasel-cli sys avail"}
  },

  // === REPL Commands ===
  {
    "proj new",
    "repl",
    "Create and load a new project in the REPL.",
    "proj new <name> <path>",
    {
      "<name> – Project name.",
      "<path> – Directory to create the project in."
    },
    {"proj new MyGame ./projects"}
  },
  {
    "proj load",
    "repl",
    "Load an existing project into the REPL.",
    "proj load <path>",
    {"<path> – Path to the project directory or wslpro.json."},
    {"proj load ./mygame"}
  },
  {
    "proj info",
    "repl",
    "Display metadata for the currently loaded project.",
    "proj info",
    {},
    {"proj info"}
  },
  {
    "proj save",
    "repl",
    "Save the currently loaded project metadata.",
    "proj save",
    {},
    {"proj save"}
  },
  {
    "scene new",
    "repl",
    "Create a new empty scene and set it as active.",
    "scene new <name>",
    {"<name> – Name for the new scene."},
    {"scene new Level1"}
  },
  {
    "scene load",
    "repl",
    "Load a scene from disk into the active runtime.",
    "scene load <path>",
    {"<path> – Path to the .wscn.json scene file."},
    {"scene load rsc/scenes/level1.wscn.json"}
  },
  {
    "scene save",
    "repl",
    "Save the active scene to disk. When a project is loaded, the default "
    "path is {project_root}/{scenes_path}/{scene_name}.wscn.json. "
    "Without a project, defaults to {scene_name}.wscn.json in the CWD.",
    "scene save [path]",
    {
      "[path] – Optional override path. Default: project's scenes_path + scene name.",
      "When a project is loaded, saves to the project's configured scenes_path.",
      "Without a project, saves to the current working directory."
    },
    {"scene save", "scene save ./backup.wscn.json"}
  },
  {
    "scene ls",
    "repl",
    "List all scene assets in the loaded project.",
    "scene ls",
    {},
    {"scene ls"}
  },
  {
    "scene status",
    "repl",
    "Show statistics for the active scene.",
    "scene status",
    {},
    {"scene status"}
  },
  {
    "ent new",
    "repl",
    "Create a new entity in the active scene.",
    "ent new [name]",
    {"[name] – Optional entity name."},
    {"ent new", "ent new Player"}
  },
  {
    "ent ls",
    "repl",
    "List all entities in the active scene.",
    "ent ls",
    {},
    {"ent ls"}
  },
  {
    "ent rm",
    "repl",
    "Remove (destroy) an entity by its numeric ID.",
    "ent rm <id>",
    {"<id> – Numeric entity ID."},
    {"ent rm 42"}
  },
  {
    "ent ren",
    "repl",
    "Rename an existing entity.",
    "ent ren <id> <new_name>",
    {
      "<id> – Numeric entity ID.",
      "<new_name> – New name for the entity."
    },
    {"ent ren 42 Hero"}
  },
  {
    "ent inspect",
    "repl",
    "Display detailed information about an entity.",
    "ent inspect <id>",
    {"<id> – Numeric entity ID."},
    {"ent inspect 42"}
  },
  {
    "comp ls",
    "repl",
    "List components attached to an entity.",
    "comp ls <id>",
    {"<id> – Numeric entity ID."},
    {"comp ls 42"}
  },
  {
    "comp avail",
    "repl",
    "List all registered component types available in the runtime.",
    "comp avail",
    {},
    {"comp avail"}
  },
  {
    "comp add",
    "repl",
    "Add a component to an entity by type name.",
    "comp add <id> <type>",
    {
      "<id> – Numeric entity ID.",
      "<type> – Component type name (e.g. transform, camera, model_instance_3d)."
    },
    {"comp add 42 transform"}
  },
  {
    "comp rm",
    "repl",
    "Remove a component from an entity. The entity must have the component already attached.",
    "comp rm <id> <type>",
    {
      "<id> – Numeric entity ID.",
      "<type> – Component type name (e.g. rigid_body, camera, transform)."
    },
    {"comp rm 42 camera", "comp rm 42 rigid_body"}
  },
  {
    "comp set",
    "repl",
    "Set a property on an entity's component. Uses EnTT meta property names "
    "(NOT serialization/JSON names). Only works on components that have been "
    "initialized with register_meta(). Supports nested property paths "
    "(dot-separated) and JSON-compatible values.",
    "comp set <id> <type> <property> <value>",
    {
      "<id> – Numeric entity ID.",
      "<type> – Component type name (e.g. transform, rigid_body). Short names, display names, and fully qualified C++ names all work.",
      "<property> – EnTT meta property name (e.g. motion_type, not 'Motion Type'). Use dot notation for nested fields (e.g. motion_type.value, collision_layer.value). These differ from serialization names in some cases.",
      "<value> – New value as JSON: numbers (1.5), strings (\\\"text\\\"), booleans (true/false), arrays ([1,2,3]), or enum names (Box, Sphere, Static, Dynamic, Kinematic)."
    },
    {"comp set 42 transform position '[1.0, 2.0, 3.0]'", "comp set 42 rigid_body shape Box", "comp set 42 rigid_body radius 0.5", "comp set 42 rigid_body motion_type.value 2"}
  },
  {
    "sig",
    "repl",
    "Signal management (not yet fully implemented).",
    "sig <subcommand> [args...]",
    {},
    {}
  },
  {
    "sys add",
    "repl",
    "Add a system to the active scene by registered name.",
    "sys add <name>",
    {
      "<name> – System name (e.g. Transform, Physics, 3D Render, Audio, Lighting, Skybox, Shadow, UI). System names are case-sensitive."
    },
    {"sys add Transform", "sys add Physics", "sys add 3D Render"}
  },
  {
    "sys ls",
    "repl",
    "List active systems in the current scene.",
    "sys ls",
    {},
    {"sys ls"}
  },
  {
    "sys avail",
    "repl",
    "Alias for sys ls — list registered systems.",
    "sys avail",
    {},
    {"sys avail"}
  },
  {
    "check",
    "repl",
    "Run a validation check on a target.",
    "check <target>",
    {"<target> – Target to validate (e.g. project, scene)."},
    {"check project"}
  },
  {
    "help",
    "repl",
    "Show the REPL help message.",
    "help",
    {},
    {"help"}
  },
  {
    "exit",
    "repl",
    "Exit the REPL session.",
    "exit",
    {},
    {"exit"}
  },
  {
    "quit",
    "repl",
    "Alias for exit.",
    "quit",
    {},
    {"quit"}
  },
  {
    "cls",
    "repl",
    "Clear the terminal screen.",
    "cls",
    {},
    {"cls"}
  }
};

} // namespace

const std::vector<cli_command_info> &
get_cli_reference ()
{
  return g_cli_reference;
}

const cli_command_info *
find_command (const std::string &name)
{
  for (const auto &cmd : g_cli_reference) {
    if (cmd.name == name) {
      return &cmd;
    }
  }
  return nullptr;
}
