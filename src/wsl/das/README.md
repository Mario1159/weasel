# `wsl::das` — daScript Integration

Embeds the daScript scripting language into the Weasel engine, allowing gameplay logic (components, systems) to be written in `.das` files.

## Execution Paths

The engine supports two authoring models — C++ and daScript — across three execution paths. All paths converge on the same ECS runtime.

```
                          User Code
                       ┌──────┴──────┐
                       │             │
                    C++ files     .das files
                       │             │
                ┌──────┴──────┐ ┌────┴────┐
                │             │ │         │
            (editor)     (release) (editor) (release/CMake)
          compile to .so   link   interpret  daslang -aot
                │             │ │         │     │
                v             v v         v     v
           dlopen()     executable  runtime  executable
```

### Path 1: C++ Compilation (Editor)

Used when the editor loads a project with `.cpp` source files.

1. `write_generated_translation_unit()` generates `runtime_module.generated.cpp` that `#include`s all user headers/sources.
2. `compile_to_shared_library()` compiles to `runtime_module_cached.so` using flags from `compile_commands.json`.
3. `dlopen(runtime_module_cached.so)` fires static `registration_hook` constructors that populate component/system/singleton registrations.
4. `resolve_user_hooks()` resolves `wsl_on_project_init/update/shutdown` weak symbols.

The shared library links against `-lwsl -lSDL3` and syncs EnTT's meta context via `wsl_get_meta_ctx_handle()`.

### Path 2: daScript Interpretation (Editor)

Used when the editor loads a project with `.das` source files.

1. `das_engine::initialize()` sets up `ProjectFsFileAccess` and registers builtins.
2. `addFsRoot("components", comp_dir)` allows `require mouse_rotate` to find `mouse_rotate.das` in any project directory.
3. For each `.das` file:
   - `compileDaScript(path)` compiles to a `Program`.
   - A new `Context` is created per program (avoids shared-node corruption).
   - `program->simulate(ctx)` generates sim nodes.
   - Functions are merged into `file_fn_lookup[path]`.
   - `get_struct_info()` extracts field metadata for component registration.

**Critical**: Each compiled program gets its own `Context`. Two programs sharing a Context would double-relocate sim nodes, causing `prefix->magic==0xdeadc0de` assertion failures.

### Path 3: daScript AOT (CMake Build)

Used when building a standalone release binary via CMake.

1. `CMakeLists.txt` calls `weasel_aot_das()` (from `cmake/WeaselDasAOT.cmake`).
2. For each `.das` file:
   - A temp directory `_tmp_${Name}/` is created.
   - The input file, `weasel_api.das`, `weasel_helpers.das`, `weasel_ecs.das`, and all component `.das` files are copied in.
   - `daslang -aot input.das output.cpp` compiles to C++.
3. Generated `.cpp` files are linked into the final executable.

The standalone `daslang` binary has no C++ engine. It compiles against the `.das` stub files only. At link time, the AOT-generated code calls into the engine's `wsl_api_module.hpp` functions.

## Module System

### Module Files

| File | Location | Role |
|------|----------|------|
| `weasel_api.das` | `modules/weasel_api/` | AOT stubs — function signatures with `=> 0` or empty bodies for the standalone `daslang -aot` compiler |
| `weasel_helpers.das` | `modules/` | Generic component helpers (`has_component_t`, `get_component_t`, `set_component_t`) — pure daScript, works in all paths |
| `weasel_ecs.das` | `modules/` | Base class `EcsSystem` for system adapters |

### Module Resolution

When a `.das` file does `require weasel_api`, the resolution depends on the execution path:

| Path | Resolution | Generic helpers visible? |
|------|-----------|------------------------|
| C++ Compilation (Editor) | C++ `Module_WeaselApi` shadows the file | No — the `.das` file is never compiled |
| C++ Compilation (Release) | N/A — C++ doesn't use `require` | N/A |
| daScript Interpretation (Editor) | C++ `Module_WeaselApi` (real bindings) | Yes — via `weasel_helpers.das` |
| daScript AOT (CMake Build) | Finds `weasel_api.das` as a file | Yes — via `weasel_helpers.das` |

The key insight: `weasel_helpers.das` has **no C++ module counterpart**, so there's nothing to shadow it. Both AOT and interpreted modes compile it from the `.das` file directly.

```
require weasel_api      → C++ Module_WeaselApi (interpreted) / weasel_api.das (AOT)
require weasel_helpers  → weasel_helpers.das (always from .das file)
```

### ProjectFsFileAccess

The `ProjectFsFileAccess` class (defined in `das_engine.cpp`) extends `FsFileAccess` to search project source directories when resolving bare `require` names. Without this, daScript's built-in resolution only checks the same directory as the requiring file.

