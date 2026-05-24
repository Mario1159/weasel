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
    "create-project",
    "cli",
    "Create a new Weasel Engine project on disk.",
    "weasel-cli create-project <path> <name>",
    {
      "<path> – Directory where the project will be created.",
      "<name> – Human-readable project name."
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
      "weasel-cli create-scene ./mygame/rsc/scenes/level1.wscn.json Level1"
    }
  },
  {
    "validate-project",
    "cli",
    "Validate a project file (wslpro.json).",
    "weasel-cli validate-project <path>",
    {"<path> – Path to the project's wslpro.json file."},
    {"weasel-cli validate-project ./mygame/wslpro.json"}
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
    {"weasel-cli validate-scene ./mygame/rsc/scenes/main.wscn.json ./mygame/wslpro.json"}
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
    "Save the active scene to disk.",
    "scene save [path]",
    {"[path] – Optional override path (defaults to scene's current path)."},
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
    "Remove a component from an entity.",
    "comp rm <id> <type>",
    {
      "<id> – Numeric entity ID.",
      "<type> – Component type name."
    },
    {"comp rm 42 camera"}
  },
  {
    "comp set",
    "repl",
    "Set a property on an entity's component.",
    "comp set <id> <type> <property> <value>",
    {
      "<id> – Numeric entity ID.",
      "<type> – Component type name.",
      "<property> – Property name.",
      "<value> – New value (JSON-compatible)."
    },
    {"comp set 42 transform position '[1.0, 2.0, 3.0]'"}
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
    "sys ls",
    "repl",
    "List active systems in the current scene.",
    "sys ls",
    {},
    {"sys ls"}
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
