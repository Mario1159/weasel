# Plan: Adopting C++20 Modules in the Weasel Engine

> Status: **SUPERSEDED (2026-08-30)** — see `CXX_MODULES_PLAN_V2.md` for the
> active plan. This document is kept as the experimental record: what was
> tried, what failed (per-namespace modules, header units, single-module on
> GCC with cereal), and why.
> Scope: `src/wsl` (the `wsl` shared library) and its consumers (editor, cli,
> mcp-server, das bindings, tests).
> Goal: Incrementally modularize the engine's namespace-based headers
> (`math`, `comp`, `gfx`, `rsc`, `reg`, `sys`, `phys`, `net`, `log`, `debug`,
> `ai`, `das`, `event`, `editor`, ...) as C++20 modules — without a big-bang
> rewrite and without breaking the existing build.
>
> **Progress (updated 2026-08-28, after a full Ninja `wsl` build):**
> - ✅ Phase 0 complete (toolchain/CMake readiness; `WEASEL_ENABLE_MODULES` on
>   Ninja, opt-in, default OFF).
> - 🔶 Phase 1: the `FILE_SET CXX_MODULES` wiring is **proven end-to-end** — a
>   full `wsl` build compiled **all 1810 non-module steps** and reached the
>   module-interface units. Of the 6 leaf modules:
>   - ✅ **`wsl.log`, `wsl.debug`, `wsl.net`, `wsl.editor`** build green. Their
>     exported declarations reference only `std`/`wsl` types; 3rd-party deps live
>     only inside *non-exported* function bodies (global fragment), so no
>     exported signature leaks a 3rd-party type.
>   - ✅ **`wsl.math` is promoted to a real module** via the Phase 4 header-unit
>     path (see §10c): it `import`s `comp/component_meta.hpp` as a *header unit*
>     (entt/cereal isolated) while glm/Jolt/imgui stay textual. Built by the
>     `wsl_header_units` custom target, not the `CXX_MODULES` file set.
>   - ✅ **`wsl.event` is promoted** (textual `comp` include in its global fragment,
>     `WSL_MODULE_BUILD` *unset* — see §10c) for the same `wsl_header_units` target.
>   - ✅ **`wsl.phys` is promoted** via a second header-unit path (see §10c): Jolt is
>     consumed through a single umbrella header unit `phys/jolt_all.hpp` (Jolt has
>     internal-linkage globals that cannot be a textual export), while the phys
>     headers guard their Jolt/glm/STL includes behind `WSL_MODULE_BUILD` and the
>     module unit provides Jolt + STL in its global fragment. Also built by
>     `wsl_header_units`.
>   - ✅ **`wsl.reg` is promoted** (2026-08-28) via the textual-comp + cluster
>     guard-pass path: `reg.cppm` defines `WSL_MODULE_BUILD`, `import`s
>     `comp/component_meta.hpp` as a header unit, and pulls the heavy engine deps
>     (`rsc/world.hpp`, `rsc/scene.hpp`, `sys/stage_registry.hpp`, `das/das_engine.hpp`)
>     into its global fragment; the 5 cluster headers that include `comp` now guard that
>     include behind `WSL_MODULE_BUILD` so `comp` is no longer compiled textually in the
>     module. Built by `wsl_header_units`. (3rd-party in those headers stays textual in
>     the global fragment — sufficient for `reg`; the gfx/rsc/das namespaces themselves
>     still need header units for their internal-linkage-global 3rd-party, see §9.)
>   - ⏸️ **`wsl.das` deferred** (2026-08-29): attempted and reverted. `das` headers are
>     shared with `wsl.reg` (which `#include`s `das/das_engine.hpp`), so a single
>     `WSL_MODULE_BUILD` guard can't serve both module units, and `das`'s transitive deps
>     (`comp` → `rsc/resource_manager.hpp` → `math/vector.hpp` needing `glm`/`JPH`/`ImGui`) drag
>     in the whole 3rd-party web. Promotion needs the full cluster guard pass + a header unit
>     per internal-linkage-global 3rd-party. `das.cppm` left as a non-building stub, excluded
>     from `wsl_header_units`. See §9.
> - ✅ **Phase 1 end-to-end LINK validated** (2026-08-28): the four active module
>   interface objects compile to BMIs and link into `libwsl.so` under
>   `WEASEL_ENABLE_MODULES=ON` (see §7 DoD). The full *parallel* `ninja` build is
>   still blocked by the Ninja 1.13.2 dyndep assertion crash (toolchain hazard),
>   so the link was proven by invoking the exact `libwsl.so` link command directly
>   rather than via a full `-j 1` dep rebuild.
> - ✅ **§10: header-unit consumption — IMPLEMENTED** (see §10c, 2026-08-28):
>     `wsl.math` is promoted by consuming `comp/component_meta.hpp` as a header unit
>     via a `wsl_header_units` custom pre-build target; the `comp↔math` serialization
>     coupling is broken without a textual include. cereal cannot be a GCC-16 header
>     unit, so it is reached only through the `component_meta.hpp` header unit.
> - 🔶 **Phase 2 (break include cycles) — STARTED (2026-08-28):**
>   the `comp` hub `runtime_context` was **decoupled by value → `std::unique_ptr`**
>   (forward-declared members in `comp/singl/runtime_context.hpp`; allocations moved
>   to the constructor init-list; accessors return `*m_X`). Full `wsl` re-validated
>   green: **all 102 buildable `*.cpp` compile** and **`libwsl.so` links** under
>   `WEASEL_ENABLE_MODULES=ON`. The only fallout was include-what-you-use: 10
>   directly-including `.cpp` files (plus transitive consumers via `app.hpp`,
>   `editor_context.hpp`, `rigid_body.hpp`, `model_instance_3d.hpp`,
>   `character_body.hpp`) needed their missing `#include`s re-added (e.g.
>   `rsc/scene_manager.hpp`, `event/message_bus.hpp`, `reg/*_registry.hpp`,
>   `sys/core_systems.hpp`). No behavioral change; public accessor signatures
>   unchanged.
> - ⬜ Phases 2 (remaining SCC) / 3 / 4 / 5 not started. The rest of the core
>   headers (`gfx`, `rsc`, `sys`, `reg`, `event`, `das`, `math`) are still mutually
>   cyclic and still include `runtime_context` members **by value** (see §11).

---

## 1. Current state (why this needs a plan)

- `src/wsl` is one `SHARED` library compiled from **every** `*.cpp`/`*.c` via
  `file(GLOB_RECURSE ...)` in `src/wsl/CMakeLists.txt:1`.
- Public API is purely **header-based** (`148` `.h`/`.hpp` files), exposed
  through `target_include_directories(wsl PUBLIC ${CMAKE_SOURCE_DIR}/src ...)`.
- Namespaces map 1:1 to directories (`wsl::math` <- `src/wsl/math/`,
  `wsl::comp` <- `src/wsl/comp/`, etc.).
- Toolchain: Clang 22.1.8 and GCC 16.2.1 both available; Ninja is the
  generator. `cmake_minimum_required(VERSION 3.22)` — **too old** for native
  module dependency scanning (needs ≥ 3.28 for `CXX_MODULES` file sets on
  Ninja with Clang/GCC).
- There is **no** existing C++20 module in `src/`. (The `Module_WeaselApi`
  "C++ module" referenced in comments is a *daScript* module, not a C++20
  module — unrelated.)

This is a good migration candidate because the code is already cleanly
namespaced, so each namespace can become one C++ module.

---

## 2. Recommended approach (decision)

**Per-namespace named modules, introduced behind a thin "module veneer" over
the existing headers, adopted incrementally.**

Concretely, each namespace becomes a named module, e.g.:

| Directory        | Module name            | Interface unit file        |
|------------------|------------------------|----------------------------|
| `src/wsl/math`   | `wsl.math`             | `src/wsl/math/math.cppm`   |
| `src/wsl/comp`   | `wsl.comp`             | `src/wsl/comp/comp.cppm`   |
| `src/wsl/gfx`    | `wsl.gfx`              | `src/wsl/gfx/gfx.cppm`     |
| `src/wsl/rsc`    | `wsl.rsc`              | `src/wsl/rsc/rsc.cppm`     |
| `src/wsl/reg`    | `wsl.reg`              | `src/wsl/reg/reg.cppm`     |
| `src/wsl/sys`    | `wsl.sys`              | `src/wsl/sys/sys.cppm`     |
| `src/wsl/phys`   | `wsl.phys`             | `src/wsl/phys/phys.cppm`   |
| `src/wsl/net`    | `wsl.net`              | `src/wsl/net/net.cppm`     |
| `src/wsl/log`    | `wsl.log`              | `src/wsl/log/log.cppm`     |
| `src/wsl/debug`  | `wsl.debug`            | `src/wsl/debug/debug.cppm` |
| `src/wsl/ai`     | `wsl.ai`               | `src/wsl/ai/ai.cppm`       |
| `src/wsl/das`    | `wsl.das`              | `src/wsl/das/das.cppm`     |
| `src/wsl/event`  | `wsl.event`            | `src/wsl/event/event.cppm` |
| `src/wsl/editor` | `wsl.editor`           | `src/wsl/editor/editor.cppm` |

Top-level headers (`app.hpp`, `input.hpp`, `ray.hpp`, `event*.hpp`) fold into a
`wsl` umbrella module that `export import`s the others.

### Why this shape

- **Granularity = one module per namespace.** A single mega-module `wsl`
  defeats incremental compilation; sub-modules per *file* is overkill and
  painful to maintain. Per-namespace matches the existing mental model and
  keeps module-dependency edges == namespace-dependency edges.