The engine registers the following roots:
- **daslib**: Built-in daScript modules (`math`, `strings`, etc.)
- **engine_modules**: `src/wsl/das/modules/` — for `require weasel_helpers`
- **components**, **systems**, **src**: Project-specific directories (added via `addFsRoot()`)

## Key Files

| File | Description |
|------|-------------|
| `das_engine.hpp/cpp` | `das_engine` class — compiles and executes `.das` files, manages contexts, provides SIGSEGV protection |
| `wsl_api_module.hpp/cpp` | `Module_WeaselApi` — registers C++ bindings (entity ops, transforms, scene queries, component access) with daScript |
| `das_ecs_binds.hpp/cpp` | `Module_Ecs` — registers engine component types (transform, camera, etc.) as daScript types |
| `das_system_adapter.hpp/cpp` | `das_system_adapter` — dual-inheritance bridge: `ecs_system` (C++) + `EcsSystemAdapter` (daScript class adapter pattern) |
| `das_system.hpp/cpp` | `das_system` — earlier system implementation using module-level wrapper functions |
| `das_registration.hpp/cpp` | File discovery and registration — compiles `.das` files, extracts struct info, registers components/systems |
| `das_interop.hpp/cpp` | Low-level `addInterop` functions for runtime type introspection (`describe_type`, `type_name`, etc.) |
| `modules/weasel_api/weasel_api.das` | AOT stub declarations for the standalone `daslang` compiler |
| `modules/weasel_helpers.das` | Generic component helpers (type-safe `has/get/set_component_t`) |
| `modules/weasel_ecs.das` | `EcsSystem` base class for system adapters |

## Writing a daScript System

Create a `.das` file in your project's `systems/` directory:

```das
require weasel_api
require weasel_ecs
require weasel_helpers
require math
require mouse_rotate

class System : EcsSystem {
    m_accumulated_dx : float = 0.0

    def override on_init() : void {
        query_entities( $(var t : TransformAccessor; var mr : MouseRotate) {
            // initialization logic
        } )
    }

    def override on_update(dt : float) : void {
        // per-frame update
    }

    def override on_event() : void {
        var kind = get_event_kind()
        if (kind == EVENT_MOUSE_MOTION) {
            m_accumulated_dx += get_event_mouse_dx()
        }
    }

    def override on_inactive() : void {
        // cleanup
    }
}
```

### Lifecycle Methods

| Method | When called |
|--------|------------|
| `on_init()` | Once after the system is loaded and all entities are available |
| `on_update(dt)` | Every frame |
| `on_event()` | When an SDL event occurs (before `on_update`) |
| `on_inactive()` | When the system is being shut down or scene is changing |

### Generic Component Helpers

The `weasel_helpers.das` module provides type-safe generic functions:

```das
// Check if an entity has a component
if (has_component_t(entity, MouseRotate())) { ... }

// Get a typed copy of a component
var mr = get_component_t(entity, MouseRotate())
mr.yaw += 1.0
set_component_t(entity, mr)
```

These work with any registered component type (C++ native, C++ project, or daScript-defined).

## SIGSEGV Protection

The `call_void_function_safe()` methods install a signal handler before calling into the VM. Null dereferences, out-of-bounds accesses, and use-after-free crashes are caught and turned into error messages instead of crashing the engine.

The handler uses `sigaltstack` to handle stack overflow crashes, and combines both SIGSEGV (via signal handler) and daScript `panic()` (via `longjmp`) into a single recovery path.

## Architecture Decisions

### Why Per-Program Contexts?

Each compiled program gets its own `Context` because `Context::relocateCode` relocates ALL sim nodes including shared module nodes. If two programs that share modules (via `require`) are simulated into the same Context, the second program's `relocateCode` tries to copy already-relocated shared nodes, causing the `prefix->magic==0xdeadc0de` assertion failure in `SimNode::copyNode`.

### Why Not `require_dynamic_modules`?

The engine intentionally does NOT call `require_dynamic_modules` during initialization. It compiles `.das_module` files and stores them in thread-local `ModuleKarma`. When the worker thread later compiles user `.das` files, the compiled modules may be in an inconsistent state across threads, causing corruption (hash set corruption in `buildAccessFlags`, etc.). Instead, modules like `builtin.das` are compiled on-demand by `compileDaScript` on the worker thread.

### Two-File Strategy (weasel_api.das vs weasel_helpers.das)

The generic helpers (`has_component_t`, etc.) are pure daScript functions that call C++ bindings (`_get_component_type_id_by_name`, etc.). They were originally duplicated in each user system file for AOT compatibility.

The two-file strategy separates concerns:
- `weasel_api.das`: AOT stubs only (no generics) — for the standalone `daslang -aot` compiler
- `weasel_helpers.das`: Generic helpers — pure daScript, no C++ counterpart, works in both AOT and interpreted modes

This avoids the "C++ module shadows the `.das` file" problem: since `weasel_helpers.das` has no C++ module, there's nothing to shadow it.
