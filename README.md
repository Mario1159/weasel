# Weasel Engine

Weasel is an ECS 2D & 3D engine for C++ and Daslang. Made for flexibility and high-performance.

## Core Features

- **PBR Rendering:** Custom clustered forward renderer using SDL3's GPU API, supporting HDR, Bloom, SSAO, and real-time shadows.
- **Shader Graph:** Material building through nodes and slang shaders.
- **Physics:** Full integration with `Box3D`.
- **UI:** HTML & CSS support for defining UI in scenes through `RML`. 
- **Audio System:** Basic audio playback support, 3D audio planned.
- **Developer Tools:** Built-in editor and MCP server.

## Why Another Game Engine?

- Most well-known game engines are not design to fit an ECS architecture from ground-up, Weasel was design so that the user space is
fabricated strictly around the ECS concepts, understanding these concepts should be the only requirement to understand the Weasel
Editor UI and the Core Weasel-lib Architecture.
- Modern engines have non-sensical requirements, Weasel is designed to run on your laptop.
- No compromises on flexibility and performance. We choose dependencies with well-known good performance. We won't try to reinvent the wheel.
- The only other C++ & Daslang game engine is Dagor
- Like it or not, AI tools are here to stay, we try to stay updated and give the user AI-assisted tools to speed their development.

## Getting Started

Download the pre-built binaries from the Release page or get them through your package manager.
You can also build the engine from source with [xmake](https://xmake.io):

### Prerequisites

- **xmake 3.0+**
- **Clang** (the supported toolchain; GCC 16 also works)
- **C++20 Compatible Compiler**

### Building

```bash
xmake f --toolchain=clang
xmake build -j2
```

### Running the Weasel Editor

```bash
./build/weasel
```

### Get Started Building Applications

Start learning how to use the weasel engine through our `Documentation` pages.

## Local Documentation

API documentation is generated with Sphinx + hawkmoth:

```bash
xmake build --target docs
```

The output will be available at `build/docs/html/index.html`.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