- **Veneer, not rewrite (phase 1).** Each `*.cppm` re-exports the existing
  headers through the *global module fragment* so we get module benefits
  (no macro leakage across TU boundaries, faster parses, BMI caching) **without
  moving every declaration into the interface unit on day one**:

  ```cpp
  // src/wsl/math/math.cppm  (Phase 4 header-unit design — see §10c)
  module;
  // 3rd-party headers that leak macros stay TEXTUAL in the global fragment.
  #include <glm/glm.hpp>
  #include <glm/gtc/quaternion.hpp>
  #include <Jolt/Jolt.h>
  #include <imgui.h>
  #include <imgui_internal.h>
  #define WSL_MODULE_BUILD            // tell the engine math headers to skip their
                                      // textual component_meta include (imported as a HU below)
  export module wsl.math;
  // comp/component_meta.hpp as a *header unit*: its global fragment pulls in
  // entt+cereal, but those stay isolated inside the header unit (no macro /
  // declaration clash in this purview). This breaks the comp<->math serialisation
  // coupling without a textual include.
  import "comp/component_meta.hpp";
  export {
      #include "matrix.hpp"
      #include "vector.hpp"
      #include "mikktspace.hpp"
  }
  ```

  Internal macros stay isolated; `import std;` (see §4) replaces `<glm>` later.

### Alternatives considered (record the trade-offs)

- **A) One big `wsl` module.** Simplest CMake, but no incremental rebuild
  benefit and a huge BMI; rejects the namespace structure the user wants to
  preserve. *Not chosen.*
- **B) Header units (`import "vector.hpp";`) only.** Least code change, but
  header units still expose all macros and are slower to compile than named
  modules; they don't give real encapsulation. Useful only as a *temporary*
  fallback during phase 1 (see §5). *Used as transitional bridge, not end
  state.*
- **C) Modules with declarations moved into `.cppm` + implementation
  partitions.** The "purest" module design, but requires rewriting every
  header and is the riskiest. *Deferred to phase 4.*

---

## 3. Phased rollout

> Status legend: ✅ done · 🔶 in progress / scaffolded · ⬜ not started

### Phase 0 — Toolchain & CMake readiness ✅
- ✅ Bumped `cmake_minimum_required(VERSION 3.28)` (was 3.22).
- ✅ Added `WEASEL_ENABLE_MODULES` option (default **OFF**); legacy header
  build is unchanged when off.
- ✅ Added Clang/GCC≥14 guard + a Ninja-generator guard (modules cannot build
  with Unix Makefiles).
- ✅ `Makefile` supports `WEASEL_MODULES=1` → `-G Ninja -DWEASEL_ENABLE_MODULES=ON`.
- Environment check: Clang 22.1.8 + GCC 16.2.1 + Ninja present (CMake 4.4.2).

- Bump `cmake_minimum_required(VERSION 3.28)` (3.30+ recommended for the most
  robust Clang/GCC `CXX_MODULES` support on Ninja).
- Pin the module-capable compiler. **Recommend Clang 22** for now (Ninja
  dynamic-dependency scanning + `ld.lld` are already in use per
  `src/wsl/CMakeLists.txt:169` and the build notes). Add a guard:
  ```cmake
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "C++ modules are validated with Clang; GCC 14+ also works but is secondary.")
  endif()
  ```
- Enable `CXX_MODULE_STD` (CMake ≥ 3.30) so `import std;` works, or set
  `CMAKE_CXX_STANDARD 20` + the compiler flag for the std module.
- Decide module file extension policy: `.cppm` for interface units, `.cpp`
  unchanged for implementation. Update `.gitattributes` / editor configs if
  needed.

