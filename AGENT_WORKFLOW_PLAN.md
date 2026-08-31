# Agent Workflow & Engine Internals Fixes — Plan

## 1. Goals & Motivation

An external AI agent built a game on top of Weasel (CLI/MCP-driven workflow) and
reported three blockers plus a set of undocumented behaviors and tooling gaps.
Every issue below traces back to real code paths; this plan maps each finding to
a concrete internal fix, ordered by impact on agent-driven development.

| # | Blocker | Root cause |
|---|---------|-----------|
| 1 | Runtime-script loading state machine is opaque | Three interacting gates: `has_loaded_module()`, `has_loaded_cached_metadata()` (`src/wsl/reg/runtime_project_module.hpp:218-219`) and the on-disk `runtime_registration_cache.json` (`src/wsl/reg/runtime_project_module.cpp:44`). |
| 2 | No authoritative Daslang API reference | `Module_WeaselApi` registrations exist only in `src/wsl/das/wsl_api_module.cpp`; installed docs are a stale hand-written stub. |
| 3 | `describe_namespace('sys')` / `describe_command('sys create')` teach C++ systems | Docs written for an engine-internal C++ path that user projects cannot use (`runtime_project_module.hpp:39-44`: "User C++ runtime sources are rejected"). |

Secondary findings (documented behaviors, save semantics, tokenizer, MCP
mutation gap, headless verification, serialization artifact) are covered by
Workstreams D–F.

Non-goal: no behavior change to the ECS itself; this plan is about state
machine clarity, truthful documentation, and closing the agent automation loop.

---

## 2. Workstream A — Runtime-script loading state machine

### A1. Problem inventory

Current gating logic lives in
`command_executor::ensure_runtime_module_loaded(bool allow_cached_metadata)`
(`src/cli/repl_handler.cpp:750-789`):

```
if (!project || has_loaded_module()) return;          // already fully loaded
if (allow_cached && has_loaded_cached_metadata()) return;  // metadata fast path
...
if (allow_cached && load_cached_metadata(project)) return; // may load EMPTY registry
compile_and_load(project);                            // slow path (scene load only)
```

Observed failures:

- **Stale empty registry:** `sys avail` / `sys add` call
  `ensure_runtime_module_loaded(true)` (`repl_handler.cpp:1494, 1574, 2394`).
  If `load_cached_metadata()` succeeds against a stale/empty
  `runtime_registration_cache.json`, both commands see an empty registry
  *forever* — the in-memory flag short-circuits every later attempt.
- **Deleting the cache file doesn't help:** the on-disk file is only consulted
  when `m_metadata_cache_loaded == false`; removing the file does not reset the
  in-memory flag.
- **`proj load` is silent:** `cmd_proj("load")` (`repl_handler.cpp:1069-1087`)
  never triggers compilation itself and reports nothing about runtime state;
  background compile failures are invisible.
- **The bypass is undiscoverable:** only scene load
  (`ensure_runtime_module_loaded()` with the default
  `allow_cached=false`, `repl_handler.cpp:1262`) forces a full compile. An
  agent has no way to know this except reading `repl_handler.cpp`.
- **Editor/CLI asymmetry:** the editor's "Reload Scripts" button calls
  `runtime_project_module().compile_and_load_async()` directly
  (`src/editor/file_list.cpp:576,585`). There is no CLI or MCP equivalent,
  which is exactly what agent workflows need.

### A2. Fix: explicit load-state enum + single entry point

Replace the two independent bools (`m_module_loaded`,
`m_metadata_cache_loaded`) with one authoritative state:

```cpp
enum class load_state_t { unloaded, metadata_cache, loaded };
load_state_t m_load_state = load_state_t::unloaded;
std::string  m_last_error;   // populated when a compile fails
```

in `src/wsl/reg/runtime_project_module.hpp/.cpp`. `last_status()` already
exists; extend it to always carry the reason for the last transition
("loaded from metadata cache", "compiled", "compile failed: <error>").

Collapse the CLI side onto one helper with two explicit modes instead of a raw
bool:

```cpp
// repl_handler.hpp
enum class runtime_load_mode { auto_mode, force_full };
bool ensure_runtime(runtime_load_mode mode);   // returns success, prints diagnostics
```

- `auto_mode` = current best-effort behavior, **with escalation** (A3).
- `force_full` = what scene load does today; also used by the new
  `script reload` command (A4).

All 15 call sites of `ensure_runtime_module_loaded(...)`
(`repl_handler.cpp:1262 … 3121`) migrate mechanically.

