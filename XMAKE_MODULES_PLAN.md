# Plan: xmake + C++20 Modules for the Weasel Engine

> Status: **Proposal** — replaces the CMake-based CXX_MODULES_PLAN_V2.md
> Scope: `src/wsl` (the `wsl` shared library) and its consumers
> Toolchain: xmake 3.1.1 + Clang 22 (GCC 14+ also works)
> ABI: **breaking** — consumers MUST `import wsl.core;` (no header fallback)

## Why xmake instead of CMake

CMake 4.4's C++20 modules implementation has three blockers we hit:

1. **Header units not supported** (stated limitation) — Jolt HU unworkable
2. **Impl-unit CMI merge failures** — GCC 16 and Clang 22 both fail to merge
   CMI-streamed declarations with fresh textual declarations of the same
   headers in impl-unit GMFs (`_Rb_tree`, `FontEffectList`, etc.)
3. **Dyndep crash** (Ninja #2592, open) — parallel builds crash

xmake solves all three:

1. **Header units supported natively** — STL header units and custom header
   units work out of the box with `add_files("*.mpp")`
2. **BMI ordering handled automatically** — xmake scans `.mpp`/`.cppm` files,
   builds BMIs in dependency order, no custom commands or response files needed
3. **No dyndep** — xmake uses its own async job system for module ordering,
   not ninja's dyndep (avoids the #2592 crash entirely)

## Why ABI-breaking style

The Clang modules transitioning guide defines three styles. We use the
**ABI-breaking style** because:

- We don't ship ABI (the engine is compiled from source by consumers)
- It forces a clean module boundary: consumers either `import wsl.core;` or
  use the legacy headers — never both (prevents the CMI/textual merge
  conflicts that blocked us in Phases 2–5)
- The module-owned ABI generates smaller binaries (vtables and inline
  functions are generated once in the module unit, not per-TU)
- It enables future `import std;` adoption without mixing with textual
  std includes

## Architecture: one `wsl.core` module

The engine's include graph is one strongly-connected component
(`comp↔gfx↔rsc↔sys↔reg↔das↔event`). Cross-module imports between SCC
members are illegal in C++20. The only viable structure is **one module
containing the entire cluster**.

```
wsl.core (single module)
├── core.cppm           ← primary module interface unit
│                         GMF: third-party includes only
│                         purview: ALL engine headers (148)
│                         non-exported using-decls for 3rd-party types
├── (92 impl units)     ← `module wsl.core;` — NO includes, NO GMF
│                         all types from the interface CMI
└── consumers           ← `import wsl.core;` + their own third-party includes
```

### Key rule: engine headers go in the PURVIEW, not the GMF

This is the **ABI-breaking** pattern from the Clang docs:

```cpp
// core.cppm
module;
// ── GMF: third-party ONLY (global-attached) ──
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <Jolt/Jolt.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
// ... etc

export module wsl.core;
#define IN_MODULE_INTERFACE

// ── PURVIEW: engine headers (types = @wsl.core-attached) ──
#include "math/vector.hpp"       // its #include <glm/...> is SKIPPED
#include "comp/component_meta.hpp" // its #include <entt/...> is SKIPPED
#include "app.hpp"
// ... all 148 headers
```

Engine headers use conditional includes:

```cpp
// math/vector.hpp
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#endif
#include <cstdint>  // std always available

namespace wsl::math {
struct vec2f { ... };  // references glm types from the module's GMF
}
```

### Key rule: impl units have NO includes

```cpp
// comp/area3d.cpp
module wsl.core;

// All types visible from the interface CMI:
// - wsl::math::vec3f (@wsl.core from the purview)
// - JPH::BodyID (global-attached from the GMF, reachable)
// - wsl::rsc::resource_manager (@wsl.core)
void area::create_body(phys::engine &engine, ...) {
    // ...
}
```

No `#include` in impl units. No GMF. No overlap with the interface. The CMI
provides everything because the interface's PURVIEW included the headers
(making all declarations — including third-party — @wsl.core-attached).

### Key rule: non-exported using-decls for third-party types

For third-party types used by impl-unit bodies that aren't fully reachable
from the interface's exported declarations:

```cpp
// core.cppm (after export module, in the purview)
// Non-exported: visible to impl units, not to consumers
using ::RenderInterface_SDL_GPU;
using ::SystemInterface_SDL;
using Rml::Vector;
using JPH::BodyID;
```

---

## Phased Implementation

### Phase A — Build System Migration (xmake replaces CMake)

Replace `CMakeLists.txt` with `xmake.lua`. Keep the legacy header build
(no modules yet). Goal: same binary, same tests, xmake instead of CMake.

**xmake.lua skeleton:**

```lua
set_project("weasel")
set_languages("c++20")
set_policy("build.warning", true)

-- Third-party packages (xmake repo replaces CPM)
add_requires("entt v3.15.0", "glm", "spdlog v1.17.0", "fmt", "reflectcpp v0.25.0")
add_requires("joltphysics v5.4.0", "rmlui", "fastgltf", "meshoptimizer")
add_requires("simdjson", "tracy", "nlohmann_json")

target("wsl")
    set_kind("shared")
    set_languages("c++20")
    add_files("src/wsl/**.cpp")
    add_includedirs("src", "src/wsl", {public = true})
    add_packages("entt", "glm", "spdlog", "fmt", "reflectcpp",
                 "joltphysics", "rmlui", "fastgltf", "meshoptimizer",
                 "simdjson", "tracy", "nlohmann_json")
    add_defines("CPP_RTTI_ENABLED", "JPH_DEBUG_RENDERER",
                "RMLUI_SDL_VERSION_MAJOR=3", "RMLUI_STATIC_LIB",
                "SPDLOG_COMPILED_LIB", "TRACY_ENABLE", "TRACY_ON_DEMAND")

target("weasel")
    set_kind("binary")
    add_files("src/editor/**.cpp")
    add_deps("wsl")

target("weasel-cli")
    set_kind("binary")
    add_files("src/cli/**.cpp")
    add_deps("wsl")

target("weasel-mcp-server")
    set_kind("binary")
    add_files("src/mcp-server/**.cpp")
    add_deps("wsl")
```

**Steps:**
1. Write `xmake.lua` with all 25 dependencies (xmake packages or CPM-style
   `add_requires` with git repos for packages not in xrepo)
2. Port the 25 CPM dependency declarations to xmake `add_requires`
3. Wire the wsl, weasel, weasel-cli, weasel-mcp-server targets
4. Handle the daslang dependency (CPM → xmake package or custom build rule)
5. Handle the slang dependency (binary distribution, custom build rule)
6. Build all targets green
7. Run all 4 test suites green
8. Delete CMakeLists.txt files, delete build/ directory

**Risk: daScript/slang have complex CMake builds.** If xmake packaging
fails for them, keep them as pre-built libs (like the current approach)
and link via `add_linkdirs`/`add_links`.

### Phase B — Serialization Migration (cereal → reflect-cpp)

Same as CXX_MODULES_PLAN_V2.md Phase 1 (already designed and validated):

1. Add reflect-cpp 0.25.0 via xmake `add_requires`
2. Create `src/wsl/serialize/` (types.hpp, serialize.hpp, adapters.hpp,
   serialize.cpp, component_adapters.hpp)
3. Replace `serialize_field_if_diff` with `save_field_if_diff`
4. Migrate comp/component_meta.hpp, rsc/*, then the 38 cereal files
5. Delete cereal from the dependency list

### Phase C — Module Conversion (ABI-breaking style)

With xmake's module support, convert the engine to `wsl.core`:

**Step C.1 — Header conditional includes:**

Add `#ifndef IN_MODULE_INTERFACE` guards to all third-party includes in
engine headers (~129 headers):

```cpp
// math/vector.hpp
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#endif
#include <cstdint>  // or import std; if using c++23
```

**Step C.2 — Create core.cppm:**

```cpp
// src/wsl/core/core.cppm
module;
// ── GMF: third-party ONLY ──
#include <entt/entt.hpp>
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <Jolt/Jolt.h>
// ... all third-party

#define IN_MODULE_INTERFACE
#include "math/vector.hpp"
// ... all 148 engine headers

export module wsl.core;

// Non-exported using-decls for third-party types in impl-unit bodies
using ::RenderInterface_SDL_GPU;
using ::SystemInterface_SDL;
// ... as needed
```

**Step C.3 — Convert impl units:**

Strip ALL includes from .cpp files, replace with `module wsl.core;`:

```cpp
// comp/area3d.cpp
module wsl.core;

// body unchanged — all types from the interface CMI
```

**Step C.4 — xmake.lua for modules:**

```lua
target("wsl")
    set_kind("shared")
    set_languages("c++20")
    set_policy("build.c++.modules", true)
    add_files("src/wsl/core/core.cppm", {public = true})
    add_files("src/wsl/**.cpp")
    -- third-party packages
    add_packages("...")
```

xmake handles:
- BMI generation for core.cppm
- Dependency scanning for impl units (discovers `module wsl.core;`)
- Ordering (core.cppm before impl units)
- Module map generation for Clang/GCC
- No custom commands, no response files, no .gcm management

**Step C.5 — Consumers:**

```lua
target("weasel")
    set_kind("binary")
    add_deps("wsl")  -- inherits module search path automatically
    add_files("src/editor/**.cpp")
```

Editor .cpp files use `import wsl.core;` instead of `#include "wsl/..."`.
They keep their own third-party includes (RmlUi, ImGui, etc.) which are
NOT in the module (only used internally by the engine).

**Step C.6 — import std (optional, c++23):**

```lua
set_languages("c++23")
set_policy("build.c++.modules.std", true)
```

Then `import std;` replaces textual STL includes in the core interface
and impl units. Requires GCC 15+ or Clang 18+.

### Phase D — Cleanup

1. Remove `CXX_MODULES_PLAN_V2.md`, `XMAKE_MODULES_PLAN.md` (this file)
2. Remove all `WSL_MODULE_BUILD` / `IN_MODULE_INTERFACE` guards (no longer
   needed — headers are only consumed via the module)
3. Remove the legacy header installation (consumers use BMIs)
4. Update doc/ with module usage examples

## Dependency Mapping (CPM → xmake)

| CPM Package | xmake `add_requires` | Notes |
|---|---|---|
| entt | `add_requires("entt")` | xrepo has it |
| glm | `add_requires("glm")` | |
| spdlog | `add_requires("spdlog")` | |
| fmt | `add_requires("fmt")` | |
| cereal | ~~`add_requires("cereal")`~~ | **REMOVED** — replaced by reflect-cpp |
| reflectcpp | `add_requires("reflectcpp")` | New |
| JoltPhysics | `add_requires("joltphysics")` | |
| RmlUi | `add_requires("rmlui")` | |
| fastgltf | `add_requires("fastgltf")` | |
| meshoptimizer | `add_requires("meshoptimizer")` | |
| simdjson | `add_requires("simdjson")` | |
| GameNetworkingSockets | `add_requires("gamenetworkingsockets")` | |
| tracy | `add_requires("tracy")` | |
| SDL3 | `add_requires("libsdl")` | |
| SDL3_image | `add_requires("libsdl_image")` | |
| SDL3_mixer | `add_requires("libsdl_mixer")` | |
| slang | Pre-built binary | Keep as custom build |
| daslang | CPM git repo | Custom build rule needed |
| ImGuizmo | `add_requires("imguizmo")` | Or CPM git |
| ImGui | `add_requires("imgui")` | |
| ImGuiTextSelect | CPM git repo | |
| ImGuiColorTextEdit | CPM git repo | |
| ImNodeFlow | CPM git repo | |
| stb | `add_requires("stb")` | |
| cli11 | `add_requires("cli11")` | |
| cpp-mcp | CPM git repo | |
| GameNetworkingSockets | `add_requires("gamenetworkingsockets")` | |
| uriparser | (daslang dep) | |
| libarchive | system or `add_requires("libarchive")` | |
| curl | system or `add_requires("libcurl")` | |

## File Mapping (CMakeLists.txt → xmake.lua)

| CMake Concept | xmake Equivalent |
|---|---|
| `add_library(wsl SHARED ...)` | `target("wsl") set_kind("shared")` |
| `target_include_directories(wsl PUBLIC ...)` | `add_includedirs(..., {public = true})` |
| `target_compile_definitions(wsl PRIVATE ...)` | `add_defines(...)` |
| `target_compile_options(wsl PRIVATE ...)` | `add_cxxflags(...)` |
| `target_link_libraries(wsl PUBLIC ...)` | `add_packages(...)` + `add_deps(...)` |
| `target_link_directories(wsl PRIVATE ...)` | `add_linkdirs(...)` |
| `add_dependencies(wsl dep)` | `add_deps("dep")` |
| `file(GLOB_RECURSE ...)` | `add_files("**.cpp")` |
| `add_definitions(-DX=...)` | `add_defines("X")` |
| `CPMAddPackage(...)` | `add_requires("package@version")` |
| `set_property(TARGET wsl PROPERTY CXX_VISIBILITY_PRESET ...)` | `set_visibilities(...)` |
| `install(DIRECTORY ...)` | `add_installrules(...)` or `set_installdir(...)` |
| `cmake_dependent_option(...)` | `option(...)` + `if is_config(...)` |

## Risk Assessment

| Risk | Mitigation |
|---|---|
| daslang/slang have complex CMake builds | Use pre-built libs (like now), or write custom xmake rules |
| Some xrepo packages may be outdated | Pin versions; use `add_requires("pkg", {system = false})` for reproducibility |
| Clang 22 stricter IWYU | Use the auto-fixer script (proven, 56 includes added in 2 rounds) |
| Third-party HU macro issues (RmlUi/SDL) | Only if using HUs for third-party — NOT needed for the single-module approach |
| Monolithic rebuild (any header change → full recompile) | Accept for now; partitions can be added later |
| import std on Clang with libstdc++ | Clang 18.1.2+ supports it; verify with Clang 22 |

## Timeline

| Phase | Effort | Deliverable |
|---|---|---|
| A: xmake migration | 1–2 sessions | Same binary via xmake, CMakeLists deleted |
| B: reflect-cpp swap | 1–2 sessions | All cereal usage replaced, round-trip tests green |
| C: module conversion | 1 session | `import wsl.core;` works for consumers |
| D: cleanup | ½ session | Guards removed, docs updated |

## References

- [xmake C++20 Modules](https://xmake.io/examples/cpp/cxx-modules)
- [xmake build.c++.modules policy](https://xmake.io/examples/cpp/cxx-modules)
- [xmake-cxx-modules skill](https://github.com/xmake-io/xmake-skills/blob/master/skills/toolchains/xmake-cxx-modules/SKILL.md)
- [Clang Standard C++ Modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html)
- [Clang Transitioning to Modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html#transitioning-to-modules)
- [Kitware import std in CMake 3.30](https://www.kitware.com/import-std-in-cmake-3-30/)
- [GCC C++20 Modules: Practical Insights](https://chuanqixu9.github.io/c%2B%2B/2025/08/14/C%2B%2B20-Modules.en.html)