### Phase 1 — Umbrella + leaf modules as veneers 🔶
- ✅ Established the **proven pattern** (validated with a standalone
  clang+Ninja project): include 3rd-party headers in the module's *global
  module fragment* so they stay out of the purview (spdlog's include guards
  make `log.hpp`'s own `#include` a no-op there). Importer compiled, linked,
  and ran.
- ✅ Created `src/wsl/log/log.cppm` (`export module wsl.log;`) — the first
  leaf module (only depends on 3rd-party spdlog, no cross-namespace edges).
- ✅ Created `src/wsl/debug/debug.cppm` (`export module wsl.debug;`; glm only)
  and `src/wsl/net/net.cppm` (`export module wsl.net;`; std only).
- ✅ Wired `wsl` target with `target_sources(... FILE_SET CXX_MODULES ...)`,
  gated by `WEASEL_ENABLE_MODULES` (currently: `wsl.log`, `wsl.debug`,
  `wsl.net`).
- ✅ **In-tree validation (2026-08-28):** each module compiles to a BMI with
  clang against the engine's real include paths, and a separate importer TU
  that does `import wsl.<ns>;` resolves and uses the exported engine types
  (spdlog-typed logger, glm-typed interface, std-typed API). This mirrors the
  exact compile steps CMake performs under Ninja, so the module step itself is
  proven — only the full multi-target link/build of slang/Jolt/RmlUi deps
  remains unrun (heavy).
- ✅ `wsl.math` **promoted to a real module via the Phase 4 header-unit path**
  (see §10c): it `import`s `comp/component_meta.hpp` as a *header unit* (entt+cereal
  isolated inside that header unit's global fragment) while glm/Jolt/imgui stay
  textual in its own global fragment. It is **not** in `WSL_MODULE_INTERFACES`
  (a `CXX_MODULES` file set forces `-fmodule-mapper`, which disables header-unit
  auto-resolution); instead it is compiled by the `wsl_header_units` custom target.
  `wsl.event` remains deferred (same mechanism will apply).
- ✅ `wsl.editor` **done & validated in-tree** (generated interfaces compile to a
  BMI cleanly).
- 🔶 **Interconnected namespaces are BLOCKED on proper module-ization (Phase 2/4).**
  A mechanical global-fragment veneer works for *leaf* namespaces but fails for
  `comp`/`gfx`/`rsc`/`sys`/`phys`/`ai`/`das`/`reg` with real module errors
  (all reproduced in-tree):
  - *Double declaration:* `declaration of 'X' in module wsl.Y follows
    declaration in the global module` — some engine headers lack an include
    guard, so including them in the global fragment (as a dependency) **and**
    via the exported header's own `#include` (purview) re-declares the same
    entity. Fix: give every engine header a proper include guard, then switch
    to `export import` edges so each type is declared exactly once (in its own
    module).
  - *Internal linkage:* `declaration of 'collision_layer_count' with internal
    linkage cannot be exported` (`phys/layers.hpp` uses `static`/`constexpr`
    globals). Fix: make such entities externally visible (`inline`/`extern`).
  - *Macro-required headers:* `das_ecs_binds.hpp` fails because RmlUi/SDL macros
    (`RMLUI_SDL_VERSION_MAJOR`) must be defined before the header — those
    headers must stay in the global fragment and the defines provided, or be
    made module-aware.
  These are exactly the Phase 2/4 tasks (break the `comp`↔`gfx` include cycle,
  add `export import` edges, fix header hygiene). The generated `.cppm` drafts
  for these namespaces were removed; the leaf modules above remain wired.
- ⬜ Establish the **module dependency graph** from the real namespace
  includes (derive it with the codebase graph — see §6) *before* adding any
  module with cross-namespace edges. Each module will do
  `export import wsl.<dep>;` for its namespace dependencies.
- ⬜ Run a full `cmake -B build -G Ninja -DWEASEL_ENABLE_MODULES=ON` build to
  confirm the whole `wsl` target links with modules (heavy: rebuilds slang,
  Jolt, RmlUi… — not yet executed).

### Phase 2 — Internal sources import modules ⬜
#### Phase 2 reality check — true include graph & root cause (2026-08-28)
- The real header-level include graph was derived (resolving **relative** includes,
  not just `wsl/...` form). Result: a **single strongly-connected component**
  spanning `{comp, gfx, rsc, sys, reg, event, events.hpp, input.hpp, das, math}`
  — i.e. nearly the whole core is mutually cyclic, not just `comp`↔`gfx`.
- Root cause confirmed in the documented hub `comp/singl/runtime_context.hpp`:
  it **aggregates the subsystems BY VALUE** (`rsc::world m_world;`,
  `gfx::render_window m_window;`, `reg::component_registry m_component_registry;`,
  `event::event_hub m_event_hub;`, `wsl::input::action_map m_app_input_map;`, …
  ~14 value members from `rsc`/`reg`/`event`/`gfx`/`input`). Value members
  **cannot be forward-declared**, so this header can't be decoupled by simple
  forward declarations. The only way to break the cycle is converting these to
  pointers/references (a behavior-preserving but API-touching refactor) across
  the interconnected core — large, and must keep the legacy header build green.
- **Implication:** the achievable module surface is the **leaf-veneer** set only
  (`log`/`debug`/`net`/`editor`, validated; `math`/`event` deferred). The
  interconnected core (`comp`/`gfx`/`rsc`/`sys`/`reg`/`event`/`das`/`math`) should
  be treated like `math`/`event` — kept as **headers** (consumed textually), not
  promoted to named modules — unless/until the pointer-decouple refactor lands.
- **Prerequisites for attempting the full refactor:** (1) a non-crashing build
  loop (upgrade Ninja past 1.13.2 dyndep bug, or accept `-j 1` + ~80 min rebuild);
  (2) per-translation-unit compile validation after each header change.

**Prerequisite—break the header-level include cycles (architectural).**
The engine's header graph is cyclic *at the header level*, so `export import`
edges are illegal until the cycles are removed. Concretely (2026-08-28 scan):
- `comp` is a hub: `comp/singl/runtime_context.hpp` includes `gfx/render_window.hpp`,
  `gfx/scene_renderer.hpp`, `rsc/*`, `reg/*`, `sys/*`, `phys/*`, `event/*`,
  `events.hpp`, `input.hpp` in its **header** (not just `.cpp`).
- Cycles exist `comp`↔`gfx`, `gfx`↔`rsc`, `rsc`↔`sys`, `rsc`↔`reg`, etc.
- Therefore `wsl.comp`/`wsl.gfx`/… cannot use `export import` without first
  replacing these header-level `#include`s with **forward declarations**
  (pointer/reference-only usage in headers; keep the full `#include` in the
  `.cpp`). This is a pervasive, build-validation-dependent refactor across the
  whole `wsl` library and must be done carefully so the (default) legacy
  header build keeps working.
- Until cycles are broken, the **global-fragment veneer** (Phase 1) remains the
  only viable approach, and it is limited to *leaf* namespaces (validated:
  `log`, `debug`, `net`, `math`, `event`, `editor`). Interconnected namespaces
  hit double-declaration / internal-linkage / macro-required errors (see Phase
  1 notes) that only the cycle-breaking + `export import` design resolves.

Once cycles are broken, Phase 2 is:
- Convert `src/wsl/**/*.cpp` to `import wsl.<ns>;` instead of
  `#include "wsl/..."`. Implementation units may use `module wsl.gfx;` and
  `import wsl.comp;`.
- Per namespace: `export import wsl.<dep>;` for each (now acyclic) dependency.
- This is mechanical per namespace, gated by build checks.

### Phase 3 — Consumers switch to `import` ⬜
- `src/editor`, `src/cli`, `src/mcp-server`, `src/wsl/das`, and the
  `tests/*` targets stop `#include`-ing `wsl/*` headers and `import wsl.*`
  instead. They must consume the `wsl` module's BMI — ensure the `wsl` target
  `FILE_SET CXX_MODULES` is `PUBLIC` so dependents resolve `import wsl.math;`.

### Phase 4 — Promote veneers to real module purview (optional, long-term) ⬜
- ⬜ Wire `import std;` (`CXX_MODULE_STD`) — must be set **before** `project()`
  (deferred in Phase 0 on purpose).
- Move declarations out of `.hpp` into the `.cppm` interface units; convert
  `.hpp` to implementation-partition or remove them. Use partitions
  (`export module wsl.gfx:material;`) for large namespaces like `gfx`/`rsc`.
- Resolve the remaining macro-leakage by moving 3rd-party headers
  (`glm`, `EnTT`, `spdlog`, `RmlUi`, `Jolt`, `SDL`) fully into the global
  module fragment / header units and `import`-ing them as needed.

### Phase 5 — Install & packaging ⬜
- Install the compiled BMIs alongside the shared library so external projects
  (the `WeaselTargets.cmake` / `WeaselConfig.cmake` flow) can `import wsl.*`.
  This is more involved than header install — document the BMI layout.
- Update `install(DIRECTORY src/ ... FILES_MATCHING *.hpp)` so it still ships
  headers for the transition window, then narrow to BMIs.

---

## 4. `import std` (standard library module)
- With `CXX_MODULE_STD` the engine can `import std;` inside module units,
  dropping most `<vector>/<string>/<memory>` includes. This is a nice win but
  **opt-in**: enable it after phase 1 is green, because some 3rd-party headers
  (GLM, EnTT) are not module-friendly and must stay in the global fragment.

---

## 5. Transition compatibility & risk controls
- **Keep headers buildable** during phases 1–3: do not delete `.hpp` files; the
  veneer references them. Consumers may mix `#include` and `import` only if the
  same declaration isn't ODR-violated — prefer a hard cutover per namespace.
- **Header-unit fallback:** if a namespace proves hard to modularize early,
  expose it temporarily as a header unit (`import "comp/transform.hpp";`)
  rather than blocking the whole effort.
- **Macro leakage:** GLM, EnTT, and `JPH_DEBUG_RENDERER`/`CPP_RTTI_ENABLED`
  defines are macro-heavy. They must live in the *global module fragment* of
  each module, never in the `export` block, or they will silently change
  meaning across TUs.
- **Windows `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`:** modules + auto-export-all can
  conflict (module symbols vs. dllexport). When `WIN32`, switch to explicit
  `[[gnu::visibility]]` / `__declspec(dllexport)` on the module interface, or
  scope the auto-export to the non-module objects only. Track as a known issue.
- **`WEASEL_ENABLE_MULTIPLAYER` / `WEASEL_ENABLE_RENDERDOC` macros** gate source
  files. Module BMI caching must invalidate when these options flip — CMake
  handles this via compile definitions on the target.
- **Incremental CI:** build with modules on a *separate* CI job first; keep the
  legacy header build working until phase 3 completes, then delete the fallback.

---

## 6. Pre-implementation dependency graph (to derive in Phase 1)
Before writing `export import` edges, extract the actual namespace include graph
with the codebase-memory tools:
```
search_graph(name_pattern=".*", project="weasel", label="...")
query_graph("MATCH (f:File)-[:INCLUDES]->(g:File) WHERE f.file_path STARTS WITH 'src/wsl' RETURN ...")
```
This yields the precise `wsl.X -> wsl.Y` module edges so no cycle is introduced
(C++ modules forbid cyclic `import` graphs).

---

## 7. Definition of done
- [x] `cmake_minimum_required ≥ 3.28`; Clang module build green in CI.
- [x] `WEASEL_ENABLE_MODULES` opt-in flag + Ninja-generator guard in place.
- [x] Proven module pattern (global-fragment 3rd-party include) validated in isolation.
- [x] `wsl.log` module interface unit created + `wsl` target `FILE_SET` wired.
- [x] Full modules **link** confirmed green in-tree (2026-08-28): the four active
  module interface objects (`log`/`debug`/`net`/`editor` `.cppm.o`) compile to
  BMIs and link into `libwsl.so` (`WEASEL_ENABLE_MODULES=ON`). Validated by
  running the exact `libwsl.so` link command extracted from `build/build.ninja`
  directly, because the full parallel `ninja` build is blocked by the Ninja
  1.13.2 dyndep assertion crash (see toolchain hazard) — `-j 1` would rebuild
  all deps from scratch (~80 min) due to the corrupt `.ninja_log`; the direct
  link skips that and proves the module objects resolve/link.
- [ ] Every namespace directory has a `wsl.<ns>` module interface unit.
- [ ] All `src/wsl` implementation files use `import`, no `wsl/*` `#include`.
- [ ] `editor`, `cli`, `mcp-server`, `das`, and `tests` consume `import wsl.*`.
- [ ] BMIs installable; `WeaselConfig.cmake` consumers can `import wsl.math;`.
- [ ] `import std;` adopted where safe; 3rd-party macros confined to global
      fragment.
- [ ] Legacy `.hpp` includes removed (or kept only as documented public API).
- [ ] Docs (`doc/`) updated with the module map and `import` examples.

---

## 8. Open questions for the maintainer
1. **Granularity:** confirm per-namespace modules (recommended) vs. a single
   `wsl` module vs. per-file modules.
2. **Compiler:** OK to require **Clang** for the modules build (GCC secondary)?
3. **`import std`:** adopt the standard library module now or defer to phase 4?
4. **Public ABI:** is the installed `wsl` meant to be consumed by *external*
   projects as modules, or only internally? This decides how much effort goes
   into BMI install/packaging in phase 5.

## 9. Open findings (from implementation, 2026-08-28)

 - **Proven working pattern (validated in-tree AND in a full Ninja build):** the
   *global module fragment* veneer works **only when no exported declaration
   references a 3rd-party type in its signature.** The 3rd-party header is
   included in the module's global fragment (private); `wsl.log`, `wsl.debug`,
   `wsl.net`, `wsl.editor` export only `std`/`wsl` types, so their 3rd-party deps
   live solely inside non-exported function bodies and never leak. This is
   confirmed by a full `wsl` build (all 1810 non-module steps + these 4 module
   interfaces compiled).
- **RESOLVED — EnTT has no include guard (`entt/entt.hpp` starts with a
  namespace decl, no `#pragma once`/guard).** Therefore it cannot be included
  textually in a module *purview* (that re-includes it, emits
  `-Winclude-angled-in-module-purview`, and attaches EnTT's entities to the
  module). The fix is **not** a header unit (CMake+Ninja+Clang cannot
  auto-schedule header units — `clang-scan-deps` does not report header-unit
  imports in its p1689 output, so CMake never schedules the BMI). Instead:
  - Every module interface defines `#define WSL_MODULE_BUILD` in its global
    module fragment and includes `<entt/entt.hpp>` *there* (global fragment).
  - Engine headers that `#include <entt/entt.hpp>` are made module-aware:
    ```cpp
    #if !defined(WSL_MODULE_BUILD)
    #include <entt/entt.hpp>
    #endif
    ```
    so when built inside a module they skip the include (EnTT is already in the
    module's global fragment) and never land in the purview.
  - EnTT is therefore **not exported** by any module; importers that name
    EnTT types just `#include <entt/entt.hpp>` (or any other 3rd-party) in
    their own TU, exactly like the spdlog/glm case. A dual-module importer that
    does `import wsl.math; import wsl.event;` + `#include <entt/entt.hpp>`
    compiles with **no name ambiguity** (validated in-tree). The same
    `WSL_MODULE_BUILD` guard pattern applies to any other guard-less 3rd-party
    header a namespace pulls in.
- **`import std` is NOT required** for this approach (the named-module-wrap
  attempt failed because EnTT's `<vector>`/`<unordered_map>` references need a
  reachable `std` module); keeping 3rd-party/std in the global fragment avoids
  that entirely. `CXX_MODULE_STD` stays deferred to Phase 4.
  - **Cross-namespace cycles:** the header graph is not a DAG (`comp` ↔ `gfx`
    both include each other). Naive `export import` edges would be illegal module
    cycles, so the global-fragment veneer (no `export import`) is used for now;
    phase 2/4 must break these cycles (forward-declaration headers, header units,
    or trimmed includes) before introducing `export import` edges.
  - **§9 hygiene fix (2026-08-28) — `phys/layers.hpp` internal-linkage globals.**
    The `static constexpr` layer globals in `src/wsl/phys/layers.hpp` are
    internal-linkage and therefore cannot be `export`ed from a module; changed to
    `inline constexpr`. This was the concrete blocker for promoting `wsl.phys` (see
    §10c). No engine header was found *missing* an include guard, so that §9 sub-item
    is closed.
  - **`das` macro-gating is a *module-build-config* note, not a header edit.** The
    earlier §9 note that `das_ecs_binds.hpp` "fails because RmlUi/SDL macros must be
    defined before the header" refers to promotion-time configuration: when `wsl.das`
    is eventually made a module, any header needing `RMLUI_SDL_VERSION_MAJOR` etc.
    must have those macros `#define`d in the module's *global fragment* before the
    include (and stay textual there). `das_ecs_binds.hpp` itself only includes
    `<entt/entt.hpp>`/`<string>`, so there is no header edit to make now — the work
    is defining the macros in the future `wsl.das.cppm` global fragment.
  - **Promoting a namespace whose 3rd-party has internal-linkage globals (e.g. Jolt)
    requires that 3rd-party as a *header unit*, not textual.** Textual Jolt fails two
    ways: (a) `export`ing a declaration that references Jolt's internal-linkage globals
    is rejected, and (b) the STL headers Jolt pulls in then conflict with the same STL
    seen in the module purview. Consuming Jolt through a single umbrella header unit
    (`phys/jolt_all.hpp`) isolates its internals and resolves both. The same pattern
    will apply to any other 3rd-party with internal-linkage globals.
  - **Cluster-promotion blocker (2026-08-28): `reg` re-includes `comp` textually via
    `rsc`/`sys`/`das` headers.** `wsl.reg` was the natural next namespace (its exported
    API names only `entt`/`cereal`/`std`/`wsl`), but it transitively includes
    `rsc/world.hpp`, `sys/stage_registry.hpp`, `das/das_engine.hpp`, each of which
    `#include`s `comp/component_meta.hpp` (and Jolt/SDL/DaScript) **textually and
    unguarded**. When `reg.cppm` pulls those into its global fragment, `component_meta.hpp`
    is then compiled twice — once as the imported header unit and once textually →
    `redefinition of 'wsl::comp::...'`. **Resolved for `reg` by guarding the `comp`
    includes in the cluster headers that actually have them** — only 5 headers:
    `rsc/resource_manager.hpp`, `rsc/scene_manager.hpp`, `rsc/scene_snapshot_serializer.hpp`,
    `sys/audio_system.hpp` (comp/audio.hpp), and `sys/system.hpp`. Guarding those behind
    `#if !defined(WSL_MODULE_BUILD)` cut the textual `comp` pull (the heavy 3rd-party in
    those headers stays textual in the global fragment, which is fine for `reg`). This is
    mechanical and safe under the legacy build (the macro is only ever defined inside
    module units). **Remaining for gfx/rsc/das promotion:** those namespaces still need
    header units for their internal-linkage-global 3rd-party (Jolt/SDL/RmlUi/daScript),
    which is the separate header-unit route — their `comp` includes can use the same guard
     pattern, but the 3rd-party cannot be textual exports (see phys/Jolt lesson).
   - **`wsl.das` is NOT promotable with the current shared-header guard trick
     (attempted 2026-08-29, reverted).** `reg` includes `das/das_engine.hpp`, so `das`
     headers are compiled inside *both* `wsl.reg` and a future `wsl.das`. A single
     `WSL_MODULE_BUILD` guard cannot serve both: guarding `das` headers' `daScript`/`entt`/
     `<csetjmp>`/`std` includes for `wsl.das` also strips them from the `wsl.reg` build (which
     defines `WSL_MODULE_BUILD` but only imports the `comp` HU, not daScript/STL HUs) →
     `sigjmp_buf`/daScript types missing. Separately, a `wsl.das` global fragment that pulls
     `comp` (via an `all_comp.hpp` umbrella) transitively reaches `rsc/resource_manager.hpp`
     → `math/vector.hpp`, which needs `glm`/`JPH`/`ImGui` absent from the fragment unless every
     transitive 3rd-party is also present — i.e. the *full* cluster guard pass. **Conclusion:**
     promoting `das` (and `gfx`/`rsc`) needs the complete cluster guard pass (every `comp`/`gfx`/
     `rsc`/`sys` 3rd-party include guarded behind `WSL_MODULE_BUILD` + a header unit per
     internal-linkage-global 3rd-party: Jolt, daScript, SDL, RmlUi, ImGui) plus a per-module set
     of imported HUs matching each namespace's exact deps. This is a distinct, larger effort —
     deferred. `das.cppm` is left as a non-building stub, excluded from `wsl_header_units`.

## 10b. Phase 2 — include-cycle reality-check & progress (2026-08-28)

- **Header-level SCC (resolved include graph, not just `runtime_context`):** the
  true cyclic component is large —
  `{comp, gfx, rsc, sys, reg, event, events.hpp, input.hpp, das, math}`. Nearly
  the whole core is one strongly-connected cluster because god-objects aggregate
  subsystems **by value**:
  - `comp/singl/runtime_context` holds ~14 subsystem members by value
    (`resource_manager`, `component_registry`, `singleton_registry`,
    `system_factory_registry`, `scene_manager`, `message_bus`, `core_systems`,
    `render_context`, `render_window`, `window`, `world`, `input`, `ui_manager`,
    `engine_resources`, …) → **decoupled first** (value → `std::unique_ptr`;
    forward-declared in the header; allocated in the constructor init-list in
    declaration order; accessors `return *m_X`). Compiles + links green.
  - `gfx/render_context`, `gfx/render_window`, `rsc/resource_manager`,
    `rsc/scene_manager`, `sys/core_systems`, `reg/*` still hold cross-namespace
    members by value and include each other transitively — these remain to be
    decoupled in later Phase 2 passes.
- **IWYU lesson:** removing a transitive include from a hub header surfaces
  *every* downstream consumer that relied on it. Fix = add the specific
  `#include` to each affected `.cpp` (and to any hub **header** that includes the
  hub — `app.hpp`, `editor_context.hpp`, `rigid_body.hpp`, `model_instance_3d.hpp`,
  `character_body.hpp` all include `runtime_context.hpp`). Mechanical, behavior-
  preserving.
- **Validation technique (works around Ninja 1.13.2 dyndep crash + stale
  `/usr/local` weasel/RmlUi install):** for each TU, from `build/`, run
  `ninja -t commands src/wsl/CMakeFiles/wsl.dir/<relpath>.cpp.o`, then execute the
  emitted `/usr/bin/c++ …` line directly. Authoritative g++ errors; **ignore
  clangd** diagnostics that stem from `compile_commands.json` carrying
  `-fmodules-ts`/`-fmodule-mapper`/`-fdeps-format=p1689r5` (clangd chokes on those
  GCC flags and reports stale "incomplete type" even after the include is added).
- **Link validation:** invoke the exact `libwsl.so` link command extracted from
  `build/build.ninja` (`build libwsl.so: …`); it links with exit 0.
- **Phase 2 step 2 (2026-08-28): broke the `rsc ↔ gfx` 2-cycle.** `rsc/cubemap_loader.hpp`,
  `rsc/model_loader.hpp`, `rsc/image_loader.hpp` no longer `#include` gfx headers —
  `gfx::cubemap`/`image`/`model_3d`/`render_context` are forward-declared in the
  headers (safe: `entt::resource_loader<T>` and `shared_ptr<T>`/pointers only need
  an incomplete `T`). `model_loader`'s one true by-value member
  (`upload_session::gpu_model`) became a raw pointer, allocated in `begin_upload`
  and released in `finish_upload` (avoided `unique_ptr` to dodge incomplete-type-at-
  destructor + move-suppression). Fallout: 7 `gfx`/`rsc` `.cpp` files needed their
  `gfx/cubemap.hpp` / `gfx/image.hpp` includes re-added (IWYU). Validated: full
  `src/wsl` (102 `.cpp`) compiles + `libwsl.so` links, exit 0.
- **Remaining SCC (still cyclic) — pointer-decouple is EXHAUSTED for these.**
  A full scan of cross-namespace **by-value** members shows the only decouple-able
  god-object/manager cycles were the two already done (`runtime_context`, `rsc↔gfx`).
  Everything left is one of:
  1. **Intrinsic value storage** — `comp` components store `math::vec3f` /
     `math::mat44f` / `math::quatf` by value (a transform *is* a vector; cannot be a
     pointer). This is the `comp↔math` cycle. Not decouple-able.
  2. **Serialization coupling via `comp/component_meta.hpp`** — `math`, `rsc`,
     `sys`, `reg`, `event` headers `#include` it (for `register_meta()` /
     `serialize()` / `type_traits` / `meta_info` / `serialization_context`). These
     are inline template/static methods that *require* `comp` complete, so the
     include cannot move to a `.cpp` (and `math/vector.hpp`'s `serialize` is a
     template method). This drives `comp↔rsc`, `sys→comp`, `reg→comp`, `event→comp`,
     `math→comp`. The `WSL_MODULE_BUILD` guard already severs it under modules, but
     the *legacy* header graph still cycles. **Only breakable via header-unit
     consumption of `component_meta.hpp` (Phase 4), not pointer-decouple.**
  3. **Template/static_assert-locked references** — `rsc/scene.hpp` holds
     `sys::ecs_system &` but also `static_assert(is_base_of_v<sys::ecs_system,T>)`
     and a `vector<unique_ptr<sys::ecs_system>>` member (destructor needs complete
     type). Forward-declaring would break the template/static_assert; would require
     moving the templated `add_system<T>` + destructor to `scene.cpp`. Risky; deferred.
  - Consequently the remaining cycles (`comp↔math`, `comp↔rsc`, `rsc↔reg`,
    `sys→gfx→rsc→sys`) are **acceptable under the global-fragment veneer** (no
    `export import` edges), which was always the plan. They only need resolving if/when
    moving to `export import` — i.e. via header units for `component_meta.hpp` (Phase 4).
  - **Verdict:** Phase 2 pointer-decouple scope is complete (2 high-value decouplings +
    the `runtime_context` god-object). Further cycle-breaking belongs to Phase 4
    (header units), not pointer-decouple. Next productive work is promoting more leaf
    namespaces to modules with the proven veneer, or building the `component_meta.hpp`
    header-unit path.

- **Corrected cycle map & the per-namespace promotion rule (2026-08-28, derived from
  the real include graph).** At the *header* level `comp` is mutually included with
  `math`, `event`, `phys`, `reg`, `gfx`, `rsc`, and `sys` (each of those `#include`s
  `comp/...` AND `comp/...` `#include`s them — e.g. `comp/singl/runtime_context.cpp`
  pulls in `rsc`/`reg`/`event`/`gfx`/`phys`/`sys`). This is a genuine SCC, **but it is
  irrelevant for module promotion because `comp` is a *header hub that is never promoted
  to a module*.** A module-level `import` cycle only exists between two *named modules*;
  since `comp` stays plain headers, no promoted namespace `X` forms a module cycle with
  it. The only real constraint is the **serialization coupling**: `X`'s *exported* API
  must be able to name `comp` types without dragging `entt`/`cereal` into `X`'s purview
  in a conflicting way. That yields a simple per-namespace rule:
  - If `X`'s exported API does **not** name `entt`/`cereal` directly (only uses them
    internally) → consume `comp/component_meta.hpp` as a **header unit** (`import
    "comp/component_meta.hpp";`), like `wsl.math`.
  - If `X`'s exported API **does** name `entt`/`cereal` (e.g. `wsl.event`'s
    `entt::id_type` members + `serialize` template) → consume `comp` **textually** in
    the global fragment with `WSL_MODULE_BUILD` *unset* (so `component_meta.hpp` pulls
    entt/cereal in normally); the header-unit path is infeasible there because cereal
    cannot be a GCC-16 header unit and would clash.
  - The bidirectional header cycles (`comp↔gfx`, `comp↔rsc`, `comp↔reg`, `comp↔sys`,
    `comp↔phys`) therefore need **no pointer-decouple refactor** to promote those
    namespaces — only the per-namespace comp-consumption choice above, plus the §9
    header-hygiene fixes (include guards, `inline`/`extern` for internal-linkage
    globals, macro-provided headers). The earlier "deferred until the big refactor"
    framing overstated the blocker.

## 10c. Phase 4 — header-unit path (IMPLEMENTED & validated end-to-end 2026-08-28)

- **Why this is the real unblocker.** The remaining cycles (`comp↔math`,
  `comp↔rsc/sys/reg/event`) are *serialization coupling* through
  `comp/component_meta.hpp` — those namespaces `#include` it for inline
  `register_meta()`/`serialize()`/`type_traits`/`meta_info`/`serialization_context`.
  A plain textual include drags `comp` (and, transitively, EnTT/cereal) into the
  importer's purview. **A header unit fixes this**: the 3rd-party includes live in
  the header unit's *own* global fragment and are NOT visible to importers, so
  `import "comp/component_meta.hpp";` gives `comp`'s declarations with EnTT/cereal
  isolated. This also sidesteps the EnTT-no-include-guard problem (which only
  bites when EnTT is included in a *module purview*).
- **PoC validated (manual g++, outside CMake):**
  - Build: `g++ -std=c++20 -fmodules-ts -fmodule-header=user <includes> -c
    src/wsl/comp/component_meta.hpp` → BMI at
    `gcm.cache/<abs-path>/comp/component_meta.hpp.gcm` (exit 0). Use
    `-fmodules-ts` (NOT `-fmodules`); `-fmodule-header=user` for a quoted/user
    header; do NOT pass `-fmodule-cache=` (GCC 16 rejects it — BMI lands in the
    default `gcm.cache` next to CWD).
  - Consume: a module interface `import "comp/component_meta.hpp";` in its purview
    compiled with `-fmodules-ts` (no `-fmodule-header`) and the `gcm.cache` in CWD
    → resolves the header unit and uses `wsl::comp::meta_info` / `world_component`
    (exit 0). EnTT not in the module's purview.
- **IMPLEMENTED — `wsl.math` promoted via the header-unit path.** Source
  (`src/wsl/math/math.cppm`) design (see §3 for the full listing):
  - `module;` global fragment: **textual** `#include` of glm, glm quaternion,
    Jolt, imgui, imgui_internal; then `#define WSL_MODULE_BUILD` so the engine math
    headers (`vector.hpp`/`matrix.hpp`) skip their textual `component_meta.hpp`
    include (they rely on the imported header unit instead).
  - `export module wsl.math;` then `import "comp/component_meta.hpp";` (header unit).
  - `export { #include "matrix.hpp"; #include "vector.hpp"; #include "mikktspace.hpp"; }`.
  - EnTT and cereal are NOT imported separately — they are reachable only through
    the `component_meta.hpp` header unit's isolated global fragment (e.g.
    `wsl::comp::register_meta` / `serialize_field_if_diff`). This is what breaks the
    textual `comp↔math` coupling.
