#include "mcp_server_app.hpp"
#include "cli_reference.hpp"
#include "component_info.hpp"
#include "mcp_stdio_server.hpp"
#include "namespace_info.hpp"

#include <mcp_server.h>
#include <sstream>

namespace wsl::mcp_server {

namespace {

mcp::json
build_text_response (const std::string &text)
{
  return {
    {
      {"type", "text"},
      {"text", text}
    }
  };
}

mcp::json
handle_list_commands (const mcp::json &params)
{
  (void)params;
  const auto &ref = get_cli_reference ();

  std::ostringstream oss;
  oss << "Available weasel-cli commands:\n\n";

  oss << "== CLI Subcommands ==\n";
  for (const auto &cmd : ref) {
    if (cmd.category == "cli") {
      oss << cmd.name << " - " << cmd.description << "\n";
    }
  }

  oss << "\n== REPL Commands ==\n";
  for (const auto &cmd : ref) {
    if (cmd.category == "repl") {
      oss << cmd.name << " - " << cmd.description << "\n";
    }
  }

  return build_text_response (oss.str ());
}

mcp::json
handle_describe_command (const mcp::json &params)
{
  if (!params.contains ("name")) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                "Missing required parameter: name");
  }

  const std::string name = params["name"].get<std::string> ();
  const cli_command_info *info = find_command (name);

  if (!info) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Unknown command: " + name);
  }

  std::ostringstream oss;
  oss << "Command: " << info->name << "\n";
  oss << "Category: " << info->category << "\n";
  oss << "Description: " << info->description << "\n";
  oss << "Syntax: " << info->syntax << "\n";

  if (!info->parameters.empty ()) {
    oss << "Parameters:\n";
    for (const auto &p : info->parameters) {
      oss << "  - " << p << "\n";
    }
  }

  if (!info->examples.empty ()) {
    oss << "Examples:\n";
    for (const auto &ex : info->examples) {
      oss << "  " << ex << "\n";
    }
  }

  return build_text_response (oss.str ());
}

mcp::json
handle_get_quick_start (const mcp::json &params)
{
  (void)params;
  std::ostringstream oss;
  oss << "Weasel Engine CLI Quick Start\n\n";
  oss << "1. Create a project:\n";
  oss << "   weasel-cli create-project ./mygame MyGame\n\n";
  oss << "2. Create a scene:\n";
  oss << "   weasel-cli create-scene ./mygame/rsc/scenes/level1.wscn.json Level1\n\n";
  oss << "3. Validate a project:\n";
  oss << "   weasel-cli validate-project ./mygame/wslpro.json\n\n";
  oss << "4. Start the interactive REPL:\n";
  oss << "   weasel-cli --project ./mygame/wslpro.json --interactive\n\n";
  oss << "   Inside the REPL you can manage entities, components, scenes, and more:\n";
  oss << "     scene new Level1              # Create a new scene\n";
  oss << "     ent new Player                # Create an entity named Player\n";
  oss << "     comp add 1 transform          # Add Transform component\n";
  oss << "     comp set 1 transform position '[0,2,0]'  # Set position\n";
  oss << "     comp add 1 rigid_body         # Add RigidBody component\n";
  oss << "     comp set 1 rigid_body shape sphere  # Set shape to sphere\n";
  oss << "     comp set 1 rigid_body radius 0.5      # Set radius\n";
  oss << "     comp rm 1 rigid_body          # Remove component\n";
  oss << "     ent inspect 1                 # Inspect entity\n";
  oss << "     scene save                    # Save scene\n";
  oss << "     help                          # Show full command reference\n\n";
  oss << "5. Attach to a running editor:\n";
  oss << "   weasel-cli --project ./mygame/wslpro.json --attach\n\n";
  oss << "For full command reference, use the list_commands and describe_command tools.\n";
  oss << "For component property details, use describe_component (e.g. Rigid Body).\n";
  oss << "For implementation status, use cli_capabilities.";

  return build_text_response (oss.str ());
}

