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

### Phase 1 — Serialization swap: cereal → reflect-cpp ✅ (2026-08-30)
- ✅ **Feasibility spike PASSED** (hard gate): rfl JSON round-trip of aggregate
  components with glm members + erased-dispatch function pointers + entt
  coexistence, all inside a GCC-16 module TU (`/tmp/opencode/spike`).
  - Design lesson from the spike: instantiating rfl templates inside a module
    *purview* triggers `-Wexpose-global-module-tu-local` (yyjson internal
    linkage). **Resolution: rfl/yyjson live only in `.cpp` implementation
    files; engine headers and module purviews never name them.**
- ✅ **Serialization core implemented & validated** (commit `1f272d2`):
  - `wsl/serialize/types.hpp` — `binary_writer/reader` (length-prefixed POD
    stream) + `json_writer/reader` (yyjson composition, rfl subtree
    grafting/extracting). Implementation-only: engine headers fwd-declare.
  - `wsl/serialize/adapters.hpp` — `rfl::Reflector` for glm (vec2/3/4, quat,
    mat4), engine math (vec2f–vec4f, quatf, mat33f/44f), `entt::entity`.
  - `wsl/serialize/serialize.hpp` — `json_write/json_read` (JSON path) and
    `msgpack_write/msgpack_read` (binary path); both share one reflection.
  - Round-trip test green (binary + JSON) with engine types.
  - CMake: `REFLECTCPP_MSGPACK ON` + link `msgpack-c`.
- ✅ **Component inventory** (aggregate census + member audit, 2026-08-30):
  13/17 world components are aggregates (auto-reflectable once their
  `serialize()` methods are deleted). Non-aggregates needing
  `rfl::Reflector` in `wsl/serialize/component_adapters.hpp` (to be written):
  `transform`, `world_transform`, `camera`, `character_body`,
  `prefab_instance`.
  Special semantics to preserve via Reflectors:
  - `audio` / `model_instance_3d`: resource ids serialize as **paths** via
    `resource_manager::serialization_context` (save: `get_resource_path`,
    load: `register_audio/register_model/register_material`; "None" when
    unset). Runtime-only fields (`playing`, `was_playing`) drop out of the
    ReflType.
  - `character_body`: default ctor already yields null `m_body`; add a
    public `post_load()` (reset applied-cache) honored by a registry
    `has_post_load` concept.
  - Singletons (non-aggregates, field lists): `rendering_manager` (~23
    fields incl. `viewports`), `physics_manager` (5 fields +
    `sanitize_settings()` pre-save), `ui_manager` (1 field),
    `skybox_instance_3d` (cubemap id as raw value + `normalize_resource_id`
    on load). `prefab_instance`/skybox/rendering keep raw id *values* (ids
    are deterministic path hashes), unlike audio/model which use paths.
- ✅ **Registry rework** (`reg/component_registry.*`, `reg/singleton_registry.*`):
  descriptor function-pointer signatures switch from cereal archives to
  opaque `wsl::serialize::json_writer/reader/binary_writer/binary_reader`
  (fwd-declared in headers). The `register_*_component<T>` template bodies
  move from headers into the `.cpp`s with **explicit instantiation over the
  closed component set** (`comp/components.hpp`) so rfl never instantiates in
  a header. New scene JSON shape (free, no back-compat):
  `{ "header": <rfl>, "entities": {alive_count, free_list_count, ids[]},
  "components": { "<type>": {count, entries[{entity,tombstone,data}]} },
  "singletons": { "<name>": <rfl> } }`; binary = POD envelope + msgpack
  component/singletons blobs — **both paths share one entry/loop shape**
  (no entt-snapshot-from-archive needed; `entt/entity/snapshot.hpp` include
  can go away).
- ✅ Migrated `rsc/scene_snapshot_serializer.*` to concrete json/binary
  functions on the new API; migrate `rsc/project_loader.cpp`,
  `rsc/resource_manager.cpp`, `rsc/data_types_serialization.hpp`.