- **Build-system integration (`src/wsl/CMakeLists.txt`, gated by
  `WEASEL_ENABLE_MODULES`):** a `wsl_header_units` custom target pre-builds the
  header unit and then compiles `math.cppm`, and `wsl` depends on it
  (`add_dependencies(wsl wsl_header_units)`). Validated: `ninja wsl_header_units`
  builds `gcm.cache/<abs>/comp/component_meta.hpp.gcm` **and**
  `gcm.cache/wsl.math.gcm` (exit 0). Key mechanics learned the hard way:
  1. **CMake (Ninja generator) does NOT split a list generator expression into
     multiple command arguments** (unlike Make). So the `-I` list is written to a
     **response file** (`@file`) via `file(GENERATE ... CONTENT
     "$<JOIN:$<LIST:TRANSFORM,$<LIST:FILTER,$<TARGET_PROPERTY:wsl,INCLUDE_DIRECTORIES>,EXCLUDE,^$>,PREPEND,-I>,\n>")`
     which GCC splits on whitespace. The `LIST:FILTER,^$` step is mandatory: empty
     `-I` entries (some dependency props carry them) add CWD to the search path and
     make GCC compute the *imported* header unit's module key relative to CWD
     (`./home/mario/...`), which then fails to match the absolute key the header
     unit was built with.
  2. **The header unit must be built with an ABSOLUTE `-c` path** (e.g.
     `${CMAKE_CURRENT_SOURCE_DIR}/comp/component_meta.hpp`) so its BMI key is the
     absolute path. An importer that `import "comp/component_meta.hpp";` (or
     `import <entt/entt.hpp>`) also resolves to the absolute key → match. A relative
     `-c` (e.g. `entt/entt.hpp`) produced a mismatching key and failed to import.
  3. **Do NOT put `math.cppm` in `WSL_MODULE_INTERFACES` (the `CXX_MODULES` file
     set).** CMake's file set forces `-fmodule-mapper=<x>.modmap`, which (see point
     2 below) disables `gcm.cache` header-unit auto-resolution, so the `import
     "comp/component_meta.hpp";` inside it cannot resolve. The custom target compiles
     `math.cppm` with `-fmodules-ts` and **no** `-fmodule-mapper`, so it falls back
     to `gcm.cache` and resolves the header unit.
  4. **`-fmodule-mapper` disables header-unit auto-resolution (EMPIRICALLY
     CONFIRMED).** Verified with a dummy mapper: `import` of a pre-built header unit
     fails (`unknown compiled module interface`) when `-fmodule-mapper` is present.
     The four leaf veneers (`wsl.log/.debug/.net/.editor`) in the file set keep
     `-fmodule-mapper`; they must never `import` a header unit.
  5. **Ninja 1.13.2 dyndep crash** still blocks a full parallel build of the
     `CXX_MODULES` file set, but the `wsl_header_units` custom target is independent
     of that dyndep and builds cleanly on its own (`ninja wsl_header_units`).
  6. **cereal cannot be built as a header unit in GCC 16** (`/usr/local/include/cereal/cereal.hpp:
     fatal error: unknown compiled module interface`). The design avoids it entirely
     — cereal is only ever reached through the `component_meta.hpp` header unit.
