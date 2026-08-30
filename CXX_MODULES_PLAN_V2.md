# Plan V2: Untangle + C++20 Modules for the Weasel Engine

> Status: Active plan — **Phase 0 complete (2026-08-30)**
> Supersedes `CXX_MODULES_PLAN.md` (kept as the experimental record: what was
> tried, what failed, and why).

## Objective

Clean engine organization **and** C++20 named modules, with every phase ending
in a green, committable build. The legacy header build stays working until the
final phase.

## Decisions (based on empirical results, 2026-08-30)

| Decision | Rationale |
|---|---|
| **Replace cereal with reflect-cpp** | cereal is the only hard third-party blocker on GCC 16 (anonymous-namespace `version_binding_tag` inside its templates → `exposes TU-local entity`; uncompilable in any GCC module TU). No backward compatibility required, so the swap is a clean cut. reflect-cpp 0.25.0 verified module-clean (0 anonymous namespaces; include + `rfl::json::write` template instantiation pass in a GCC-16 module TU) |
| **Stay on GCC 16** (Clang fallback documented) | All deps validated with GCC; cereal was the only GCC-specific wall. Clang also passes everything, so it remains a fallback if GCC module bugs surface |
| **Single module first, per-namespace modules after untangling** | Per-namespace modules are structurally impossible until the include SCC dissolves (shared headers compiled into ≥2 modules = ODR/merge errors). The untangle work is the path, not a detour |
| **`import std` is off the table** | Every third-party pulls STL textually; mixing textual STL with `import std` is forbidden (verified on GCC 16 and Clang 22; Clang's std module needs libc++ anyway) |
| **rfl formats**: `rfl::json` for scenes/projects, `rfl::msgpack`/`cbor` for binary payloads | Human-readable, debuggable scene files; compact binary where size matters. No format-compat constraints |

### Empirical library matrix (GCC 16.2.1, module-TU global-fragment include)

| Library | Result |
|---|---|
| spdlog, glm, entt, imgui, SDL3, Jolt (`jolt_all.hpp`), daScript (`daScript_all.hpp`) | PASS |
| cereal | **FAIL** (TU-local exposure — hard blocker) |
| reflect-cpp 0.25.0 (baseline + template use) | PASS |

Engine-owned fwd-decls of third-party types (~18 sites, mostly `das/*.hpp`)
are rejected by **both** GCC 16 and Clang 22 when the definition is in the GMF
("declaration follows declaration in the global module") — engine-owned,
fixable by guarding/removal (Phase 2).

## Key insight driving the order

The include SCC (`comp↔gfx↔rsc↔sys↔reg↔event↔das↔math`) has two edge kinds:

1. **Value/ownership edges** (components store `math::vec3f` by value) — legal
   module directions on their own (comp→math is fine).
2. **Serialization edges** (`math/vector.hpp` includes
   `comp/component_meta.hpp` for `serialize`) — these close the cycles.

Moving serialization out of the value-type headers (Phase 1+2) dissolves the
knot: `math` stops including `comp`, the first namespace becomes a leaf, and
untangling proceeds outward.

## Phases

> Status legend: ✅ done · 🔶 in progress · ⬜ not started

### Phase 0 — Housekeeping ✅ (2026-08-30)
- ✅ Committed the guard-pass baseline (single `wsl` module attempt state, all
  inert under the legacy build) — no more 170 uncommitted files.
- ✅ Deleted stray probe files (`-.d`, `std.d`, `std.o`, `a-probe.d`).
- ✅ Renamed CMake target `wsl_header_units` → `wsl_module` and decoupled it
  from `wsl` (opt-in only): the legacy build must never block on the module
  compile while the migration is in progress.
- ✅ Added reflect-cpp 0.25.0 via CPM; linked into `wsl` (cereal still active
  until Phase 1 swap completes).
- ✅ Fixed the `rsc↔gfx` decoupling fallout in `rsc/cubemap_loader.hpp`
  (`make_shared<gfx::cubemap>` moved to the `.cpp` — inline body needed the
  complete type), which broke `src/editor` TUs outside the previously
  validated set.
- ✅ Disabled CMake's default module scanning for all TUs
  (`CMAKE_CXX_SCAN_FOR_MODULES OFF`): legacy header TUs must not compile in
  `-fmodules-ts` mode; the module TU is a custom command and needs no scanning.
- ✅ Wrote this plan; demoted `CXX_MODULES_PLAN.md` to experimental record.

### Phase 1 — Serialization swap: cereal → reflect-cpp ⬜ (1–2 weeks)
- **Feasibility spike first (gate)**: compile a reflect-cpp-based
  `component_meta.hpp` shim inside a module TU with GCC 16 *before* migrating
  the 38 cereal-referencing files. 1-hour check.
- New `wsl/serialize.hpp` — the single home of serialization:
  - `rfl::json::write/read` for scenes/projects; `rfl::msgpack`/`cbor` for
    binary payloads
  - `save_field_if_diff(...)` replacing `serialize_field_if_diff`
    (skip-if-equal-to-default on top of rfl)
  - glm adapters: `vec2/3/4`, `quat`, `mat4`, `color` (replaces
    `rsc/cereal_glm.hpp`)
- Cleanup dividend: plain aggregate components need **zero** serialization
  code (rfl reflects aggregates); only custom-logic components keep a method.
- Migrate `comp/component_meta.hpp` (keep the entt-meta/`register_meta` part —
  editor introspection is unrelated and untouched), then `rsc/*`
  (scene_snapshot_serializer, project_loader, world,
  data_types_serialization), then the remaining cereal files; audit `das/*`
  for serialize usage.
- Drop `CEREAL_CLASS_VERSION` versioning; bump scene/project format markers.
- Remove cereal from CPM.
- **Gate**: round-trip test per component + scene save/load + project load.

### Phase 2 — Break the serialization edges ⬜ (3–5 days)
- Remove `#include "../comp/component_meta.hpp"` from `math/vector.hpp`,
  `math/matrix.hpp`, and the `rsc`/`sys`/`reg`/`event` headers — serialization
  lives in `wsl/serialize.hpp`, included only by TUs that serialize.
- Result: `math` is a leaf; `comp→math` is one-directional and legal.
- Promote `export module wsl.math;` as the first real named module.
- Guard/remove the third-party fwd-decls (~18 sites).

### Phase 3 — Untangle the hub ⬜ (1–2 weeks, incremental)
Per namespace, repeating one pattern (order: `math → event → phys → net →
debug → log → rsc → gfx → sys → reg → das → comp`):
1. Move `runtime_context`'s transitive includes into `runtime_context.cpp`
   (pointer-decoupled already; this is IWYU completion).
2. Shared headers get **one owner module**; consumers use `export import` or
   fwd-declare.
3. Promote `export module wsl.<ns>;` when its cycles are gone.
- After each promotion: full legacy build + module build green.

### Phase 4 — Consumers switch to `import wsl.<ns>;` ⬜ (3–5 days)
- `editor`, `cli`, `mcp-server`, tests switch per-target (a TU must not mix
  `import` + `#include` of the same engine headers).
- Measure compile-time win (BMI caching per namespace).

### Phase 5 — Flip the default + install ⬜
- `WEASEL_ENABLE_MODULES` default ON; keep headers installable for external
  consumers (BMIs are compiler-version-locked — headers remain the
  distribution format for a while).
- Simplify/remove the `WSL_MODULE_BUILD` guard scaffolding where possible.

## Risks

| Risk | Mitigation |
|---|---|
| rfl template instantiation cost for hundreds of components | Explicit instantiation in one serialization `.cpp`; measure early in Phase 1 |
| Deeper reflect-cpp headers hiding a GCC module landmine | Phase 1 feasibility spike is a hard gate |
| Interim single-module = full module rebuild per header change | Passing through, not settling; Phases 2–3 shrink the rebuild surface steadily |
| GCC module compiler bugs | Clang 22 fallback validated (cereal gone = the GCC-only wall is gone) |
| Ninja 1.13.2 dyndep crash (upstream #2592, open) | Irrelevant while using the custom-command path (no dyndep); revisit only for FILE_SET module sources |

## Phase 0 state of the tree

- `WSL_MODULE_BUILD` guards on engine-header third-party/STL includes: inert
  under the legacy build (macro defined only inside module units).
- `src/wsl/wsl.cppm`: single-module attempt (not yet green — see
  `CXX_MODULES_PLAN.md` §11 blockers: cereal [resolved by Phase 1], fwd-decl
  conflicts [Phase 2], include-order cascade [Phase 2/3]).
- `thirdparty/*_all.hpp`, `phys/jolt_all.hpp`, `das/daScript_all.hpp`: GMF
  umbrella includes for the module TU (validated to pass GCC 16 modules).