### A3. Fix: cache validity must be proven, not assumed

`load_cached_metadata()` currently succeeds if the cache file exists and its
`source_hash` matches. Tighten it in
`src/wsl/reg/runtime_project_module.cpp`:

1. **Emptiness check:** if the deserialized cache contains zero components,
   zero systems, and zero singletons while `.das` sources exist under
   `systems_path`/`components_path`/`singletons_path`, treat the cache as
   invalid, delete it, log why, and fall through to full compile.
2. **Registry sanity check after apply:** after
   `apply_registration_cache()`, if `system_factory_registry` gained nothing,
   same fallback.
3. **State reset on failure:** any failed validation resets `m_load_state` to
   `unloaded` so deleting the cache file (or fixing sources) takes effect on
   the next command without restarting the process.
4. Log the active source on every command that depends on it:
   `registry source: full-compile | metadata-cache (<file>, hash=<n>)`.

### A4. Fix: expose reload through CLI and MCP

Add parity with the editor button:

- New REPL/CLI command `script reload [full]`:
  - kicks `compile_and_load_async()` (mirrors `file_list.cpp:576`),
  - polls `poll_async_reload()` until completion (one-shot mode blocks;
    REPL prints progress),
  - on finish prints `finalize_load()` result, new component/system counts,
    and `last_status()`; non-zero exit code on failure in one-shot mode.
- New subcommands for introspection:
  - `script status` — prints load state enum, loaded project root,
    source hash, registry counts, last error.
  - `script invalidate` — clears in-memory state AND deletes the on-disk
    cache file (the "deleting the cache doesn't help" trap).
- Register `script reload` as an MCP tool (`script_reload`) in
  `src/mcp-server/mcp_server_app.cpp` (see Workstream E for the general
  mechanism).

### A5. Fix: make `proj load` honest

In `cmd_proj("load")` (`repl_handler.cpp:1069`):

- After loading the manifest, run `ensure_runtime(auto_mode)` synchronously in
  one-shot mode (agents need deterministic completion), async in REPL mode.
- Print runtime outcome explicitly:
  `Runtime: compiled N components / M systems` or
  `Runtime: FAILED — see 'script status'; continuing without user scripts.`
- One-shot exit code reflects runtime failure when `--strict-runtime` is set.

### A6. Verification (manual)

- [ ] `rm <project>/.../runtime_registration_cache.json` (path per
      `registration_cache_path()`) followed by `sys avail` shows the full
      system list again (state reset works).
- [ ] A project whose sources compile but whose cache was hand-crafted empty
      still ends up with a working registry (escalation works).
- [ ] `weasel-cli -a "script reload"` produces the same registry state as the
      editor's Reload Scripts button.
- [ ] Every command that ends with an empty registry despite existing
      `.das` sources prints `script status` guidance including `last_status()`.

---

## 3. Workstream B — Authoritative Daslang API reference

### B1. Problem inventory

- The real API surface is the `addExtern<>()` call sequence inside
  `Module_WeaselApi` (`src/wsl/das/wsl_api_module.cpp:2649`, registrations
  e.g. `is_key_pressed` at :2828, `apply_impulse` at :2983,
  `apply_force` at :2987). Nothing generated is shipped.
- `/usr/local/share/weasel/daslib/weasel_api.das` on end-user machines is a
  hand-written stub declaring functions that don't exist (`set_position`,
  `get_position_x`, camera helpers…). Current CMake only installs
  `weasel_helpers.das` and `weasel_ecs.das` (`CMakeLists.txt:882-893`), so the
  stub is a leftover from older installs — but it keeps lying to anyone who
  finds it.
- `doc/source/stdlib/weasel_api.rst` is hand-maintained and already drifted
  (e.g. `is_key_pressed(camera : int)` documents the parameter as `camera`;
  the real signature takes a scancode).
- Critical functions (`apply_force`, `apply_impulse`, `is_key_pressed`)
  appear nowhere an agent looks first; they were found by grepping
  `wsl_api_module.cpp`.

### B2. Fix: generate the API reference from the registration site

Introduce a lightweight registration ledger next to `addExtern`:

1. In `src/wsl/das/wsl_api_module.cpp`, wrap each `addExtern` call in a small
   helper (e.g. `register_api_fn(name, category, description,
   side_effects, fn_ptr)`) that performs the existing `addExtern` **and**
   appends `{name, signature, category, description}` to a static
   `api_registry` vector. Signature can be captured from the das-side
   prototype string used today in comments/headers; categories: entity,
   components, physics, input, audio, events, scene, window, logging.
   - Alternative if per-call wrapping is too invasive: a build-time
     reflection pass over `wsl_api_module.cpp` (regex/clang-based) that emits
     the same table. Prefer the wrapper: single source of truth, no parser.
2. Expose the table twice:
   - **MCP tool `describe_das_api`** (new handler
     `src/mcp-server/das_api_info.{hpp,cpp}`, registered in
     `mcp_server_app.cpp:register_tools`):
     - no args → grouped listing (name + one-line description),
     - `name` arg → full entry: signature, semantics, side-effect class,
       usage example, related functions.
   - **CLI command `das api [name]`** in `cli_handler.cpp`/`repl_handler.cpp`
     for attach-mode users.
3. Regenerate `doc/source/stdlib/weasel_api.rst` from the same registry via a
   small `--dump-docs-rst` mode of `weasel-cli` (or a dedicated generator
   target wired into the existing `docs` target). Delete hand-written drift;
   *(optional)* a small unit test in `tests/weasel-cli` asserting the rst
   matches the runtime registry would prevent future drift, but manual
   regeneration per release is acceptable.
4. Document the guaranteed-correct facts that agents need most, pulled from
   the ledger: `apply_force`, `apply_impulse`, `is_key_pressed(scancode)`,
   `get_component` proxy accessors, event declare/connect helpers.

### B3. Fix: kill the lying stub everywhere

- Add an install-time cleanup step (e.g.
  `install(CODE "file(REMOVE ...share/weasel/daslib/weasel_api.das)")`) and a
  matching line in `packaging/linux` so upgrades from old versions remove the
  stale stub instead of leaving it beside the real native module.
- If a `.das` declaration is ever wanted for standalone-daslang tooling, it
  must be **generated** from the same ledger, never hand-written; mark the
  ledger as its only source.
- Update `src/wsl/das/README.md` (already explains native resolution,
  `README.md:44,62-72`) to add a prominent "ignore any `weasel_api.das` you
  find on disk" warning plus pointer to `describe_das_api`.

### B4. Verification (manual)

- [ ] The MCP tool lists every extern registered in `Module_WeaselApi`,
      zero omissions, zero inventions.
- [ ] Grepping the repo for `set_position` finds no shipped doc/stub claiming
      it exists.
- [ ] Sphinx docs regenerate cleanly from the ledger.

---

## 4. Workstream C — Truthful engine-facing documentation (MCP)

### C1. Problem inventory

- `describe_namespace('sys')` (`src/mcp-server/namespace_info.cpp:145-300`)
  teaches `ecs_system`, CRTP helpers, and
  `factory.register_system_type<my_system>` C++ workflows — impossible for
  users, since runtime C++ systems are rejected
  (`src/wsl/reg/runtime_project_module.hpp:39-44`).
- `describe_command('sys create')`
  (`src/mcp-server/cli_reference.cpp:371`) says "Generate a C++ system
  template file (header and optionally source)" while the implementation
  emits a Daslang file (`cmd_sys` create branch,
  `src/cli/repl_handler.cpp:2350-2388`, writes `class System : EcsSystem`
  into `<systems>/<name>.das`).

### C2. Fix

Rewrite the docs around the only supported path (Daslang):

1. **namespace_info.cpp `sys` section:** replace the CRTP/factory walkthrough
   with the Daslang lifecycle:
   - `sys create <name>` → generates `<systems>/<name>.das`
     (`options gen2`, `require weasel_ecs`, `class System : EcsSystem`,
     `def override on_update(dt : float)`),
   - display name derivation (`foo_bar_system.das` → "Foo Bar System", see
     D4),
   - `sys avail` / `sys add "<display name>"` / `sys ls`,
   - note that `add` requires the runtime module to be loaded — reference the
     new `script status`/`script reload` commands (Workstream A),
   - a complete minimal movement-system example using `get_component`
     proxies and `apply_force`.
