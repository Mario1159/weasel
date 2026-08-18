# `wsl::das` — daScript Integration

Embeds the daScript scripting language into the Weasel engine, allowing gameplay logic (components, systems) to be written in `.das` files.

## Execution Paths

The engine supports Daslang-authored gameplay components and systems across
interpretation and AOT execution. Engine built-in components remain C++/EnTT
types; user runtime C++ components and systems are not supported.

```
                           Daslang source
                           ┌──────┴──────┐
                           │             │
                    interpreted       AOT-generated C++
                     editor/runtime    CMake build
                           │             │
                           └──────┬──────┘
                                  v
                           same EnTT runtime
```

### Path 1: Daslang Interpretation (Editor)

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

### Path 2: Daslang AOT (CMake Build)

Used when building a standalone release binary via CMake.

1. `CMakeLists.txt` calls `weasel_aot_das()` (from `cmake/WeaselDasAOT.cmake`).
2. For each `.das` file, `weasel-cli aot` compiles in-process with the real
   `Module_WeaselApi` loaded. No fake `weasel_api.das` stub is used.
3. Generated `.cpp` files are linked into the final executable.

The generated C++ links against the same engine binding types and functions
used by the interpreted path. The original `.das` sources remain available to
the current runtime loader for program metadata and class-adapter execution.

## Module System

### Module Files

| File | Location | Role |
|------|----------|------|
| `weasel_helpers.das` | `modules/` | `get_component_or` built-in views and the `query` macro |
| `weasel_ecs.das` | `modules/` | Base class `EcsSystem` for system adapters |

### Module Resolution

When a `.das` file does `require weasel_api`, the resolution depends on the execution path:

| Path | Resolution | Generic helpers visible? |
|------|-----------|------------------------|
| daScript Interpretation (Editor) | C++ `Module_WeaselApi` (real bindings) | Yes — via `weasel_helpers.das` |
| daScript AOT (CMake Build) | Real C++ `Module_WeaselApi` in the in-process compiler | Yes — via `weasel_helpers.das` |

The key insight: `weasel_helpers.das` has **no C++ module counterpart**, so there's nothing to shadow it. Both AOT and interpreted modes compile it from the `.das` file directly.

```
require weasel_api      → C++ Module_WeaselApi (interpreted and AOT)
require weasel_helpers  → weasel_helpers.das (always from .das file)
```

### ProjectFsFileAccess

The `ProjectFsFileAccess` class (defined in `das_engine.cpp`) extends `FsFileAccess` to search project source directories when resolving bare `require` names. Without this, daScript's built-in resolution only checks the same directory as the requiring file.

The engine registers the following roots:
- **daslib**: Built-in daScript modules (`math`, `strings`, etc.)
- **engine_modules**: `src/wsl/das/modules/` — for `require weasel_helpers`
- **components**, **systems**: Project-specific directories (added via `addFsRoot()`)

## Key Files

| File | Description |
|------|-------------|
| `das_engine.hpp/cpp` | `das_engine` class — compiles and executes `.das` files, manages contexts, provides SIGSEGV protection |
| `wsl_api_module.hpp/cpp` | `Module_WeaselApi` — registers C++ bindings (entity ops, transforms, scene queries, component access) with daScript |
| `das_ecs_binds.hpp/cpp` | Native ECS module support used by the Daslang runtime |
| `das_system_adapter.hpp/cpp` | `das_system_adapter` — dual-inheritance bridge: `ecs_system` (C++) + `EcsSystemAdapter` (daScript class adapter pattern) |
| `das_system_adapter.hpp/cpp` | Daslang class-adapter system bridge |
| `das_registration.hpp/cpp` | File discovery and registration — compiles `.das` files, extracts struct info, registers components/systems |
| `das_interop.hpp/cpp` | Low-level `addInterop` functions for runtime type introspection (`describe_type`, `type_name`, etc.) |
| `modules/weasel_helpers.das` | Built-in component views and the `query` macro |
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
        query() $(t : Transform&; mr : MouseRotate&) {
            // initialization logic
        }
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

### Component Access and Queries

The `weasel_helpers.das` module provides live built-in views and direct
references for Daslang components inside queries:

```das
var transform = get_component_or(entity, Transform())
transform.position.x = 10.0

query() $(t : Transform&; mr : MouseRotate&) {
    t.position.x += 1.0
    mr.yaw = 5.0
}
```

Built-in view properties point directly into EnTT storage. Daslang component
query references point directly into the scene-local Daslang component pool;
there is no `set_component_t` write-back step.

## SIGSEGV Protection

The `call_void_function_safe()` methods install a signal handler before calling into the VM. Null dereferences, out-of-bounds accesses, and use-after-free crashes are caught and turned into error messages instead of crashing the engine.

The handler uses `sigaltstack` to handle stack overflow crashes, and combines both SIGSEGV (via signal handler) and daScript `panic()` (via `longjmp`) into a single recovery path.

## Architecture Decisions

### Why Per-Program Contexts?

Each compiled program gets its own `Context` because `Context::relocateCode` relocates ALL sim nodes including shared module nodes. If two programs that share modules (via `require`) are simulated into the same Context, the second program's `relocateCode` tries to copy already-relocated shared nodes, causing the `prefix->magic==0xdeadc0de` assertion failure in `SimNode::copyNode`.

### Why Not `require_dynamic_modules`?

The engine intentionally does NOT call `require_dynamic_modules` during initialization. It compiles `.das_module` files and stores them in thread-local `ModuleKarma`. When the worker thread later compiles user `.das` files, the compiled modules may be in an inconsistent state across threads, causing corruption (hash set corruption in `buildAccessFlags`, etc.). Instead, modules like `builtin.das` are compiled on-demand by `compileDaScript` on the worker thread.

### Native module plus helper macro

`weasel_api` is the real native module in both interpreted and in-process AOT
compilation. `weasel_helpers.das` contains only the public `get_component_or`
overloads and the `query` AST macro. It calls the native module's stable pointer
and iteration primitives; it does not maintain a parallel component copy or an
AOT-only stub module.
