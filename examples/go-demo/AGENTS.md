# AGENTS.md

Guidance for AI agents working in this repository.

## What this is

This is a game project for the Weasel Engine, an ECS 2D & 3D game engine
for C++ and Daslang. Scenes follow the Entity Component System model:
entities hold components (Transform, RigidBody, Camera,
ModelInstance3D, ...), and behavior lives in systems.

## Project layout

- `wslpro.json` - project manifest (paths, default scene)
- `rsc/scenes/` - scenes stored as `.wscn` files
- `src/components/`, `src/systems/`, `src/singletons/` - runtime code
- `src/main.cpp` - standalone game entry point
- Asset folders: models, images, audio, fonts, shaders, materials

## Making engine calls

Prefer attaching to a running Weasel editor instance:

    weasel-cli -a ent new Player
    weasel-cli -a comp set 0 transform position '[0, 2, 0]'

`-a` attaches to the first running editor and uses its open
project. Without an editor it warns and falls back to standalone
execution (add `--project <path>` to select one). Mutations
auto-save when running standalone; when attached, saving is done
in the editor.

For interactive exploration use the REPL: `weasel-cli -i -a`

The same functionality is also exposed as MCP tools through
`weasel-mcp-server`. When connected to it, prefer those tools
(list_commands, describe_command, list_components,
describe_component, describe_namespace) over shelling out.

## Notes

- Core systems (Transform, Physics, 3D Render, ...) exist in
  every scene automatically; do not create them manually.
- Validate structural changes with:
  `weasel-cli validate-project wslpro.json`