mcp::json
handle_cli_capabilities (const mcp::json &params)
{
  (void)params;
  std::ostringstream oss;
  oss << "Weasel CLI/REPL Capabilities Manifest\n";
  oss << "======================================\n\n";

  oss << "== Fully Implemented ==\n";
  oss << "  proj new        - Create and load a new project\n";
  oss << "  proj load       - Load an existing project\n";
  oss << "  proj info       - Show project metadata\n";
  oss << "  proj save       - Save project metadata\n";
  oss << "  scene new       - Create a new empty scene\n";
  oss << "  scene load      - Load a scene from disk\n";
  oss << "  scene save      - Save the active scene\n";
  oss << "  scene ls        - List scene assets\n";
  oss << "  scene status    - Show scene statistics\n";
  oss << "  ent new         - Create a new entity\n";
  oss << "  ent ls          - List all entities\n";
  oss << "  ent rm          - Remove (destroy) an entity\n";
  oss << "  ent ren         - Rename an entity\n";
  oss << "  ent inspect     - Show entity details\n";
  oss << "  comp ls         - List components (registered or on an entity)\n";
  oss << "  comp avail      - List registered component types\n";
  oss << "  comp add        - Add a component to an entity\n";
  oss << "  comp rm         - Remove a component from an entity\n";
  oss << "  comp set        - Set a component property\n";
  oss << "  sys ls          - List active systems\n";
  oss << "  sys avail       - List registered systems\n";
  oss << "  sys add         - Add a system to the active scene\n";
  oss << "  check           - Run validation checks\n";
  oss << "  rsc ls          - List resources\n";
  oss << "  help            - Show help\n";
  oss << "  exit/quit       - Exit REPL\n";
  oss << "  cls             - Clear screen\n\n";

  oss << "== Documented but NOT Implemented ==\n";
  oss << "  sig             - Signal management (stub only)\n\n";

  oss << "== Previously Fixed Limitations ==\n";
  oss << "  - Component type names now accept short names (transform,\n";
  oss << "    rigid_body), display names (Transform), or fully qualified\n";
  oss << "    C++ names (wsl::comp::transform)\n";
  oss << "  - model_id property on model_instance_3d now accepts\n";
  oss << "    builtin:// paths (e.g. builtin://cube) without crashing\n";
  oss << "  - sys add <name> can attach systems to scenes dynamically\n";
  oss << "    (e.g. Transform, Physics, 3D Render)\n";
  oss << "  - Nested comp set properties (motion_type.value,\n";
  oss << "    collision_layer.value) now persist through scene save\n";
  oss << "  - scene save without a path writes to project's scenes_path\n";
  oss << "    (no double .wscn.json extension)\n";
  oss << "  - # comments and inline comments are now supported in REPL\n\n";
  oss << "== Remaining Notes ==\n";
  oss << "  - `scene new` creates a scene with no systems attached\n";
  oss << "    (use sys add to add them)\n";
  oss << "  - `comp set` supports primitive types, enums (by name or int),\n";
  oss << "    compound types (vec3, quat) via JSON arrays/objects,\n";
  oss << "    and nested property paths (e.g. motion_type.value)\n";
  oss << "  - Physics components modified via comp set may need manual\n";
  oss << "    re-sync (e.g. rigid_body changes require on_inspector_changed)\n";
  oss << "  - comp set uses EnTT meta property names (e.g. motion_type)\n";
  oss << "    NOT display names (e.g. Motion Type) or serialization names\n";
  oss << "  - comp set only works on components with register_meta() called\n";

  return build_text_response (oss.str ());
}

} // namespace

mcp_server_app::mcp_server_app (const std::string &host, int port)
  : m_server (mcp::server::configuration{host, port})
{
  mcp::json capabilities = {
    {"tools", mcp::json::object ()}
  };

  m_server.set_server_info ("Weasel MCP Server", "0.1.0");
  m_server.set_capabilities (capabilities);
  m_server.set_instructions (
    "This MCP server provides reference documentation for the Weasel Engine. "
    "Available tool categories:\n"
    "  CLI/REPL: list_commands, describe_command, get_quick_start, cli_capabilities\n"
    "  Components: list_components, describe_component\n"
    "  Engine Namespaces: list_namespaces, describe_namespace — learn how to "
    "write a game using comp/sys/rsc/phys/gfx/math/reg");

  m_stdio_server.set_server_info ("Weasel MCP Server", "0.1.0");
  m_stdio_server.set_capabilities (capabilities);
  m_stdio_server.set_instructions (
    "This MCP server provides reference documentation for the Weasel Engine. "
    "Available tool categories:\n"
    "  CLI/REPL: list_commands, describe_command, get_quick_start, cli_capabilities\n"
    "  Components: list_components, describe_component\n"
    "  Engine Namespaces: list_namespaces, describe_namespace — learn how to "
    "write a game using comp/sys/rsc/phys/gfx/math/reg");

  register_tools ();
}

void
mcp_server_app::set_stdio_mode (bool enable)
{
  stdio_mode_ = enable;
}

void
mcp_server_app::run ()
{
  if (stdio_mode_) {
    m_stdio_server.run ();
  } else {
    m_server.start (true);
  }
}

