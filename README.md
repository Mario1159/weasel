# Weasel Engine

Weasel is an ECS 2D & 3D engine for C++ and Daslang. Made for flexibility and high-performance.

## Core Features

- **PBR Rendering:** Custom clustered forward renderer using SDL3's GPU API, supporting HDR, Bloom, SSAO, and real-time shadows.
- **Shader Graph:** Material building through nodes and slang shaders.
- **Physics:** Full integration with `Jolt Physics` engine.
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
You can also easily build the engine using CMake:

### Prerequisites

- **CMake 3.22+**
- **C++20 Compatible Compiler** (GCC 11+, Clang 13+, MSVC 2022+)

### Building

```bash
cmake -B build -S .
cmake --build build
```

### Running the Weasel Editor

```bash
./build/weasel
```

### Get Started Building Applications

Start learning how to use the weasel engine through our `Documentation` pages.

## Local Documentation

API documentation can be generated using Doxygen:

```bash
ninja -C build docs
```

The output will be available in `build/docs/html/index.html`.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