- ✅ Deleted the 17 component `serialize()` methods + all cereal includes +
  `serialize_field_if_diff` + `rsc/cereal_glm.hpp`; then remove cereal from
  CPM/link.
- **Gate PASSED**: all 4 test suites green (149 test cases, 0 failures),
  including the das binary + JSON scene snapshot round-trips. cereal removed
  from CPM and all targets; `weasel_core_tests`, `weasel_cli_tests`,
  `weasel_das_tests`, `weasel_mcp_server_tests` all SUCCESS.

### Phase 2 — Break the serialization edges ✅ (2026-08-30)
- ✅ `math` headers no longer include `comp/component_meta.hpp` (nor entt or
  imgui): `register_meta()`/`custom_inspect()` bodies moved out-of-line into
  `math/math_meta.cpp`; the headers depend only on glm, Jolt and the STL.
  Result: **`math` is a leaf**; `comp→math` is one-directional and legal.
- ✅ **`wsl.math` promoted — the first real named module**:
  - `math/math.cppm`: GMF = glm + Jolt + STL textually;
    `WSL_MODULE_BUILD` defined; purview exports `vector.hpp` + `matrix.hpp`.
  - Built by the `wsl_math_module` custom target (no `-fmodule-mapper`; CMI
    resolves from `gcm.cache`; object `wsl_math.o` carries the module
    initializer and is linked into consumers).
  - Consumer validation: `tests/weasel-core/test_math_module.cpp` does
    `import wsl.math;` (module mode) or the legacy header include (fallback),
    and passes in BOTH modes — 6 assertions incl. glm interop across the
    module boundary. Note: in module mode every include must precede the
    import, and doctest needs `DOCTEST_CONFIG_USE_STD_HEADERS` on GCC 16.
  - Retired the single-module scaffolding: `wsl.cppm`, the 10 stale
    namespace `.cppm` stubs and the `thirdparty/*_all.hpp` umbrellas
    (`jolt_all.hpp`/`daScript_all.hpp` kept for the phys/das promotions).
- 🔁 Resequenced: the third-party fwd-decl guarding (~18 sites) moves to
  Phase 3 — it only matters when a namespace that fwd-declares daScript
  types is actually promoted (the GMF then provides the definitions), and
  doing it per-promotion keeps the legacy build verifiable at each step.

**KEY FINDING (2026-08-30, from the phys attempt): the attachment rule
governs everything.** A class DECLARED in a module purview is attached to
that module; its out-of-line definitions and every TU naming the class must
share that attachment. Consequences, all verified empirically
(`/tmp/opencode/impltest`):
1. Out-of-line definitions migrate to **implementation units**
   (`module;` GMF + `module wsl.<ns>;` — GCC accepts a GMF in impl units and
   the interface is visible without an import; self-import is rejected).
   Definitions written there attach to the module and satisfy module
   vtables.
2. Consumers must link the interface object (vtable + initializer live
   there) and must themselves be module TUs (import) — a *legacy* TU that
   includes the header sees a *global-attached* class and its member calls
   mangle differently → undefined symbols. **Dual-mode survives only for
   all-inline namespaces** (math, event) whose out-of-line members were
   re-inlined; math_meta.cpp was deleted accordingly.
3. Therefore namespaces whose classes are named by other engine headers
   (phys ← comp/singl/physics_manager; likewise rsc/gfx/sys/reg/das/comp)
   cannot be promoted while any legacy TU names them. They move to the
   **Phase-5 full cutover**: promote interface + impl units + migrate every
   consumer TU in the same step, in reverse-dependency order (comp last).
   `phys/phys.cppm` is kept as the worked template for that cutover (its
   GMF/topological-order/impl-unit structure is validated up to the link
   stage).
