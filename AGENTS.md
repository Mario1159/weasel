# Weasel Engine — Agent Guide

## Project Description

Weasel is an ECS 2D & 3D game engine for C++ and Daslang. It is designed from the ground up around the Entity Component System (ECS) architecture, targeting flexibility and high-performance while remaining lightweight enough to run on a laptop.

### Core Features

- **PBR Rendering:** Custom clustered forward renderer using SDL3's GPU API, supporting HDR, Bloom, SSAO, and real-time shadows.
- **Shader Graph:** Material building through nodes and Slang shaders.
- **Physics:** Full integration with Box3D.
- **UI:** HTML & CSS support via RML.
- **Audio System:** Basic audio playback support.
- **Developer Tools:** Built-in editor and MCP server.
- **AI Assistance:** A2A/ACP support comes from the external
  [agentsdk-cpp](https://github.com/ldapx/agentsdk-cpp) package
  (`<agentsdk/...>` headers, `agentsdk::a2a` / `agentsdk::acp` namespaces),
  consumed via its GitHub URL in `xmake.lua`.

## Project Structure

```
weasel/
├── cmake/                  # Legacy stb_image_impl.c (used by xmake)
├── doc/                    # Sphinx documentation sources
├── examples/               # Example projects
├── packaging/              # Linux packaging (Makefile, etc.)
├── rsc/                    # Runtime resources (shaders, fonts, icons)
├── src/
│   ├── wsl/                # Core engine library (libwsl)
│   │   ├── comp/           # ECS components
│   │   ├── sys/            # ECS systems
│   │   ├── rsc/            # Resource management
│   │   ├── phys/           # Physics integration (Box3D)
│   │   ├── gfx/            # Rendering (SDL3 GPU, PBR pipeline)
│   │   ├── math/           # Math utilities
│   │   ├── reg/            # Registry helpers
│   │   ├── das/            # Daslang bindings and modules
│   │   ├── net/            # Networking (GameNetworkingSockets)
│   │   ├── log/            # Logging (spdlog)
│   │   ├── debug/          # Debug utilities (Tracy profiler)
│   │   └── editor/         # Editor-specific engine code
│   ├── editor/             # Weasel Editor application
│   ├── cli/                # weasel-cli (command-line interface)
│   └── mcp-server/         # weasel-mcp-server (AI assistant integration)
└── tests/
    ├── weasel-cli/         # CLI unit tests (doctest)
    ├── weasel-core/        # Core engine tests: event bus, resource ids, math module (doctest)
    ├── weasel-das/         # Engine daslang component-accessor smoke test (doctest)
    └── mcp-server/         # MCP server unit tests (doctest)
```

### Build Targets

| Target | Description |
|--------|-------------|
| `wsl` | Core engine shared library (`libwsl.so` / `wsl.dll`) |
| `weasel` | Editor executable |
| `weasel-cli` | Command-line interface tool |
| `weasel-mcp-server` | MCP server for AI-assisted development |

## Building

```bash
xmake f --toolchain=clang
xmake build -j2
```

### Running the Editor

```bash
./build/weasel
```

### Generating Documentation

```bash
xmake build --target docs
```

Output is available at `build/docs/html/index.html`.

## Code Style

All code in this repository must follow the style defined in [`CODESTYLE.md`](CODESTYLE.md).

## Phase B: rfl Migration (reflect-cpp)

### Overview

Phase B migrates cereal serialization to reflect-cpp (rfl) across the engine's registry, scene, and resource layers. The key changes are:

- **Descriptor function pointers** now use `serialize::binary_writer&`/`serialize::binary_reader&`/`serialize::json_writer&`/`serialize::json_reader&` instead of `cereal::*Archive&`
- **`has_serialize` concept** simplified to just check for `serialize()` method return type
- **`component_load_entry<T>::data`** changed from `T` to `std::optional<T>` to support nullable/tombstone entries with rfl
- **`component_save_entry<T>::data`** remains `const T*` (for save); tombstone represented as `nullptr`
- **`save_component_json`/`load_component_json`** now use `writer.write(entries)` and `reader.read(entries)` instead of cereal archive operator()
- **`scene_snapshot_serializer`** uses rfl via `serialize::json_writer`/`json_reader` for JSON, `serialize::binary_writer`/`binary_reader` for binary

### Key Files

| File | Status |
|------|--------|
| `serialize/component_adapters.hpp` | ✅ Complete - 18+ component helpers with rfl CustomParser |
| `reg/component_registry.hpp/cpp` | ✅ Complete |
| `reg/singleton_registry.hpp/cpp` | ✅ Complete |
| `reg/detail/registry_helpers.hpp` | ✅ Complete |
| `rsc/scene_snapshot_serializer.hpp/cpp` | ✅ Complete |
| `rsc/project.hpp` | ✅ Complete |
| `rsc/project_loader.cpp` | ✅ Complete |
| `rsc/resource_ids.hpp` | ✅ Complete |
| `gfx/material_asset.hpp` | ✅ Complete |
| `rsc/resource_manager.cpp` | ✅ Complete |
| `math/vector.hpp` | ✅ Complete |
| `math/matrix.hpp` | ⚠️ Uses cereal (array-based, complex) |
| `serialize/types.hpp` | ✅ Binary/JSON reader/writer wrappers |
| `serialize/adapters.hpp` | ✅ rfl custom parsers for glm/entt types |

### rfl Field Definition Pattern

Types migrated to rfl use `static constexpr auto rfl_fields`:

```cpp
struct my_type {
  int x;
  std::string name;
  static constexpr auto rfl_fields = rfl::fields(
    rfl::Field<"x", &my_type::x>(),
    rfl::Field<"name", &my_type::name>()
  );
};
```

### cereal_glm.hpp / data_types_serialization.hpp

These cereal helper files are **deprecated** and replaced by `serialize/adapters.hpp`. They should not be included in new code. The `core.cppm` module file still references them but will be updated in Phase C.

### Known Issues

- **Stale LSP errors**: The LSP picks up stale system headers at `/usr/local/include/wsl/`. Actual compilation uses correct CPM-installed headers.
- **Build verification**: A full from-scratch compile may OOM on memory-constrained machines; build with `-j2` (or lower) if needed.
- **Phase C modules build**: Blocked on libarchive include path. In addition, `phys/phys.cppm` lists several headers that were removed during the Box3D migration (`broad_phase_layer_interface.hpp`, `jolt_runtime.hpp`, etc.) and must be pruned before `with_modules` builds again. Jolt itself is gone: only `phys/jolt_all.hpp` and a few module-file includes still reference it.
