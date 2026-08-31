# Plan: Parallel System Execution with Dependency Fences

## Status
Investigation complete and plan revised per design review. **No code has been
changed.** This document is a design plan only. Implementation is broken into
phases marked TODO.

## 1. Current Architecture (as investigated)

### System storage & update path
- Systems live as `std::vector<std::unique_ptr<sys::ecs_system>> scene::systems`
  (`src/wsl/rsc/scene.cpp:67`).
- The single update entry point in the running app is
  `app.cpp:332` → `core_systems::update(dt)`
  (`src/wsl/sys/core_systems.cpp:248`).
- `core_systems::update` runs systems in **strict sequential order**, in two
  loops plus two more loops for editor updates:
  1. `to_vec()` core systems → `sys->update()` (core_systems.cpp:266)
  2. `scene->systems` → `sys->update()` (core_systems.cpp:282)
  3. `to_vec()` core systems → `sys->editor_update()` (core_systems.cpp:291)
  4. `scene->systems` → `sys->editor_update()` (core_systems.cpp:300)
- `scene::update` (`scene.cpp:134`) is a *valid standalone* entry point (used by
  the MCP server / tests to step a scene). **`scene_manager::update`
  (`scene_manager.cpp:227`) is dead code** — it has no live caller in the frame
  path; only `core_systems::update` drives the running loop. See §8 (cleanup).
- Each `ecs_system::update` (`system.hpp:184`) only runs `on_update` when
  `m_active`. `editor_update` runs when `m_editor_active`.

### What actually does work in `on_update` today
Per a full-tree grep, only four core systems override `on_update`:
`transform_system`, `physics_system`, `audio_system`, `render_ui_system`.
`shadow_system`, `lighting_system`, `render_3d_system`, `skybox_system`,
`render_2d_system` have **empty** `on_update` — all their per-frame CPU cost
lives in the render pass (`render_build_draw_data` / `render_record_draw_cmd`),
which is invoked separately in `core_systems::render`
(`core_systems.cpp:396`).