- ✅ math dual-mode restored: register_meta/custom_inspect re-inlined
  (entt/ImGui/component_meta in the wsl.math GMF — the event pattern);
  the consumer test registers meta and takes custom_inspect's address
  through the module — green in both modes.

### Phase 3 — Untangle the hub 🔶 (in progress, 2026-08-30)
Progress: `math ✅` (reworked all-inline, 2026-08-30), **`event ✅`**
(2026-08-30). Remaining: `phys → net → debug → log → rsc → gfx → sys →
reg → das → comp` — with the attachment-rule caveat below, most of these
move to the Phase-5 full cutover.

Per namespace, repeating one pattern (order: `math → event → phys → net →
debug → log → rsc → gfx → sys → reg → das → comp`):
1. Move `runtime_context`'s transitive includes into `runtime_context.cpp`
   (pointer-decoupled already; this is IWYU completion).
2. Shared headers get **one owner module**; consumers use `export import` or
   fwd-declare.
3. Promote `export module wsl.<ns>;` when its cycles are gone.
- After each promotion: full legacy build + module build green.

**`wsl.event` — PROMOTED (2026-08-30):**
- `event/event.cppm`: GMF = entt + `comp/component_meta.hpp` + STL
  textually; purview = event_hub_fwd/event_hub/message_event/message_bus.
  The comp include is a **GMF material** decision: `component_meta.hpp`
  (post-cereal: entt + STL only) stays attached to the *global module* in
  every module that needs it, avoiding the duplicate-entity problem for the
  future `wsl.comp` promotion. `message_bus.hpp`'s comp include was the one
  unguarded site — now guarded.
- CMake: the module build is generalized into `weasel_add_module(<name>
  <dir>)`; both module objects are linked into the consumer test.
- Consumer test now imports **both** modules in one TU and instantiates the
  hub's template API (`note_listener`/`note_emit` → `comp::stable_type_id`
  from the global module) alongside wsl.math — validated in module AND
  legacy modes. All 4 suites green in legacy mode.

### Phase 4 — Consumers switch to `import wsl.<ns>;` ⬜ (3–5 days)
- `editor`, `cli`, `mcp-server`, tests switch per-target (a TU must not mix
  `import` + `#include` of the same engine headers).
- Measure compile-time win (BMI caching per namespace).

### Phase 5 — Full cutover 🔶 (started 2026-08-30)
**End-state architecture (all mechanics empirically validated):**
- `wsl.math` ✅, `wsl.event` ✅ (inline, dual-mode), `wsl.phys` (interface
  ready), and **`wsl.core`** — ONE module for the whole remaining cluster
  (rsc/gfx/sys/reg/das/comp + top-level glue, 139 headers in a single
  purview): its internal cycles are legal inside one module, and the
  `rsc↔gfx↔sys↔reg↔comp` knot makes per-namespace modules impossible
  (attachment rule, §Phase 3).
- **`wsl_core.cppm` COMPILES GREEN** (GCC 16, attempt ~20): GMF = full
  third-party + STL set + `comp/component_meta.hpp` (global-attached
  utility); imports = Jolt header unit (`import "phys/jolt_all.hpp";`) +
  wsl.math/event/phys; purview = topologically ordered headers
  (`/tmp/opencode/order_purview.py` derives the order from the unguarded
  internal include graph).
- **Jolt is a header unit everywhere** (`phys/jolt_all.hpp`, built with
  `-fmodule-header=user` + ABSOLUTE path): its TU-local vtables cannot
  survive one module importing Jolt while another includes it textually.
  Never mix HU + textual Jolt within one TU; across TUs both are
  global-attached and merge.