- **Legacy build unaffected.** All module machinery is behind `WEASEL_ENABLE_MODULES`;
  `WSL_MODULE_BUILD` is defined only inside module interface units, so `vector.hpp`/
  `matrix.hpp` still `#include "../comp/component_meta.hpp"` textually in every
  legacy TU (re-verified: a legacy TU such as `math/mikktspace.cpp` still compiles).
- **`wsl.event` — NOT the header-unit path (2026-08-28).** At the *header* level
  `comp↔event` is actually **bidirectional** (`comp/singl/runtime_context.cpp` includes
  `event/`, and `event_hub.hpp`/`message_bus.hpp` include `comp/`) — but `comp` is a
  *header hub* that is **never promoted to a module**, so there is no *module-level*
  `import` cycle. `wsl.event` therefore consumes `comp` **textually** in its global
  fragment (`src/wsl/event/event.cppm`, built by the same `wsl_header_units` custom
  target to dodge the Ninja dyndep crash). Two hard-won gotchas:
  1. **`WSL_MODULE_BUILD` must NOT be defined in `event.cppm`.** `comp/component_meta.hpp`
     drops its `entt`/`cereal` includes when that macro is set (it then expects them from
     the imported header unit); a textual include *with* the macro leaves `entt`/`cereal`
     undeclared inside comp → compile error. So `wsl.event` includes comp textually with
     the macro unset, letting comp pull entt/cereal in normally.
  2. **`wsl.event` cannot use the header-unit path at all** — its exported API names
     EnTT types (`entt::id_type`, `entt::registry&`, function-pointer typedefs) and a
     cereal `serialize` template, so EnTT/cereal must be in its *purview*. Importing
     `comp/component_meta.hpp` as a header unit (which also carries entt/cereal in its
     isolated fragment) then clashes with the textual entt/cereal in `wsl.event`
     (`conflicting default argument for 'class BindingTag'` from cereal). cereal also
      cannot be a GCC-16 header unit. So the header-unit path is infeasible for
      `wsl.event`; the one-way textual include is the correct design.
  - **`wsl.phys` — promoted via a *second* header-unit path: the heavy 3rd-party
    (Jolt) as an umbrella header unit (2026-08-28).** Unlike `wsl.math` (which
    keeps glm/Jolt/imgui **textual**), `wsl.phys` cannot keep Jolt textual because
    Jolt declares globals with **internal linkage** (e.g. `JPH::cBroadPhaseLayerInvalid`)
    that cannot be `export`ed — so Jolt must be *isolated* inside a header unit.
    Recipe (validated; `ninja wsl_header_units` → `gcm.cache/wsl.phys.gcm`, exit 0):
    1. New `src/wsl/phys/jolt_all.hpp` umbrella header that `#include`s every Jolt
       header the phys API needs (the union of all `<Jolt/...>` includes across the
       phys headers). Built as a *user header unit* (`-fmodule-header=user`) by a
       `hu_jolt_all.stamp` custom command; its BMI lands in `gcm.cache`.
    2. Every phys header guards its `Jolt`, `glm`, and STL includes behind
       `#if !defined(WSL_MODULE_BUILD)` so, inside the module, they are skipped and
       the types are supplied by the imported Jolt HU / the module's global fragment.
       (`src/wsl/phys/layers.hpp` also had to change its `static constexpr` globals to
       `inline constexpr` — internal-linkage globals cannot be exported.)
    3. `src/wsl/phys/phys.cppm`:
       ```
       module;
       #define WSL_MODULE_BUILD
       import "phys/jolt_all.hpp";          // Jolt umbrella header unit
       #include <cstdint> <memory> <mutex> <string> <unordered_set> <vector> <atomic>
       #include <glm/glm.hpp> <glm/gtc/quaternion.hpp>
       export module wsl.phys;
       export { #include "layers.hpp" ... "utils.hpp" }
       ```
    4. **Critical STL-consistency rule (the hard-won lesson):** the STL headers used
       by the phys API must be included **textually in the global fragment** (above),
       NEVER imported as header units and NEVER left to be pulled in by a phys header
       inside the `export {}` block. If a phys header's `#include <memory>` is reached
       in the *purview*, GCC re-declares `std` types already visible from the imported
       `jolt_all.hpp` HU → `redeclaring 'struct std::atomic' in module 'wsl.phys'
       conflicts with import ... of module jolt_all.hpp`. Equally, a `<glm/...>` include
       that lands in the purview is rejected outright
       (`post-module-declaration imports must be contiguous`). So all of Jolt, glm, and
       STL must be present in the global fragment before `export module`.
    5. `wsl.phys.cppm` is built by a `wsl_phys.stamp` custom command (depends on
       `hu_jolt_all.stamp`), **not** added to `WSL_MODULE_INTERFACES` (the `CXX_MODULES`
       file set would force `-fmodule-mapper` and break the `import "phys/jolt_all.hpp";`
       resolution — same reason `wsl.math` is excluded).
    - The legacy (modules-OFF) build is unaffected: `jolt_all.hpp`/`.cppm` are not
      matched by the `*.cpp` GLOB, and the `#if !defined(WSL_MODULE_BUILD)` guards make
      the phys headers include Jolt/glm/STL textually exactly as before.
  - **`wsl.reg` — PROMOTED (2026-08-28).** `reg` was the natural next promotion after
    `phys`. Its exported API names only `entt`/`cereal`/`std`/`wsl` types (no Jolt/SDL
    in signatures), so the recipe is the textual-comp path: `reg.cppm` defines
    `WSL_MODULE_BUILD`, `import`s `comp/component_meta.hpp` as a header unit, pulls
    `rsc/world.hpp`, `rsc/scene.hpp`, `sys/stage_registry.hpp`, `das/das_engine.hpp` into
    its global fragment, and the `reg`/`event` headers guard their heavy includes. The
    blocker was that those transitive headers included `comp/component_meta.hpp` textually
    and unguarded, so `comp` was compiled both as the imported HU and textually →
    `redefinition of 'wsl::comp::...'`. **Resolution:** the 5 cluster headers that
    include `comp` (`rsc/resource_manager.hpp`, `rsc/scene_manager.hpp`,
    `rsc/scene_snapshot_serializer.hpp`, `sys/audio_system.hpp`, `sys/system.hpp`) now
    guard that include behind `#if !defined(WSL_MODULE_BUILD)`. With that, `reg.cppm`'s
    global fragment no longer compiles `comp` textually, so the only `comp` copy is the
    imported header unit — `wsl.reg` compiles and `wsl.reg.gcm` is generated via
    `wsl_header_units`. The heavy 3rd-party in the cluster headers stays textual in the
    global fragment (sufficient for `reg`; gfx/rsc/das themselves still need header units
    for their internal-linkage-global 3rd-party).