**Implication (review concern #1):** the dominant parallelizable work in the
`update` pass is (a) the user-authored scene / Daslang `on_update` systems and
(b) the render-build phase. The plan therefore targets **both**: Das/ scene
`update` systems are the primary win, and `render_build_draw_data` is folded
into the same scheduler **when it is safe** (see §5 / §6).

### Dependency declaration today (NOT used for scheduling)
Two parallel, partly redundant mechanisms exist:

1. **Type-id based (registry)** — `declare_system_dependency<A,B>()`
   (`system_factory_registry.hpp:365`) appends `B`'s `entt::id_type` to
   `system_descriptor::dependencies`. Queried via
   `system_factory_registry::get_system_dependencies(type_id)`
   (`system_factory_registry.cpp:189`). **Runtime systems register almost
   nothing here** — the only live call is in `mcp-server/namespace_info.cpp:683`
   (`ShadowSystem → LightingSystem`), which is compiled into the MCP server
   binary, not the editor/runtime. So at runtime this graph is effectively
   empty.
2. **String based (instance)** — `set_relationships(...)` /
   `set_dependencies(...)` (`system.hpp:282-299`) store `std::vector<std::string>`
   display names on the instance. Declared by the core systems
   (`render_3d → {Lighting System, Transform System}`, `shadow → {Transform
   System}`, `lighting → {Shadow System, Transform System}`, `physics →
   {Transform System}`; see each `*_system.hpp` constructor). Das systems have
   **no** way to declare these today (review concern #3).

Naming is inconsistent: the type is registered as `"Transform"` but the instance
is named `"Transform System"` and that is what `physics_system` references. The
builder resolves dependency names to **type ids** robustly via the fallback in
§4.2.

### Important ordering fact (review concern #2)
Dependencies are **ignored for execution order today**. `rebuild_system_cache`
(`core_systems.cpp:320`) fixes the order as
`audio, physics, transform, shadow, lighting, skybox, render_3d, render_2d,
render_ui`. So **`physics` currently runs *before* `transform`**, yet
`physics_system` reads `world_transform` that `transform_system` writes
(`physics_system.cpp:66,733`). Honoring the declared `physics → transform`
dependency will **reorder** execution: physics will run *after* transform for
the first time. This is almost certainly a correctness **fix** (removes a
one-frame-stale read) but it is a deliberate behavior change and must be
regression-tested (see §10).

### Threading infrastructure today
- **No core thread pool exists.** `wsl` only spawns `std::thread` for
  AI/networking clients and async reloads.
- `editor::job_manager` (`src/editor/job_manager.cpp`) exists but is
  editor-only, uses fire-and-forget `std::future` jobs, and is **not** a
  parallel-for / barrier primitive. It cannot be reused for frame scheduling.
- Tracy zones already wrap every system (`ZoneScopedN("sys::update")`), so
  per-system timing is observable once parallelized.

## 2. Goal
Run independent systems concurrently and insert **fences** (synchronization
barriers) between dependency levels, derived automatically from (a) dependencies
declared at registration / construction time and (b) **component read/write
coherence derived from query-level access (CADS, see §4.3 / §4.4)**. Preserve the
correctness contract that a
system never starts until every system it depends on has finished its current
pass.

Two passes are parallelized:
- `update` + `editor_update` over the merged active core + scene system list
  (the primary win is user-authored scene / Das `on_update` systems).
- `render_build_draw_data` over the same list, **gated as safe** by the component-access (CADS)
  edges (it is read-only over components and writes into
  per-system draw buffers, so it is a good parallelization candidate).

### Why system granularity (not per-query)
The scheduling/execution unit is the **whole system pass** (`on_update` /
`editor_update` / `render_build_draw_data`), enqueued as one task per system.
The query-level read/write info (CADS, §4.3) is used only to *compute the
dependency graph* (which systems may share a level), never to schedule
individual queries. Per-query scheduling was considered and rejected:
- **Task overhead dominates** — ECS iterations are usually cheap; one task per
  iteration costs more in enqueue/steal/fence than the work it parallelizes.
- **Intra-system shared state** — a system's iterations often share caches,
  scratch buffers, and member state, and rely on internal ordering (e.g.
  `transform_system`'s recursive parent→child walk). Splitting them across
  threads would require per-query thread-safety and internal fences the systems
  were not written for, and would fight the ECS system abstraction.
- **Larger race surface** — entt structural changes (`emplace`/`erase`) are not
  thread-safe; per-query scheduling multiplies the chance of an accidental
  cross-query mutation race.
- **The wins are system-shaped** — in this engine the parallelizable cost is
  render-build plus user/Das `on_update` systems, which are already whole-system
  units. More parallelism is better obtained by *adding more systems* (which
  system scheduling parallelizes for free) than by subdividing one system's
  queries.

The one case where finer parallelism helps — a single large, embarrassingly
parallel iteration over many entities — is addressed separately by an *opt-in
internal parallel-for* over the entity range inside that iteration (chunked
across `task_pool` workers), keeping scheduling at system granularity.

## 3. New Component: `wsl::sys::task_pool` (thread pool)
New files `src/wsl/sys/task_pool.hpp` / `.cpp`.

- Fixed-size pool sized from a new engine setting
  `max_system_worker_threads` (default `std::thread::hardware_concurrency()`,
  clamped to ≥1).
- API:
  - `void enqueue(std::function<void()> task)` — non-blocking.
  - `template<class F> void dispatch(std::span<F> tasks)` — enqueue many, then
    **fence** (block) until all complete. This fence is the synchronization
    primitive the plan relies on.
  - `size_t worker_count() const`.
- Implementation: `std::vector<std::thread>` + a MPMC task queue with a
  `std::mutex`/`std::condition_variable`. Per-level fence implemented with a
  `std::atomic<size_t>` outstanding counter + a `std::condition_variable`
  (fallback for platforms without `std::barrier`). Prefer C++20 `std::barrier`
  when available.
- The pool is created once (owned by `core_systems`) and reused every frame —
  **never** create threads per frame. The main thread blocks at each `dispatch`
  fence while workers run; per-system Tracy zones fire on worker threads.

## 4. Dependency Graph Builder: `wsl::sys::system_dependency_graph`
New file `src/wsl/sys/system_dependency_graph.hpp` (header-only is fine).

### 4.1 Inputs
A list of `ecs_system*` for the pass (active core + active scene systems merged
for the `update` pass; editor-active ones for the `editor_update` pass; for the
`render_build` pass, active+editor-active systems that override
`render_build_draw_data`).

### 4.2 Edge resolution (unify the two mechanisms + Das)
For each instance `S`:
1. From `S->get_type_id()` look up
   `system_factory_registry::get_system_dependencies(type_id)` → set of
   dependency **type ids** (type-id mechanism; currently near-empty at runtime,
   but supported for the future / MCP).
2. From `S->get_dependencies()` (string names) resolve each name to a type id
   via `system_factory_registry::find_system(name)` (matches display name) and,
   as fallback, by comparing against each candidate instance's `get_name()`
   (handles `"Transform System"` vs registered `"Transform"`).
3. **Das systems (review concern #3):** Das-authored systems declare dependencies
   through a new script API (§4.3) that stores into the same `m_dependencies`
   vector, so they are resolved identically to step 2.
4. Map each resolved dependency type id to the concrete instance(s) present in
   the input set. If multiple instances share a type id, depend on **all** of
   them.
5. A dependency whose instance is **absent** from the pass is treated as
   externally satisfied; emit a debug log once.

### 4.3 Component access captured per query/iteration (CADS — primary edge source)
Instead of relying on hand-written system→system dependencies, the ordering
graph is derived from **what components each system actually reads and writes**,
  declared at the query/iteration site. This is the primary source
of edges; explicitly declared system dependencies (§4.2) become an optional
overlay used only for non-component logical orderings (e.g. "render_3d needs
lighting to have filled a renderer singleton").

- **C++:** extend `register_iteration` to carry a read/write split, e.g.
  `register_iteration<reads<comp::transform>, writes<comp::world_transform>>(
   hub, name, fn)`. The existing single-pack form is migrated to mark intent; a
  system that mutates a component via `get`/`view` must list it under `writes`.
  `transform_system` writes `world_transform`; `render_2d` reads it, etc.
- **Daslang:** the `query()` macro already binds each component `T&` through
  engine accessors (`weasel_helpers.das:44` → `get_world_transform_accessor`,
  etc.). Extend the macro to record each bound component as **read** (default) or
  **write** via an explicit annotation, e.g.
  `query() $(t : WorldTransform& : write)`. All Das component access funnels
  through those engine functions, so capture is complete with no extra author
  burden beyond marking writes.
- **Runtime fallback (optional calibration):** for legacy systems that do raw
  `registry.get`/`view` without the annotated helpers, a one-time calibration
  pass can wrap the registry and record `(component, mode)` per system, then
  freeze the signature. Un-annotated access defaults to **conservative write**
  so it stays safe.

Das systems therefore need no separate dependency API for the common case; the
optional `set_system_dependencies` script function (mirroring
`add_script_event_source` / `current()`) remains available only for logical,
non-component orderings.

### 4.4 Conflict graph rules (incl. WAW resolution)
From each system's read set `R` and write set `W`, build precedence edges:

| Access pair on component C | Edge | Meaning |
|---|---|---|
| A writes, B reads | `A → B` | B observes A's write (no stale read) |
| A writes, B writes (WAW) | `A → B` (serialized) | see WAW resolution below |
| A reads, B reads | — | no edge, run concurrently |

Read/read of the same component is always safe to parallelize. Only a writer
forces serialization of readers (and of other writers).

**Structural mutations** (`create`/`destroy` entity, `add`/`remove`/`emplace`
component) are treated as a write to a special `__structural__` token that
conflicts with *every* other system, conservatively serializing the mutating
system. This catches registry-wide changes that the per-component sets would
otherwise miss.

**WAW (multiple writers of the same component) resolution.** Data flow gives no
direction between two writers — both are producers; neither consumes the other
within the frame — so the graph knows they must be serialized but not *who runs
last*, and **last writer wins** the component value for the rest of the frame.
Resolve in priority order:
1. **Explicit author order** — a declared relationship (§4.2) or a per-system
   `write_priority` between the writers. The only way to express real intent
   (e.g. "physics is authoritative for `world_transform`").
 2. **Stage position (§4.5)** — if the writers are in different stages, the stage
    that appears *earlier* in the stage vector is ordered before the one that
    appears *later* (the later stage *wins*). This inserts a precedence edge
    derived from the stage order.
3. **Same stage → deterministic tie-break + warning** — if no declared order and
   both writers share a stage, impose a stable order by `get_type_id()`
   (reproducible; satisfies `deterministic_system_order`) and **log a warning**
   naming the conflicting writers ("WAW on `world_transform`: physics_system &
   transform_system — last in stable order wins"). Under a
   `strict_system_ordering` flag this becomes an **error**, forcing the author
   to decide. `write_priority` is an optional override for same-stage ties.

Real case in this engine: `transform_system` and `physics_system` both write
`world_transform`. Under CADS this is surfaced as a WAW the author must resolve,
rather than being hidden in today's accidental cache order (where transform runs
after physics and silently wins). A finer future refinement — overlapping
writers that touch **disjoint entity subsets** — is out of scope for v1;
system-level conservatively serializes them.

This CADS mechanism (query-level R/W capture → conflict graph, incl. WAW) is a
v1 feature and the primary safety guarantee now that implicit tier edges are
removed.

### 4.5 Stages (coarse ordering + WAW resolution)
Every system belongs to exactly one **stage** — a named bucket that gives a
coarse pipeline position and, crucially, a deterministic tie-break for WAW
conflicts. Stages live in a single global, **ordered `std::vector<stage>`**
(`stage_registry`); a stage's order is its **position in the vector**, *not* an
author-supplied integer. Systems reference stages by **name**. Built-in default
stages are registered at startup in pipeline order; users append via
`register_stage(name)` or insert between existing stages via
`insert_stage(name, before_name)` / `register_stage_at(name, position)`.

**Runtime semantics (soft by default):** stages are *not* hard fences. Systems
across stages still run concurrently whenever their component access does not
conflict (CADS levels). A stage only forces ordering where it is needed:
- For a WAW on component C between writers in different stages, the stage that
  appears *earlier* in the vector is ordered before the one that appears *later*
  (the later stage *wins* the component for the rest of the frame). This inserts a
  precedence edge derived from the stage order, serializing just that pair.
- All other cross-stage systems overlap freely.

This satisfies "stages may overlap at runtime, but determine WAW order." An
optional `stage_fences_enabled` flag promotes every stage boundary to a hard
fence (each stage fully completes before the next begins) — useful for
reproducibility/debugging, at the cost of parallelism.

**Default stages** (in pipeline / vector order; position shown for reference
only, derived from order):

| Position | Name | Typical systems |
|---|---|---|
| 0 | `input` | input polling, message-bus drain, pre-frame setup |
| 1 | `pre_physics` | apply control intent (forces/velocities, character input) |
| 2 | `transform` | hierarchy → `world_transform` (parents, cameras, render sources) |
| 3 | `physics` | Jolt integration; authoritative `world_transform` for bodies |
| 4 | `post_physics` | constraints, character controllers, kinematic follow |
| 5 | `animation` | skeletal/blend animation writes transforms |
| 6 | `logic` | gameplay/user systems — **default stage** for unassigned systems |
| 7 | `camera` | camera follow / view updates |
| 8 | `render_build` | `render_build_draw_data` (reads components, writes draw buffers) |

Built-in core systems are assigned explicit stages: `transform_system` →
`transform`, `physics_system` → `physics`, `shadow_system` / `lighting_system` /
`render_3d_system` / `render_2d_system` / `skybox_system` / `render_ui_system` →
`render_build`, `audio_system` → `logic`. With these assignments the only engine
 WAW (`transform` vs `physics` on `world_transform`) resolves to **physics wins**
 (later in the vector: `physics` at position 3 vs `transform` at 2), which is the
 intended authoritative behavior **assuming physics owns dynamic bodies**. Validate
 this; ideally `transform_system` should skip entities that have a `rigid_body` so
 it no longer writes `world_transform` for them — that removes the WAW entirely
 (no wasted write, no ordering ambiguity) and is the cleaner long-term fix.
intended authoritative behavior.

**API:**
- C++: `system_registration_options` gains a `stage` (name); built-ins pass it in
  `register_system_type`. A system may also call `set_stage(name)` in its
  constructor; unassigned → `logic`.
- Daslang: `set_system_stage(name)` (mirrors `set_system_dependencies` via
  `das_system_adapter::current()`).
- `runtime_context` exposes `register_stage(name)` (append), `insert_stage(name,
  before_name)`, `register_stage_at(name, position)`, and enumerates stages.

**Per-pass uniformity:** a stage is a property of the *system*, so it applies to
every pass (`update`, `editor_update`, `render_build`). A system assigned to
`logic` (position 6) that also overrides `render_build_draw_data` will therefore
build draw data at position 6 — *before* the built-in render systems at
`render_build` (8). If a draw-building system must run alongside the built-ins,
assign it to the `render_build` stage (or insert a custom stage between `logic`
and `render_build`).

### 4.6 No implicit tier edges (review concern #5)
The earlier plan's "implicit core→scene edges" option is **dropped**. Authors
declare dependencies on *any* set of core or user-defined systems (type id or
name); the scheduler honors exactly those plus the component-access (CADS)
edges. Nothing forces every scene system to wait for the whole core tier, so
parallelism is not artificially limited. This is a behavior change from today
  (scene systems no longer implicitly run after all core) and must be documented
  and tested — scene authors that read physics/transform results must now declare
  those dependencies (the component-access (CADS) edges already cover
  `world_transform` reads, so most cases are safe automatically; explicit
  declaration is still recommended
for clarity). A debug setting `warn_undeclared_cross_tier` (default true) logs
scene systems that participate in level 0 alongside core writers.

### 4.7 Output
- `std::vector<std::vector<ecs_system*>> levels` via longest-path / Kahn
  topological sort over (declared ∪ component-access ∪ stage-derived) edges.
  Level 0 = no unsatisfied deps. A system's level = `1 + max(level of its deps)`.
  Systems with a cycle
  are logged and forced into a safe sequential fallback level (never silently
  dropped).
- Cache the built `levels` and **invalidate** it whenever systems are
  added/removed or activation changes
  (`core_systems::rebuild_system_cache`, `refresh_activation`), and whenever a
  Das system declares dependencies in `on_init`. Rebuild is cheap and rare, so
  frame cost stays near zero.

## 5. Scheduler: `wsl::sys::system_scheduler`
New file `src/wsl/sys/system_scheduler.hpp` / `.cpp`. Owns one `task_pool`.

```
void run_pass(pass p,                     // update | editor_update | render_build
              std::vector<ecs_system*>& systems,
              entt::registry& reg, double dt,
              system_factory_registry& sys_reg);
```

Algorithm:
 1. Build/refresh `system_dependency_graph` → `levels` (declared ∪
    component-access (CADS) ∪ stage-derived WAW edges).
2. For each level `L` in order:
    - For each system `s` in `L`: `pool.enqueue([s,&reg,dt]{ s-><pass>(reg,dt);
      })` (e.g. `update`, `editor_update`, or `render_build_draw_data`).
    - **Fence**: `pool.dispatch` waits until all tasks in `L` complete. *This
      fence is the dependency barrier.* Level `L+1` cannot begin until the fence
      returns.
3. Honour existing gating: only `m_active` systems enter the `update` graph;
   only `m_editor_active` systems enter the `editor_update` graph; only
     only active/editor-active systems overriding `render_build_draw_data`
     enter the `render_build` graph.
   Skip systems where `has_failed()` (as today). **Cascade**: if a dependency
   system `has_failed()`, also skip its dependents (currently not done — new
   safety).

 `entt::registry` caveat: concurrent **writes** to the same component / structural
 changes from two parallel systems are data races. The component-access (CADS)
 edges (§4.4) are the synchronization contract and are computed automatically;
the declared edges are an additional explicit contract. The render
`render_build_draw_data` pass is safe because it reads components and writes into
per-system draw buffers (no shared entt writes); `render_prepare_gpu_rsc` and
`render_record_draw_cmd` remain serial (GPU-state risk) and are **out of scope**
for v1.

## 6. Integration Points
- `core_systems::update` (`core_systems.cpp:248`): replace the four sequential
  loops with two `system_scheduler::run_pass` calls — one for `update` over the
  merged active core+scene list, one for `editor_update`. The scheduler owns the
  `task_pool`; `core_systems` constructs it once.
- Keep `m_runtime_ctx->sync()` and `update_async_uploads()` exactly where they
  are (before the passes) — they are not systems.
- `core_systems::render` (`core_systems.cpp:396`): route the
  `render_build_draw_data` loop (currently `core_systems.cpp:417-435`) through
  `system_scheduler::run_pass(render_build, ...)`. Fence, then continue with the
  existing serial `prepare_gpu_rsc` / `record_draw_cmd` phases. Gate behind
  `parallel_render_build_enabled` (default true; safe due to §4.4).
- `scene::update` (`scene.cpp:134`) stays as a valid standalone stepping API but
  must also go through the scheduler when invoked (so it benefits from the same
  fencing). `scene_manager::update` (`scene_manager.cpp:227`) is **removed**
  (dead code — see §8).

## 7. Configurability & Diagnostics
New settings on `runtime_context` / engine config:
- `parallel_systems_enabled` (default `true`).
- `max_system_worker_threads` (default hardware concurrency).
- `parallel_render_build_enabled` (default `true`; safe via §4.4 / §4.5).
- `stage_fences_enabled` (default `false`): when `true`, every stage boundary is a
  hard fence (stage N completes before N+1); when `false` (default) stages
  overlap and only WAW pairs are ordered by stage position (§4.5).
- `deterministic_system_order` (default `false`): when `true`, run
  single-threaded in declared/level order for reproducible debugging — keeps the
  DAG for validation but serializes execution.
- `warn_undeclared_cross_tier` (default `true`): log scene systems that share a
  level with core writers without an explicit dependency.
- `strict_system_ordering` (default `false`): promote unresolved same-stage WAW
  conflicts from a warning to a hard error (forces the author to declare order).
- Stage registry: built-in default stages (§4.5) are registered as an ordered
  vector at startup; users extend via `register_stage(name)` / `insert_stage(name,
  before_name)` / `register_stage_at(name, position)`. Unassigned systems default
  to the `logic` stage.

Diagnostics:
- Tracy: add a `ZoneScopedN("sys::level_N")` around each level (on the main
  thread) and keep existing per-system zones so parallelism is visible.
- On graph build, log the computed level count and any cycles/failed cascades at
  `debug` level.
- Component-access capture (§4.3 / §4.4) is active in v1: when two systems in the
  same level would share a writable component (or a read/write pair), the builder
  already separates them by a component-access edge; if it must use the conservative
  "unknown access" fallback, log at `debug` so authors can declare access
  explicitly and recover parallelism.

## 8. Cleanups / Risks
- **Remove dead code (review concern #4):** delete `scene_manager::update`
  (`scene_manager.cpp:227-232`) — it has no live caller and only existed as a
  double-update trap. `scene::update` remains a legitimate standalone step API
  but also routes through the scheduler. `core_systems::update` is the single
  frame driver.
- **Naming inconsistency** (`"Transform"` vs `"Transform System"`): resolved by
  the §4.2 name→instance fallback. Prefer migrating `set_relationships` callers
  to type-id `declare_system_dependency` as a follow-up; the builder supports
  both in v1.
- **Physics/Transform reorder (review concern #2):** honoring `physics →
  transform` moves physics *after* transform (today physics runs first in the
  cache). This is a deliberate behavior change and likely a correctness fix for
  stale `world_transform` reads; add a regression test and a release note.
- **Core/scene ordering (review concern #5):** implicit tier edges are removed;
  ordering is entirely declared + component-access (CADS). Document that scene
  systems no longer implicitly run after all core systems; the component-access
  (CADS) `world_transform` edges keep the common cases safe.
- **Daslang runtime systems** (`das_system_adapter`): they are `ecs_system`
  instances and participate automatically once §4.3 gives them dependency
  declaration; ensure their `get_type_id()` is stable so deps resolve.

## 9. Implementation Order (TODO checklist)
 - [x] Add `task_pool` (`src/wsl/sys/task_pool.{hpp,cpp}`).
 - [~] Add query-level read/write component capture (CADS): `register_iteration_rw`
       (`reads<>`/`writes<>` split) and `collect_component_access` added; `register_iteration`
       keeps treating all listed components as writes (conservative). Remaining: extend
       the Das `query()` macro to record read/write access on the active adapter; add the
       one-time calibration fallback for raw `registry` access (until then, systems using
       raw `registry.get/view` only are treated as *unknown* and serialized, §4.3).
 - [x] Add `system_dependency_graph` (declared edge resolution via type-id + string +
       instance-name fallback + Das deps + CADS component-access conflict edges +
       **stage-derived WAW edges** (stage position, then type-id tie-break + warning) +
       topological `Kahn` levels + cycle fallback + unknown-access serialization +
       failed-system cascade). Caching/invalidation is recomputed per `run_pass` (cheap
       at current system counts).
 - [x] Add `system_scheduler` wrapping the pool; implement `run_pass` with per-level
       `dispatch` fences for `update`, `editor_update`, and `render_build`; honors
       `parallel_systems_enabled`, `deterministic_system_order`, and
       `parallel_render_build_enabled`.
 - [x] Wire `core_systems::update` to use the scheduler for `update` + `editor_update`.
 - [x] Wire `core_systems::render` `render_build_draw_data` through the scheduler
       (`parallel_render_build_enabled`).
 - [x] Das dependency API: `set_system_stage`, `set_system_dependency`,
       `set_system_conflict` free functions in `weasel_api` (backed by
       `das_system_adapter::set_current_stage` / `add_current_dependency` /
       `add_current_conflict`); catalog regenerated.
 - [x] Stages: ordered `stage_registry` vector with default stages (§4.5),
       `register_stage` / `insert_stage` / `register_stage_at`, `position_of`,
       `system_registration_options::stage`, `system_descriptor::stage`,
       `ecs_system::set_stage` + `add_dependency`/`add_conflict`, Das `set_system_stage`,
       default `logic` stage, and `stage_fences_enabled` (hard fences between stages).
       Core systems assign stages in `register_factory_types`; runtime/Das systems default
       to `logic`.
 - [x] Add engine settings on `runtime_context::parallel_settings()`
       (`parallel_systems_enabled`, `parallel_render_build_enabled`,
       `max_system_worker_threads`, `stage_fences_enabled`, `strict_system_ordering`,
       `warn_undeclared_cross_tier`, `deterministic_system_order`). `core_systems` reads
       them each frame via `ensure_scheduler`.
 - [x] Add Tracy level zones + debug logging of level graph (`ZoneScopedN` per pass/level;
       warnings for unknown stages and same-stage WAW).
 - [x] Remove `scene_manager::update` dead code (§8).
 - [ ] Unit tests (`tests/weasel-core`): graph leveling, cycle handling, failed-system
       cascade, fence ordering (mock `ecs_system`), component read/write conflict
       graph (incl. WAW tie-break + warning under `strict_system_ordering`), and
       Das `query()` read/write capture.
 - [ ] (Optional future) Parallelize `render_prepare_gpu_rsc` behind the same
       scheduler once per-system GPU upload thread-safety is verified.

## 10. Verification
- Build: `make configure && cmake --build build -j2`.
- Run editor, observe in Tracy that independent Das/scene `update` systems now
  overlap within a level and a fence separates levels; `physics` now runs
  *after* `transform` (behavior change from today — verify rigid bodies track
  transforms correctly).
- Toggle `deterministic_system_order=true` and confirm identical per-frame order
  with no parallelism (regression safety).
- Toggle `parallel_render_build_enabled=true` and confirm `render_build_draw_data`
  overlaps while still reading up-to-date `world_transform` (component-access
  edges order it after transform/physics).
- Toggle `parallel_systems_enabled=false` to fully serialize (debugging escape
  hatch).
- Toggle `stage_fences_enabled=true` and confirm (in Tracy) that every stage
  boundary is a hard fence while intra-stage systems still overlap; then set it
  back to `false` (default soft overlap).
- Confirm the `transform`/`physics` WAW resolves to physics-last via the
  `world_transform` log/zone ordering; set `strict_system_ordering=true` and
  confirm an unresolved same-stage WAW is reported as an error.
- Run new `tests/weasel-core` unit tests.