void
mcp_server_app::register_tools ()
{
  mcp::tool list_tool = mcp::tool_builder ("list_commands")
                          .with_description (
                            "List all available weasel-cli and REPL commands.")
                          .build ();
  m_server.register_tool (list_tool, [] (const mcp::json &params,
                                          const std::string & /*session_id*/) {
    return handle_list_commands (params);
  });
  m_stdio_server.register_tool (list_tool, [] (const mcp::json &params,
                                                const std::string & /*session_id*/) {
    return handle_list_commands (params);
  });

  mcp::tool desc_tool = mcp::tool_builder ("describe_command")
                          .with_description (
                            "Get detailed usage information for a specific command.")
                          .with_string_param ("name",
                                               "Name of the command to describe.")
                          .build ();
  m_server.register_tool (desc_tool, [] (const mcp::json &params,
                                          const std::string & /*session_id*/) {
    return handle_describe_command (params);
  });
  m_stdio_server.register_tool (desc_tool, [] (const mcp::json &params,
                                                const std::string & /*session_id*/) {
    return handle_describe_command (params);
  });

  mcp::tool quick_tool = mcp::tool_builder ("get_quick_start")
                            .with_description (
                              "Return a quick-start guide for the Weasel CLI.")
                            .build ();
  m_server.register_tool (quick_tool, [] (const mcp::json &params,
                                          const std::string & /*session_id*/) {
    return handle_get_quick_start (params);
  });
  m_stdio_server.register_tool (quick_tool, [] (const mcp::json &params,
                                                const std::string & /*session_id*/) {
    return handle_get_quick_start (params);
  });

  // --- Component introspection tools ---

  mcp::tool list_comp_tool
      = mcp::tool_builder ("list_components")
            .with_description (
                "List all registered ECS components with their fully "
                "qualified C++ type names.")
            .build ();
  m_server.register_tool (list_comp_tool,
                          [] (const mcp::json &params,
                              const std::string & /*session_id*/) {
                            return handle_list_components (params);
                          });
  m_stdio_server.register_tool (list_comp_tool,
                                [] (const mcp::json &params,
                                    const std::string & /*session_id*/) {
                                  return handle_list_components (params);
                                });

  mcp::tool desc_comp_tool
      = mcp::tool_builder ("describe_component")
            .with_description (
                "Get detailed information about a registered component: all "
                "properties, their types, enum values, and descriptions.")
            .with_string_param ("name",
                                "Component name (display name like 'Rigid Body' "
                                "or C++ type like 'wsl::comp::rigid_body').")
            .build ();
  m_server.register_tool (desc_comp_tool,
                          [] (const mcp::json &params,
                              const std::string & /*session_id*/) {
                            return handle_describe_component (params);
                          });
  m_stdio_server.register_tool (desc_comp_tool,
                                [] (const mcp::json &params,
                                    const std::string & /*session_id*/) {
                                  return handle_describe_component (params);
                                });

  // --- Engine Namespace documentation tools ---

  mcp::tool list_ns_tool
      = mcp::tool_builder ("list_namespaces")
            .with_description (
                "List all engine namespaces (comp, sys, rsc, phys, gfx, math, "
                "reg) with brief descriptions. Use describe_namespace for "
                "detailed documentation on how to use each one.")
            .build ();
  m_server.register_tool (list_ns_tool,
                          [] (const mcp::json &params,
                              const std::string & /*session_id*/) {
                            return handle_list_namespaces (params);
                          });
  m_stdio_server.register_tool (list_ns_tool,
                                [] (const mcp::json &params,
                                    const std::string & /*session_id*/) {
                                  return handle_list_namespaces (params);
                                });

  mcp::tool desc_ns_tool
      = mcp::tool_builder ("describe_namespace")
            .with_description (
                "Get detailed documentation about an engine namespace: what "
                "it contains, key classes, and how to use it to write a game.")
            .with_string_param (
                "name",
                "Namespace name: comp, sys, rsc, phys, gfx, math, or reg.")
            .build ();
  m_server.register_tool (desc_ns_tool,
                          [] (const mcp::json &params,
                              const std::string & /*session_id*/) {
                            return handle_describe_namespace (params);
                          });
  m_stdio_server.register_tool (desc_ns_tool,
                                [] (const mcp::json &params,
                                    const std::string & /*session_id*/) {
                                  return handle_describe_namespace (params);
                                });

  // --- CLI capabilities tool ---

  mcp::tool cli_cap_tool = mcp::tool_builder ("cli_capabilities")
                               .with_description (
                                   "List which CLI/REPL commands are actually "
                                   "implemented vs. documented but missing.")
                               .build ();
  m_server.register_tool (
      cli_cap_tool, [] (const mcp::json &params,
                        const std::string & /*session_id*/) {
        return handle_cli_capabilities (params);
      });
  m_stdio_server.register_tool (
      cli_cap_tool, [] (const mcp::json &params,
                        const std::string & /*session_id*/) {
        return handle_cli_capabilities (params);
      });
}

} // namespace wsl::mcp_server
