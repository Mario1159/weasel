# Weasel Engine — Agent Guide

## Project Description

Weasel is an ECS 2D & 3D game engine for C++ and Daslang. It is designed from the ground up around the Entity Component System (ECS) architecture, targeting flexibility and high-performance while remaining lightweight enough to run on a laptop.

### Core Features

- **PBR Rendering:** Custom clustered forward renderer using SDL3's GPU API, supporting HDR, Bloom, SSAO, and real-time shadows.
- **Shader Graph:** Material building through nodes and Slang shaders.
- **Physics:** Full integration with Jolt Physics.
- **UI:** HTML & CSS support via RML.
- **Audio System:** Basic audio playback support.
- **Developer Tools:** Built-in editor and MCP server.

## Project Structure

```
weasel/
├── cmake/                  # CMake modules (CPM, Daslang integration, stb, etc.)
├── doc/                    # Sphinx documentation sources
├── examples/               # Example projects
├── packaging/              # Linux packaging (Makefile, etc.)
├── rsc/                    # Runtime resources (shaders, fonts, icons)
├── src/
│   ├── wsl/                # Core engine library (libwsl)
│   │   ├── comp/           # ECS components
│   │   ├── sys/            # ECS systems
│   │   ├── rsc/            # Resource management
│   │   ├── phys/           # Physics integration (Jolt)
│   │   ├── gfx/            # Rendering (SDL3 GPU, PBR pipeline)
│   │   ├── math/           # Math utilities
│   │   ├── reg/            # Registry helpers
│   │   ├── das/            # Daslang bindings and modules
│   │   ├── ai/             # AI integration (A2A, MCP)
│   │   ├── net/            # Networking (GameNetworkingSockets)
│   │   ├── log/            # Logging (spdlog)
│   │   ├── debug/          # Debug utilities (Tracy profiler)
│   │   └── editor/         # Editor-specific engine code
│   ├── editor/             # Weasel Editor application
│   ├── cli/                # weasel-cli (command-line interface)
│   └── mcp-server/         # weasel-mcp-server (AI assistant integration)
└── tests/
    ├── weasel-cli/         # CLI unit tests (doctest)
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
make configure
cmake --build build -j2
```

### Running the Editor

```bash
./build/weasel
```

### Generating Documentation

```bash
cmake --build build --target docs
```

Output is available at `build/docs/html/index.html`.

## Code Style

All code in this repository must follow the style defined in [`CODESTYLE.md`](CODESTYLE.md).