- **Cross-module consumer validated (2026-08-28).** A `consumer_test` module that does
  `import wsl.math; import wsl.event;` and uses a safe symbol from each
  (`wsl::math::vec2f`'s `(float,float)` ctor + `x()/y()` accessors, and the
  `std`-only `wsl::event::message_event` concept) **compiles** with `-fmodules-ts` and
  no `-fmodule-mapper`, resolving both BMIs from `gcm.cache`. Crucially, the consumer
  reaches `comp` declarations *through* `wsl.math`'s interface without importing
  `comp/component_meta.hpp` itself or knowing about the header unit — proving the
  header-unit decoupling is transparent to importers. (Using `vec2f`'s `glm`/`ImGui`
  members from a consumer is intentionally avoided; those overloads leak 3rd-party types
  and would require the consumer to see glm/imgui — a known Phase-1 veneer limitation.)
- **Legacy build unaffected.** All module machinery is behind `WEASEL_ENABLE_MODULES`;
  `WSL_MODULE_BUILD` is defined only inside module interface units, so `vector.hpp`/
  `matrix.hpp` still `#include "../comp/component_meta.hpp"` textually in every
  legacy TU (re-verified: a legacy TU such as `math/mikktspace.cpp` still compiles).
- **Next step:** connect a real importer (`import wsl.math;` / `import wsl.event;`) in
  the engine once the Ninja `CXX_MODULES` file-set dyndep crash is fixed, or move that
  importer to the custom-target compile path (no `-fmodule-mapper`) so it resolves both
  `wsl.math.gcm` / `wsl.event.gcm` and the `comp` header unit from `gcm.cache`.

## 10. Blocker discovered by the full build: exported signatures vs. 3rd-party types (2026-08-28)

A full Ninja `wsl` build (with `WEASEL_ENABLE_MODULES=ON`) compiled every
non-module translation unit and then failed on the **module interface units**
`wsl.math` (and, by the same mechanism, `wsl.event`):

- `math.cppm` does `export { #include "vector.hpp"; #include "matrix.hpp"; }`.
  That re-exports **every** declaration in those headers, including members
  whose signatures reference 3rd-party types:
  - `vec2f(const glm::vec2&)` / `operator glm::vec2()` → **glm**
  - `template<class Archive> void serialize(Archive&)` → **cereal**
  - `static void register_meta()` (uses `entt::meta_factory`) → **entt**
  - `bool custom_inspect(const char*)` (uses `ImGui`) → **imgui**
- An exported signature that names a 3rd-party type requires that type to be an
  **importable module (header unit)**. But those deps are only in the *global
  module fragment* (textual/private). Clang therefore tries to re-declare them
  in `wsl.math`'s interface and fails with:
  `conflicting declaration of 'class cereal::BinaryOutputArchive' in module 'wsl.math'`
  and `conflicting declaration of 'class __cxxabiv1::__class_type_info' in module 'wsl.math'`.