2. **cli_reference.cpp:** correct the `sys create` summary to "Generate a
   Daslang system template (.das) in the project's systems directory"; audit
   neighboring entries (`comp create`, `singl create` at :273/:295 say "C++
   world/singleton component template" — verify what they emit and align).
3. **Cross-check sweep:** *(optional)* a unit test in `tests/mcp-server`
   asserting no namespace/command doc mentions "C++" for anything under
   `sys/comp/singl create` flows would guard against regression — the suite
   already covers these handlers (`test_mcp_server.cpp:136`
   "handle_describe_namespace for sys"), so extending it is cheap.

### C3. Verification (manual)

- [ ] `describe_namespace('sys')` contains no C++ registration instructions;
      its example compiles when pasted into a fresh `sys create` file.
- [ ] `describe_command('sys create')` matches actual emitted artifacts.

---

## 5. Workstream D — Surface hidden engine behaviors as structured metadata

Goal: behaviors that agents had to reverse-engineer become first-class output
of `describe_component` / `describe_namespace` from a single source of truth
(a free-text `behavior_notes` list added to component descriptors and fed
through `src/mcp-server/component_info.cpp`).

### D1. Rigid Body sync rules

Ground truth in `src/wsl/sys/physics_system.cpp`:

- Transform writes are ignored mid-simulation for Dynamic bodies:
  the transform→body reset skips them unless `force_all`
  (`physics_system.cpp:519-523`); `force_all=true` is an editor-only escape
  hatch (:282 comment "skips the dynamic-body check so editor movement…").
- Scale-change rebuilds likewise skip dynamic bodies (the rebuild view at
  `physics_system.cpp:581` region).

Document in `describe_component("Rigid Body")`:

- ownership model (physics owns the body once created; engine reads back),
- the supported respawn/teleport pattern (remove + re-add component, or
  future `teleport()` binding),
- kinematic/static bodies accept transform writes.

### D2. Mass / force scale

`rigid_body::create_body` uses
`mOverrideMassProperties = CalculateMassAndInertia`
(`src/wsl/comp/rigid_body.cpp:434-435`) with Jolt's default density
(1000 kg/m³) → r=0.5 sphere ≈ 524 kg, hence ~6500 N to get ≈ g lift.
Options (implement 1, decide on 2):

1. **Document** in `describe_component("Rigid Body")` + `phys` namespace:
   density assumption, worked example, expected magnitude for
   `apply_force`/`apply_impulse`.
2. *(follow-up)* Expose a `density` property on `rigid_body` and/or a
   `get_mass(entity)` Daslang binding so scripts can size forces
   programmatically.

### D3. Built-in model dimensions

`builtin://sphere` = radius 0.5, `builtin://cube` = half-extent 0.5
(`src/wsl/gfx/model_3d.cpp:99,208,416-436`).
`namespace_info.cpp:372-384` already states this — promote those exact numbers
into `describe_component("Model Instance 3D")` and the `rsc` namespace text so
the fact survives regardless of which doc the agent reads.

### D4. System display-name derivation

`foo_bar_system.das` registers as "Foo Bar System" (title-case derivation in
the runtime registration path of `runtime_project_module.cpp`; mirrored by
`to_title_case` in `sys create`, `repl_handler.cpp:2354`).
Document in `describe_namespace('sys')` and `describe_command('sys add')`,
and make `sys avail` output show `file → display name` pairs so mapping is
self-evident.

### D5. Verification (manual)

- [ ] All four facts answerable from MCP alone, no source grepping.
- [ ] Each note carries the file anchor it came from (kept in sync manually
      for now; ledger-ization like B2 can follow).

---

## 6. Workstream E — Close the agent loop (save semantics, tokenizer, MCP mutations, headless verification)

### E1. Save semantics follow the loaded path

Bug: `scene save` with no argument derives the filename from the *scene name*
(`repl_handler.cpp:1299-1319`) → attached-mode saves wrote
`Main Scene.wscn.json` while `wslpro.json`'s `default_scene_path`
(`src/wsl/rsc/project.hpp:59`) still pointed at `main.wscn.json`; the project
then opens the stale scene (`resource_manager.cpp:1391-1392`).

Fix in `cmd_scene("save")`:

1. Default order becomes:
   1. `m_active_scene_source_path` (set on load, `repl_handler.cpp:1273`) if
      the scene was loaded from disk,
   2. `<root>/<scenes_path>/<default_scene_path>` when the scene name matches
      the default scene's stem,
   3. current name-derived path as last resort.
2. If the resolved target differs from `default_scene_path`, print a warning:
   `Warning: saved to X but project default_scene_path points at Y — run
   'proj set default_scene_path X' or 'proj save'.`
3. Same treatment for `prefab save` (`repl_handler.cpp:3036-3085`).
4. Optional convenience: `scene save --as-default` updates
   `default_scene_path` and persists the project in one step.

### E2. Tokenizer: stop leaking splits into errors

One-shot CLI mode builds `"sys" "add" "<single arg>"` (`cli_handler.cpp:825`),
so unquoted `weasel-cli -a sys add Ball Controller System` reaches the executor
split and fails with `Unknown system: Ball` (`repl_handler.cpp:2406`).

Fixes:

1. Improve the error to be self-diagnosing: print the closest matches from
   `system_factory_registry` (Levenshtein or prefix):
   `Unknown system: 'Ball'. Did you mean: 'Ball Controller System'? Use
   quotes: sys add "Ball Controller System".`
2. In one-shot mode (`cli_handler.cpp`), join trailing non-flag args for
   known multi-word slots (`sys add`, `sig connect <event> <system>
   <handler>` positions) instead of passing one token.
3. Long-term: route all name lookups through one resolver that owns error
   reporting. (Note: the REPL tokenizer already supports quotes —
   `tests/weasel-cli/test_command_executor.cpp:60` "tokenize double-quoted
   string preserves spaces" — so only one-shot CLI joining needs work.)

### E3. MCP mutation tools

Today every MCP tool is introspection
(`src/mcp-server/mcp_server_app.cpp:306-450`: list_commands,
describe_command, get_quick_start, list_components, describe_component,
list_namespaces, describe_namespace, cli_capabilities), so all mutations go
through bash anyway.

Implement in two tiers:

1. **Tier 1 (low risk, closes the loop):** `run_cli_command` passthrough tool
   — executes a weasel-cli command string in attach mode
   (reuse `src/cli/editor_client.{hpp,cpp}` transport), returns stdout/stderr
   and exit code. Guard rails: explicit confirmation flag, optional allowlist
   regex configured at server startup, timeout.
2. **Tier 2 (structured, later):** first-class tools for the highest-value
   mutations, implemented atop the same executor:
   `script_reload` (from A4), `scene_save`, `entity_add`/`entity_remove`,
   `component_set`. Each returns structured JSON instead of parsed text.
3. Update the AGENTS.md MCP instructions block once Tier 1 ships: mutations
   should prefer `run_cli_command` over raw bash.

### E4. Headless verification: `scene play --frames N`

Agent gameplay verification currently means eyeballing the editor. Add:

```
weasel-cli --project P --scene S play --frames 120 [--inspect e|name] [--json]
```

Implementation sketch:

- Headless loop already exists conceptually (core systems tick without
  rendering; `m_rtc.core_systems()` headless branch at
  `repl_handler.cpp:2266-2278`). Drive `physics_engine.update`,
  `transform_system`, and user systems for exactly N fixed steps.
- `--inspect` snapshots requested entities after the final frame:
  position/rotation/scale, rigid-body velocity, component list.
  `--frames K --inspect` optionally samples every K frames for trajectories.
- `--json` emits machine-readable output (pairs with the serializer's JSON
  style in `scene_snapshot_serializer.hpp`).
- Determinism notes: seed/fixed-dt documented so agents can reproduce runs.

### E5. Serialization artifact: stale `entity_names`

`scene::remove_entity_name` (`src/wsl/rsc/scene.cpp:305-308`) is **never
called anywhere**, so destroying an entity leaves its name in
`header.entity_names` on save (`scene_snapshot_serializer.cpp:123-124`
iterates the whole map; struct at `scene_snapshot_serializer.hpp:57`).

Fix:

1. Call `remove_entity_name(e)` on the destroy path (wherever
   `registry.destroy(e)` is funneled — `ent rm` executor path and editor
   deletion), or better: purge defensively in
   `scene_snapshot_serializer::save` before building the header
   (`erase_if(names, entity not alive)`).
2. Do both: immediate cleanup at destroy + defensive purge at save.
3. *(Optional)* `tests/weasel-cli/test_command_executor.cpp` already covers
   `ent rm destroys entity` (:235); a create → destroy → save → reload case
   could be added there if desired — otherwise verify manually.

---

## 7. Phasing & priority

| Phase | Items | Rationale |
|-------|-------|-----------|
| 1 — unblock agents now | A3 (cache escalation), A4 (`script reload/status/invalidate` + MCP), C2 (doc corrections), E5 (stale names) | Smallest diffs; remove the traps that caused wrong beliefs. |
| 2 — trustworthy docs | B2–B3 (API ledger + `describe_das_api` + stub removal), D1–D4 (metadata notes), E1 (save follows path), E2 (tokenizer errors) | Eliminate guessing; prevent the duplicate-scene class of bug. |
| 3 — close the loop | E3 tier 1 (`run_cli_command`), E4 (headless play/inspect), A5 (`proj load` honesty) | Agents verify themselves end-to-end. |
| 4 — polish | E3 tier 2 structured mutation tools, D2 option 2 (density/mass bindings), *(optional)* doc-drift unit tests | Ergonomics once correctness lands. |

## 8. Master TODO checklist

Testing strategy: verification is **manual** (per-workstream checklists in
A6/B4/C3/D5 above). Unit tests are optional; where the existing suites
already have anchors, extending them is cheap but not required.

### Phase 1 — unblock agents now

- [x] A2: replace `m_module_loaded`/`m_metadata_cache_loaded` with a single
      `load_state_t` + `m_last_error` (`runtime_project_module.{hpp,cpp}`)
- [x] A2: collapse `ensure_runtime_module_loaded(bool)` into
      `ensure_runtime(runtime_load_mode)`; migrate all call sites in
      `repl_handler.cpp`
- [x] A3: cache-validity escalation (emptiness + post-apply registry checks,
      state reset on failure, log registry source)
- [x] A4: add `script reload` / `script status` / `script invalidate` commands
- [x] A4: register `script_reload` MCP tool
- [x] C2: rewrite `namespace_info.cpp` sys section for Daslang-only workflow
- [x] C2: fix `cli_reference.cpp` `sys create` text; audit `comp create`,
      `singl create` entries
- [x] E5: purge stale `entity_names` (destroy path + defensive save purge)

### Phase 2 — trustworthy docs

- [x] B2: registration ledger extracted from `addExtern` calls in
      `wsl_api_module.cpp` via `generate_das_api_catalog.py` →
      `das_api_catalog.gen.hpp` (all 86 externs, zero omissions)
- [x] B2: `describe_das_api` MCP tool (`das_api_info.{hpp,cpp}`) + CLI
      `das [name]`
- [x] B2: regenerated `doc/source/stdlib/weasel_api.rst` from the ledger
- [x] B3: install-time removal of stale `weasel_api.das` stub (CMake +
      packaging); README warning in `src/wsl/das/README.md`
- [x] D1: Rigid Body sync-rule notes in `describe_component`
- [x] D2: mass/density scale documentation (density property = follow-up)
- [x] D3: builtin model dimensions promoted to `describe_component`
- [x] D4: display-name derivation documented in `sys` namespace + `sys avail`
      file-stem convention note
- [x] E1: `scene save` follows loaded path + warns on `default_scene_path`
      mismatch (+ optional `--as-default`)
- [x] E2: self-diagnosing "Unknown system" errors with suggestions;
      one-shot multi-word arg joining (`sys add`)

### Phase 3 — close the loop

- [x] A5: `proj load` runs/triggers runtime load and reports outcome
- [x] E3 tier 1: `run_cli_command` MCP passthrough (attach mode, confirm
      gate, timeout)
- [x] E3 item 3: update MCP server instructions to advertise the mutation
      tools (`script_reload`, `run_cli_command`)
- [x] E4: headless `play --frames N [--inspect] [--json]` (ticks Transform +
      Physics core systems + scene user systems; renders skipped)

### Phase 4 — polish (optional)

- [x] E3 tier 2: structured mutation tools (`scene_save`, `entity_add`,
      `entity_remove`, `component_set`) registered as MCP tools with confirm gate
- [x] D2 option 2: `density` property on `rigid_body` (kg/m^3, serialized,
      structural-change aware) deriving body mass; `get_rigid_body_mass` +
      `RigidBody.density`/`RigidBody.get_mass()` Daslang bindings
- [x] Optional unit tests: doc-drift guard in `tests/mcp-server`
      (catalog vs `wsl_api_module.cpp` addExtern registrations)

### Final manual walkthrough (after all phases)

1. Fresh project → `sys create ball controller system` → edit `.das` →
   `script reload` → `sys avail` shows it → `sys add "Ball Controller
   System"` succeeds.
2. Break a `.das` source → `script reload` reports failure visibly;
   `script status` shows the error; fixing the source and reloading recovers.
3. Delete `runtime_registration_cache.json` mid-session → next command
   recovers without restart.
4. Load `main.wscn.json`, mutate, `scene save` → writes back to
   `main.wscn.json`; no duplicate scene appears.
5. Spawn ball → apply force via script → `play --frames 120 --inspect ball
   --json` reports movement without opening the editor.
6. Destroy an entity → `scene save` → saved JSON contains no stale names.
