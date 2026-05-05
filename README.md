# Weasel Engine

Weasel is a modern, high-performance C++20 game engine built for flexibility and speed. It features a robust Entity Component System (ECS), a custom Vulkan-based renderer via SDL_gpu, and integrated high-fidelity physics.

## Core Features

- **Advanced Rendering:** Custom PBR renderer using SDL3's GPU API, supporting HDR, Bloom, SSAO, and real-time shadows.
- **Entity Component System (ECS):** Powered by `EnTT` for high-performance data-oriented design.
- **Physics Engine:** Full integration with `Jolt Physics` for stable and scalable simulations.
- **Dynamic UI:** Rich UI development using `RmlUi` (HTML/CSS) and `ImGui` for developer tools.
- **Audio System:** Event-driven audio playback using `SDL_mixer`.
- **Serialization:** Full scene and component serialization via `cereal`.
- **Developer Tools:** Built-in editor with entity inspectors, resource management, and real-time console.

## Getting Started

### Prerequisites

- **CMake 3.22+**
- **C++20 Compatible Compiler** (GCC 11+, Clang 13+, MSVC 2022+)
- **Vulkan SDK** (for shader compilation)

### Building

```bash
cmake -B build -S .
cmake --build build
```

### Running the Example

```bash
./build/weasel
```

## Documentation

API documentation can be generated using Doxygen:

```bash
ninja -C build docs
```

The output will be available in `build/docs/html/index.html`.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