- This was **faithfully reproduced** in isolation: a module that only does
  `export module wsl.math; export {}` (no 3rd-party-dependent exports) compiles
  cleanly even with `cereal/entt/glm/Jolt/imgui` in the global fragment; the
  same module with `export { #include "vector.hpp"; #include "matrix.hpp"; }`
  fails identically to the real build. The earlier "in-tree validation" used an
  empty `export {}` and so missed this — **the global-fragment veneer is only
  safe when the exported API does not touch 3rd-party types.**

### Two ways forward (strategic decision required)

- **Option A — Header units for 3rd-party deps (the correct, scalable fix).**
  Consume `glm`, `cereal`, `entt`, `Jolt`, `imgui`, `spdlog`, … as **header
  units** (`import <glm/glm.hpp>;` etc.) in the module interface instead of the
  global fragment, so exported signatures can name them. This also unblocks the
  interconnected namespaces. **Blocked on:** CMake+Ninja+Clang currently does
  not auto-schedule header units — `clang-scan-deps` does not report
  header-unit imports in p1689, so CMake never emits the BMI compile rule (seen
  earlier in `/tmp/opencode/hu`). Needs either a CMake mechanism to force header
  units (e.g. `target_sources(... FILE_SET CXX_MODULES ...)` listing the headers,
  or prebuilt BMIs + `-fmodule-file`) or a clang flag. **Not yet solved.**
- **Option B — Data-only export (bounded, degrades API).**
  Guard the 3rd-party-dependent members (`serialize`, `register_meta`,
  `custom_inspect`, the glm-ctor/operator) behind `#if !defined(WSL_MODULE_BUILD)`
  so the module exports only the plain data structs (`vec2f{ float x,y; }` etc.).
  Keeps the legacy header build fully functional; module importers get the types
  but must `#include` the header (non-module) for serialization/meta. Invasive
  across `math`/`event` (and every interconnected header) and yields a weaker
  module.

**Recommendation:** pursue **Option A** (header units) because it is the only
path that scales to `comp`/`gfx`/`rsc`/`sys`/`phys`/`ai`/`das`/`reg`; Option B
is a stopgap that still leaves those namespaces blocked. Until A or B lands,
`wsl.math`/`wsl.event` stay out of the active module set (`WSL_MODULE_INTERFACES`
in `src/wsl/CMakeLists.txt` should list only the 4 green modules).

#### Progress on Option A (2026-08-28)

- **Source side DONE (correct, does not touch the legacy build):** the engine
  headers that `math`/`event` pull now guard their 3rd-party `#include`s behind
  `#if !defined(WSL_MODULE_BUILD)` so they are skipped inside a module and
  consumed as header units instead: `src/wsl/comp/component_meta.hpp` (entt +
  cereal/archives), `src/wsl/math/vector.hpp` and `src/wsl/math/matrix.hpp`
  (cereal, entt, Jolt, glm, imgui). `src/wsl/math/math.cppm` was rewritten to
  `import <cereal/cereal.hpp>;` / `<glm/glm.hpp>;` / `<entt/entt.hpp>;` /
  `<imgui.h>;` / `<Jolt/Jolt.h>;` (note: `Jolt` is ONE header unit — `<Jolt/Jolt.h>`
  already pulls `Vec3`; do **not** import `Jolt/Math/Vec3.h` separately, it
  re-defines Jolt symbols). `event` needs the same treatment.
- **Deps proven header-unit-CLEAN:** every dep (`cereal`, `entt`, `glm`, `imgui`,
  `Jolt`, and the cereal archives) compiles with `g++ -std=c++20
  -fmodule-header -x c++-header -c` with **no errors** — including `cereal`, whose
  `registerClassVersion` TU-local type only broke when it was in a *textual*
  global fragment, not as a header unit. So Option A is technically feasible.
  (Correction: the module build uses **GCC 16.2.1** (`/usr/bin/c++`), not Clang —
  the earlier plan text listing "Clang 22" as the build compiler was wrong; both
  are installed but CMake selects GCC here. All isolated harness work below used
  GCC, consistent with the real build.)
- **Remaining toolchain steps (the actual hard part):** CMake does **not**
  auto-schedule header units (FILE_SET → "not scheduled for compilation", because
  `clang-scan-deps` omits header-unit imports from p1689). They must be built
  **manually** and wired with `-fmodule-file` / a module mapper. Confirmed
  mechanics so far:
  - Header-unit BMIs land in clang's **module cache**
    (`gcm.cache/<input-path>.gcm`), not at `-o`; the BMI's *name* equals the
    **input file's** path, so a wrapper `#include "dep"` yields a BMI named after
    the wrapper, not after `<dep>`. A module mapper can redirect
    `import <dep>` → that BMI file regardless of its internal name, **but the
    mapper text format that clang accepts for header-unit names still needs to be
    pinned down** (both `"<dep>"` and `<dep>` forms were rejected in testing).
  - Per-dep quirks: `Jolt` must be a single unit; `cereal` built from its real
    path triggered a stray `import` ("unknown compiled module interface") —
    likely a stale `gcm.cache` entry; building it via a wrapper is clean.
  - A full `wsl` link also hits the **Ninja dyndep assertion crash** (see
    toolchain hazard below), so even after header units work, the end-to-end link
    needs `-j 1` or a Ninja upgrade.
- **Next concrete action:** build the dep header units manually (wrapper +
  `-fmodule-header`, cache in `build/`), pin the module-mapper/`-fmodule-file`
  name format, then `import` them from `wsl.math`/`wsl.event` and re-enable those
  two modules in `WSL_MODULE_INTERFACES`. Reusable harness: `/tmp/opencode/hubuild*`.

#### Cereal conflict — RESOLVED: no serialization-library change needed (2026-08-28)

The question "is cereal incompatible with modules?" is answered **no**. Two
sound paths, neither requires swapping cereal:

1. **Defer `wsl.math`/`wsl.event` (Option B, already in effect).** They are
   omitted from `WSL_MODULE_INTERFACES`, so they remain plain headers consumed
   *textually* by other modules' global fragments. Their exported signatures
   never force cereal/glm/entt/imgui to be importable modules. Zero build infra,
   full legacy behaviour. This is the immediate recommendation and is what the
   current build does.
2. **Header units for cereal (Option A) — feasible, with a known GCC recipe.**
   The only reason `<cereal/cereal.hpp>` failed as a header unit is GCC's
   **include-translation**: cereal pulls in `<string>`/`<sstream>`, which GCC
   auto-translates to `import <string>`, and that std *header unit* is not
   prebuilt. The fix is to prebuild the `std` header units (or rely on the `std`
   named module) so the translation resolves. `glm`/`entt`/`imgui`/`Jolt` build
   as header units cleanly on their own.

**Corrected GCC mechanics (supersedes the "stray import" note above):**

- Building a header unit from its **real path** yields a BMI named after that
  path and lands in `gcm.cache/<path>.gcm`; build it with
  `g++ -std=c++20 -fmodule-header -x c++-header -c <realpath>` (no `-o`).
- **Do NOT pass `-fmodule-mapper`** — it *disables* GCC's automatic
  module-name → `gcm.cache` resolution, which then fails every import. Build all
  needed header units into `gcm.cache` and let GCC auto-resolve by name.