- **Implementation units**: `module;` + third-party/STL GMF ONLY (no engine
  headers — the interface provides every engine decl; including your own
  namespace's header creates a global-attached duplicate → ambiguity) +
  `module wsl.core;` + code. Validated on `sys/task_pool.cpp`.
- **simdjson is module-hostile on GCC 16** (TU-local exposure hard errors,
  like cereal) → the `ai` namespace (whose a2a API names simdjson types)
  stays a header-only island inside libwsl for now; rfl/simdjson are out of
  the core interface GMF (the purview signatures are clean; impl units
  include them in their own GMFs).
- **Cross-module fwd-decls are poison**: a fwd-decl of `wsl::event::*` /
  `wsl::phys::*` / `wsl::math::*` in a core header attaches it to the wrong
  module → guarded behind `WSL_MODULE_BUILD` everywhere (imports provide the
  types). `event_hub`'s typed `sys::ecs_system*` seam became `void*`
  (callers cast).
- **Remaining mechanical work (next session):**
  1. Convert the other ~60 wsl `.cpp`s to `wsl.core` impl units (recipe
     above; each: GMF = third-party/STL only, add `module wsl.core;`).
  2. Wire libwsl: `.cpps` with `-fmodules-ts`, `wsl_core.o` +
     `wsl_phys.o` into the target, build-order deps on the CMIs.
  3. Consumer migration: editor/cli/mcp/tests TUs replace engine includes
     with `import wsl.core;` etc. (+ `-fmodules-ts` on those targets).
  4. Flip `WEASEL_ENABLE_MODULES` default ON; retire the legacy path and
     the (now inert) guard scaffolding.
  5. Re-enable `ai` once its simdjson surface is wrapped or simdjson gains
     module support.

## Risks

| Risk | Mitigation |
|---|---|
| rfl template instantiation cost for hundreds of components | Explicit instantiation in one serialization `.cpp`; measure early in Phase 1 |
| Deeper reflect-cpp headers hiding a GCC module landmine | Phase 1 feasibility spike is a hard gate |
| Interim single-module = full module rebuild per header change | Passing through, not settling; Phases 2–3 shrink the rebuild surface steadily |
| GCC module compiler bugs | Clang 22 fallback validated (cereal gone = the GCC-only wall is gone) |
| Ninja 1.13.2 dyndep crash (upstream #2592, open) | Irrelevant while using the custom-command path (no dyndep); revisit only for FILE_SET module sources |

### Phase 1 implementation notes (hard-won details)
- `wsl/serialize/component_adapters.hpp` — `rfl::Reflector` for the 5
  non-aggregate components + audio/model path mapping + singletons
  (rendering_manager, physics_manager, skybox, ui_manager via out-of-line
  `write_state`/`read_state` — the `has_state_io` registry concept) +
  `JPH::BodyID` and `SDL_FColor`.
- rfl needs `-D REFLECT_CPP_C_ARRAYS_OR_INHERITANCE` (PUBLIC compile
  definition on the reflectcpp target) because every component derives from
  the `world_component`/`singleton_component` tag bases.
- Registry save/load: both formats share one entry shape — count + entries
  (tombstone flag, entity u32, payload); JSON payloads are grafted rfl JSON
  subtrees, binary payloads are msgpack blobs. `post_load()` concept restores
  runtime caches (character_body, physics_manager sanitize).
- gotcha fixed twice: `array_size(key)` must be read BEFORE
  `enter_array(key)` (the key lookup only works on object nodes), and
  yyjson tags small integers as UINT — accept both in element reads.

## Phase 0 state of the tree

- `WSL_MODULE_BUILD` guards on engine-header third-party/STL includes: inert
  under the legacy build (macro defined only inside module units).
- `src/wsl/wsl.cppm`: single-module attempt (not yet green — see
  `CXX_MODULES_PLAN.md` §11 blockers: cereal [resolved by Phase 1], fwd-decl
  conflicts [Phase 2], include-order cascade [Phase 2/3]).
- `thirdparty/*_all.hpp`, `phys/jolt_all.hpp`, `das/daScript_all.hpp`: GMF
  umbrella includes for the module TU (validated to pass GCC 16 modules).