- **`-fmodule-file=NAME=PATH` is a D-only flag in GCC** ("valid for D but not for
  C++"); it is the wrong tool on the C++ side. Use `gcm.cache` auto-resolution or
  a proper C++ module mapper instead.
- The `std` module is built with `g++ -std=c++20 -fmodules -x c++ -c
  /usr/include/c++/16/bits/std.cc` → `gcm.cache/std.gcm`; `import std` then
  resolves. For *header-unit* `import <string>` etc., prebuild those std header
  units (the `import <string>` translation is what cereal triggers).
- CMake **does not** auto-schedule header-unit compiles (p1689 omits them), so
  they must be built manually and their BMIs placed where auto-resolution finds
  them (or wired via a mapper). This, not cereal, is the real infra cost of
  Option A.

**Decision:** keep cereal. Promote `math`/`event` to modules only after the
std-header-unit build + CMake scheduling exists; until then they stay deferred
(Option B). No library replacement is warranted.


- **Toolchain hazard (observed 2026-08-28):** a full Ninja build of `wsl` with
  modules intermittently aborts with a Ninja **internal assertion**
  (`build.cc:435: Plan::RefreshDyndepDependents ... Assertion 'edge &&
  !edge->outputs_ready()' failed`, "Subprocess aborted") during CXX *dyndep*
  generation (seen at the `meshoptimizer` dyndep edge, step ~670/1328). This is
  Ninja 1.13.2's known dyndep/parallel race with C++20 modules, **not** a defect
  in the module code — `wsl.log`/`wsl.debug`/`wsl.net` already compiled green in
  an earlier run, and `wsl.editor` compiles green in isolation. Workarounds to
  get a full green link: retry (dyndep outputs cache), build with `-j 1`, or
  upgrade Ninja. Tracked here so the failure is not mistaken for a module bug.

---

## 11. Re-evaluation (2026-08-30): per-namespace-modules + header-units is NOT viable here

The Phase-4 header-unit (HU) strategy was pushed as far as `wsl.math` / `wsl.event`
/ `wsl.phys` / `wsl.reg` / `wsl.sys`. Three further, decisive blockers surfaced that
make the per-namespace-module + HU design infeasible for this codebase:

1. **Third-party MACROS do not cross header-unit boundaries.** A header unit exports
   *declarations*, not macros. `math/vector.hpp` uses `ImMax` (an ImGui macro) and
   other code depends on RmlUi/SDL macros (`RMLUI_SDL_VERSION_MAJOR`, …). When those
   libs are consumed as HUs, the macros vanish → `‘ImMax’ was not declared`. Fixing
   it would require the importing module to `#include` the header **textually** too —
   but a header that is BOTH imported as a HU and included textually re-defines every
   entity → the very "textual-purview vs imported-HU" conflict we already know GCC 16
   rejects. So macro-dependent third-party cannot live in a HU at all.
2. **Shared `wsl`-internal headers would be multiply-defined across modules.**
   `reg` references `comp`, `rsc`, `sys`, `event`, `das` types; `sys` references
   `comp::audio`; etc. If each namespace module *textually* includes the shared
   headers it needs, the same entities get defined in ≥2 modules → duplicate-entity /
   module-merge errors when both are imported. The only correct ownership rule is
   "each header belongs to exactly one module", which is impossible for the tightly
   coupled core (`comp↔gfx↔rsc↔sys↔reg↔event↔das↔math` is one SCC). Module
   `import` cycles between the *same* namespace are also illegal, so `export import`
   edges can't be wired without a full decoupling that the SCC forbids.
3. **Header-unit inventory keeps growing.** Beyond the libs already HU-ified
   (glm/spdlog/fmt/tracy/SDL/RmlUi/ImGui/entt-cereal/component_meta + Jolt + daScript),
   the cluster pulls in `fastgltf`, `curl`, `libarchive`, `nlohmann/json`,
   `meshoptimizer`, `simdjson`, `SDL3_image`, `SDL3_mixer` — each would need its own
   HU, plus a fully-complete `stl_all.hpp` (`<future>`, `<iterator>`,
   `<condition_variable>`, `<mutex>`/`<atomic>` were missing). This is a never-ending
   chase and still doesn't solve (1) or (2).

### Decision — pivot to a SINGLE `wsl` module, fully textual (no HUs)

A single module `wsl` that `export`s every `wsl/*.hpp` (included **textually**, with
`WSL_MODULE_BUILD` left **undefined**) sidesteps all three blockers at once:

- **One translation unit** ⇒ no cross-module duplication (each header compiled once).
- **Textual includes** ⇒ third-party macros (ImGui, RmlUi, SDL) work normally.
- **No HUs** ⇒ no "textual-vs-HU STL" conflict, no Jolt `alloca`-macro clash (that
  only bit when Jolt was a HU *and* textually included elsewhere), no ever-growing
  HU inventory. The earlier Jolt `alloca` failure is moot because Jolt is now just
  one textual include inside the single module TU.

This is essentially "compile the whole engine as one module (a unity build)" — the
legacy `.cpp` files of `libwsl` keep compiling as before, and the module object only
carries the inline/template instantiations from the headers (weak symbols, links
cleanly with the `.cpp` objects). Consumers get a single `import wsl;`.

`das/das_system_adapter.hpp` pulls the generated `modules/weasel_ecs_adapter_gen.inc`,
but that `.inc` contains only `__forceinline` definitions (verified) ⇒ safe to include
in the module (weak symbols, no duplicate-strong-symbol link error).

### Action plan (replaces the per-namespace rollout in §3/§10c)

1. Retire the per-namespace `.cppm` interface units and all HU custom commands
   (`wsl_math`/`wsl_event`/`wsl_phys`/`wsl_reg`/`wsl_sys`, `hu_*`, `wsl_header_units`).
2. Add one `src/wsl/wsl.cppm`:
   ```cpp
   module;
   export module wsl;
   export {
   #include "ai/..."      // every wsl/*.hpp (excluding the HU defs + *.inc)
   ...
   }
   ```
   `WSL_MODULE_BUILD` is intentionally NOT defined, so the guard pass (§10c) re-opens
   the third-party / `comp/component_meta` includes textually. (The guard pass is now
   inert but harmless; it can be reverted later.)
3. Wire `wsl.cppm` via a `CXX_MODULES` file set (or the same dyndep-dodging custom
   command pattern) so `import wsl;` resolves for `editor`/`cli`/`mcp-server`/tests.
4. Build `wsl` and fix any remaining module-incompatible constructs (e.g. a header
   that requires a macro *before* its own include — provide it in `wsl.cppm`'s
   global fragment).
5. **Deferred refinement (not required for a working module):** split the single
   `wsl` module into `wsl:ns` *partitions* (one per namespace) once the SCC is
   decoupled, or adopt `import std`. Partitions keep per-namespace structure without
   the cross-module duplication problem.

**Status after pivot (2026-08-30):** HUs + `wsl.math/event/phys/reg/sys` were built
to prove the HU mechanics; `wsl.sys` then compiled once the `fastgltf`/`std::future`
gaps were closed; `wsl.reg`/`wsl.math`/`wsl.sys` then failed on the macro +
shared-header blockers above. The HU approach is now abandoned in favour of the
single-module design in step 2–3. Code is mid-refactor: the guard pass is in place,
the HU files (`thirdparty/*_all.hpp`, `phys/jolt_all.hpp`, `das/daScript_all.hpp`)
exist but will be dropped.

### Single-module attempt — definitive blockers (2026-08-30, after §11 pivot)

The single `wsl` module (all engine headers textual in the purview; every
third-party + STL in the **global module fragment**; engine headers guard their
`<...>` and third-party `"..."` includes behind `WSL_MODULE_BUILD`) was built and
hit blockers that are *toolchain/library* limits, not ours:

1. **`import std` is incompatible with textual STL.** Verified: a module that does
   `import std;` and textually `#include <glm>` (which pulls `<vector>`) fails with
   `redeclaring 'std::size_t' in module … conflicts with import`. So the standard
   library must be consumed **either** textually **or** as `import std` — never both.
   Because every third-party header (`glm`, `Jolt`, `daScript`, `RmlUi`, `cereal`,
   `spdlog`, …) includes the STL **textually** and cannot be edited, `import std`
   is unusable here. So STL must stay textual — which forces it into the GMF.
2. **STL-in-purview attachment failure.** With STL kept textual, it must be in the
   GMF (not the `export {}` block), else GCC fails to attach libstdc++ internals
   (`std::numeric_limits` / `std::ranges::__max_size_type`) to the module
   (`conflicting declaration … in module 'wsl'`). Solved by guarding all 626
   `<...>` includes in engine headers behind `WSL_MODULE_BUILD` so they resolve to GMF.
3. **`component_meta.hpp` double-exposure.** It is both in the GMF (via
   `thirdparty_all.hpp`) and pulled into the purview by engine `".../component_meta.hpp"`
   includes → redefinition. Fixed by removing it from `thirdparty_all.hpp` (it is an
   engine header; it now lives only in the purview). This class of fix generalizes:
   **every** third-party header pulled into the purview by an engine `"..."` include
   must be guarded, else it redeclares against its GMF copy.
4. **Third-party forward declarations in engine headers conflict with the GMF.**
   e.g. `das/wsl_api_module.hpp` forward-declares `class das::Module;`, which
   conflicts with `das::Module` defined in the GMF daScript copy
   (`redeclaring 'class das::Module' in module 'wsl' conflicts with import`). There
   are many such forward-decls of `das::*`, `entt::*`, etc. throughout the engine.
5. **`cereal` is module-hostile.** `cereal/details/helpers.hpp:401` → `conflicting
   default argument for 'class BindingTag'` (and `is_enum::base_type` conflicts).
   cereal templates do not survive being compiled inside a module translation unit,
   even in the GMF. This was already known (cereal cannot even be a GCC-16 header
   unit).
6. **Cascading type loss.** Once `das`/`cereal`/`log` (spdlog) headers fail, every
   downstream type (`wsl::log`, `entity_match_predicate_t`, `rendering_manager`,
   `ui_manager`, …) becomes "not declared", so the module never reaches green.

**Conclusion (2026-08-30):** C++20 modules with **GCC 16** are not tractable for
this codebase without (a) editing third-party libraries (cereal/daScript/spdlog) —
unacceptable, or (b) a large hand-refactor of every engine header that
forward-declares / textual-includes third-party (effectively rewriting the include
strategy of the whole engine). The plan's original goal — incremental modules that
keep the legacy build green — cannot be met on this toolchain.

**Recommended path forward (decision needed):**
- **(A) Shelve modules for now.** Revert the guard pass + single-module changes,
  keep the legacy header build (which is green). Record the blockers above so a
  future attempt (e.g. on **Clang**, whose module model differs and better tolerates
  textual third-party, or after a `import std` + per-library module shim effort) can
  pick it up. This is the pragmatic default.
- **(B) Try Clang.** The plan always preferred Clang; its `std` module + header-unit
  handling may dodge the GCC-specific attachment/conflict behaviours. Requires
  switching the modules build to Clang 22 and re-running this whole investigation.
- **(C) Full module refactor.** Out of scope: make every third-party a proper module
  (shim header units that `import std` and re-export) and remove all textual
  third-party + forward-decls from engine headers. Very large, risky.

**Status of the tree (2026-08-30, end of session):** mid-refactor. `WSL_MODULE_BUILD`
guards are in place on all engine headers' `<...>` and third-party `"..."` includes;
`src/wsl/wsl.cppm` (single module) and the simplified CMake `wsl_header_units`
target exist; `thirdparty/*_all.hpp`, `phys/jolt_all.hpp`, `das/daScript_all.hpp`
still exist but are now unused. `libwsl` legacy build is unaffected (guards only
activate inside module units). Nothing is committed.
