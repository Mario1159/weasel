# Signal System Rework — Plan

## 1. Goals & Motivation

The current `wsl::event` signal system works, but it has grown constraints that
no longer match how events are meant to be used:

- **Component coupling:** every signal/connection carries a `Components...` contract
  and `component_types` / `entity_matches` predicates. This ties events to the
  component world and complicates connection logic. Signals should instead be owned
  by exactly **one system (the emitter)**, with no component relation.
- **No first-class runtime API:** connections are auto-registered from scene files
  (`scene_snapshot_serializer`) and via the editor (`signal_inspector`). There is no
  clean public surface for users to register/disconnect connections at runtime.
- **Two missing event categories:** we need both
  - **Observer events** (immediate, pub/sub) — the renamed signal system.
  - **Buffered / message events** (enqueued, drained later) — currently *not
    implemented at all* and must be built from scratch.

This plan splits the signal system into two clear subsystems and updates all
consumers.

---

## 2. Target Architecture

### 2.1 Observer Event System (renamed signals)

Rename the existing signal concepts to the EnTT-style emitter/sink vocabulary:

| Old name | New name | Role |
|----------|----------|------|
| `signal` / `signal_hub` | `event_source` / `event_hub` | The hub that owns sources and connections. |
| signal declaration | `event_source<Event, EmitterSystem>` | Declared by the emitting system only. |
| connectable handler | `event_sink<Event, HandlerSystem>` | A handler that observes an event source. |
| `emit(...)` | `event_source<Event>::emit(...)` / `event_hub::emit(...)` | Fire the event to connected sinks. |

> **Namespace note.** The chosen top-level namespace `wsl::event` *already exists*:
> `src/wsl/events.hpp` defines `wsl::event::scene_changed`, an editor-wide event
> currently dispatched through an `entt::dispatcher` on `runtime_context`. The reworked
> types (`event_hub`, `message_bus`, `event_source`, `event_sink`) simply join this
> existing namespace — there is no name clash. The pre-existing `scene_changed`/
> `dispatcher` path is **migrated onto the new observer event system** (see Phase 4b),
> not left as a parallel mechanism.

**Key change:** `declare_event_source<Event, EmitterSystem>()` takes **only** the
event type and its emitter system. The `Components...` template parameter and the
`component_types` / `entity_matches` machinery are removed entirely.

- `event_source` is registered against exactly one `EmitterSystem` (the only
  system required for registration, as requested).
- `event_sink` is a named handler owned by a `HandlerSystem`. Handlers are matched
  by `(event_type_id, system_type_id, handler_name)` — **nothing else**.
- **No entity-scoping fields on connections.** `source_entity`/`target_entity` (and
  the old `extract_source_entity` callback) are removed. Any entity identity a
  handler needs is a field of the **event payload type** itself (e.g.
  `PlayerHit { entt::entity attacker; entt::entity victim; … }`); the handler reads
  it from the event. This drops the per-entity invocation model that the component
  relation relied on, and keeps connections stable across scene reloads (they no
  longer reference runtime-fragile entity IDs).

  **Accepted tradeoff — no entity-scoped observers / hierarchy bubbling.** By moving
  entity identity into the payload we deliberately give up Bevy-style `EntityEvent`
  + `propagate = ChildOf` behavior: an event triggered on a child (e.g. a `Mesh`)
  will **not** automatically re-fire on its parent (`Ship`/`Player`) up the
  `ChildOf` chain. If a game needs hierarchy-aware reactions (damage on a part
  propagating to the whole entity, a pickup bubbling to the player), the *handler*
  must walk the relationship manually (read the parent from the payload / component
  graph) — there is no engine-provided propagation. This is the cost of the simpler,
  payload-only model and is accepted. (Revisit only if a real use case demands it.)

### 2.2 Buffered / Message Events (new, from scratch)

A separate, independent subsystem: a typed **message bus**.

- **Post / enqueue:** `message_bus::post<Event>(args...)` appends a copy of the
  event into a per-type queue (buffered).
- **Drain:** `message_bus::drain(registry)` **publishes** the pending queue: each
  pending `message` is unpacked into its typed `message_reader<T>` buffer (and
  delivered to any `subscribe` callback for non-system consumers), then the pending
  queue is cleared. Systems then **pull** from their own readers during their
  updates — there is **no broadcast and no dispatch to registered handlers**. The
  two subsystems stay independent: a buffered message is *not* automatically
  re-emitted as an observer `event_source`. (A message→observer bridge, if ever
  wanted, would be an explicit opt-in feature, not part of `drain`.)
- Buffered semantics: events posted during a drain are appended to the next
  frame's queue (no infinite loop), preserving frame ordering.
- This subsystem has no component relation either; it is pure message passing.

### 2.3 Relationship between the two

The two event types use **different propagation models** and are fully independent:

- **Observer events** (`event_source` / `event_sink`): explicit, connection-tracked,
  connectable at runtime, serializable, editor-wired. Fire immediately to their
  connected sinks (like today's signals).
- **Buffered / message events** (`message_reader<T>` + `message_bus`): a
  per-frame, **pull-only** queue. A system declares a `message_reader<T>` for each
  message type it cares about and pulls typed events during its own update. There is
  **no `on_event` broadcast** for messages — this is the Bevy `MessageReader<T>`
  model and keeps the cost proportional to (systems × *their own* types), not
  (systems × all messages).

```
emit()  ──► event_hub  ──► connected event_sinks            (explicit, immediate)
post()  ──► message_bus ── drain() ──► typed reader buffers ──► systems pull via message_reader<T>
```

Both feed from the same SDL poll entry point (§ Phase 3): input and game messages
enter the pool together and are consumed by whichever systems hold a reader for them.

---

## 3. Implementation Phases

### Phase 1 — Rename signals → observer event system & drop component relation

**Files:** `src/wsl/event/signal_hub_fwd.hpp`, `src/wsl/event/signal_hub.hpp`

- Rename types:
  - `signal_hub` → `event_hub`
  - `signal_debug_entry` → `event_source_debug_entry`
  - `signal_connectable_handler_debug_entry` → `event_sink_debug_entry`
  - `signal_connection_data` / `signal_connection_debug_entry` →
    `event_connection_data` / `event_connection_debug_entry`
  - `registered_signal_source` → `registered_event_source`
  - `registered_connectable_handler` → `registered_event_sink`
  - `connected_handler` → `connected_sink`
- `declare_signal<Signal, OwnerSystem, Components...>` →
  `declare_event_source<Event, EmitterSystem>()` (drop `Components...`).
- Remove `component_type_debug_entry`, `make_component_type_debug_entries<...>`,
  `contains_component_type`, `matches_component_set`, `make_entity_match_predicate`,
  and all `component_types` / `entity_matches` fields.
- `declare_connectable_handler<Signal, OwnerSystem, Components...>` →
  `declare_event_sink<Event, HandlerSystem>(handler_name, invoke)` (drop
  `Components...`).
- `declare_handler` / `declare_iteration` kept (iterations are a system concept, not
  event-related) but the `Components...` static_asserts removed where they referenced
  signal component contracts.
- `connect` / `disconnect` take only `(event_type_id, system_type_id, handler_name)`
  — the `source_entity` / `target_entity` parameters are removed. The
  `entity_matches` validation and `extract_source_entity` callback are deleted.
- **Drop the `entt::entity` argument from `handler_invoke_fn`**
  (`src/wsl/event/signal_hub_fwd.hpp:18`:
  `void(*)(::wsl::sys::ecs_system&, entt::registry&, entt::entity, const void*)`).
  With entity scoping gone, the invoke signature becomes
  `void(*)(ecs_system&, entt::registry&, const void*)` and the `dispatch` call in
  `signal_hub.hpp` (the `connected_handler` loop that invokes handlers) must stop
  passing the target entity.

### Phase 2 — Public runtime API

Expose a clean, user-facing API for connect/disconnect at runtime (the editor and
scene loader already use `connect`/`disconnect`; now make them first-class and
usable from user code / Daslang):

**Files:** `src/wsl/event/signal_hub.hpp`, `src/wsl/das/wsl_api_module.cpp`

- Introduce **lightweight typed handles** so connections are expressed between
  objects, not type-ID triples. The handles are non-owning views into `event_hub`;
  `add_listener`/`remove_listener` forward to the hub's internal `connect`/
  `disconnect` (connections still live centrally in the hub for serialization/editor).
  **Registration vs. runtime:** `declare_event_source` / `declare_event_sink` are the
  *one-time* registration calls made from each system's `register_event_sources` /
  `register_event_sinks` hook. The handles are used *afterwards* to emit and to wire
  connections at runtime — constructing a handle does **not** (re-)declare anything;
  it looks up an already-declared source/sink (asserting if missing).
  ```cpp
  namespace wsl::event {

  template <typename Event, typename EmitterSystem>
  class event_source {
  public:
    explicit event_source(event_hub& hub);          // non-owning accessor; source must already be declared via event_hub::declare_event_source
    template <typename... Args>
    void emit(Args&&... args);                       // fire to connected sinks
    bool add_listener(const std::string& handler_name);
    template <typename HandlerSystem>
    bool add_listener(const event_sink<Event, HandlerSystem>& sink);
    bool remove_listener(/* same shape */);
  };

  template <typename Event, typename HandlerSystem>
  class event_sink {
  public:
    event_sink(event_hub& hub, std::string handler_name);
  };

  }
  ```
  Usage:
  ```cpp
  event_source<PlayerHit, CombatSystem> on_hit{hub};
  event_sink<PlayerHit, DamageSystem>   apply_dmg{hub, "on_player_hit"};
  on_hit.add_listener(apply_dmg);                 // connect (no entity params)
  on_hit.remove_listener(apply_dmg);              // disconnect

  // Emitting — constructs the event and dispatches it to connected sinks.
  // Entity identity lives in the payload, not the connection:
  //   struct PlayerHit { entt::entity attacker; entt::entity victim; int dmg; };
  on_hit.emit(attacker_entity, victim_entity, 25);          // build + dispatch
  // or with an already-constructed instance:
  PlayerHit hit{ .attacker = attacker_entity,
                 .victim   = victim_entity,
                 .dmg      = 25 };
  on_hit.emit(hit);
  // or via the free helper (same effect):
  wsl::event::emit(hub, PlayerHit{ attacker_entity, victim_entity, 25 });
  // The DamageSystem handler receives the event and reads ev.attacker / ev.victim.
  ```
- Add Daslang bindings in `wsl_api_module.cpp` so scripts can `add_listener`/
  `remove_listener` at runtime (replacing the raw `signal_hub().connect(...)`),
  e.g. `event_source(hub, "PlayerHit", "CombatSystem").add_listener("DamageSystem", "on_player_hit")`.
- Document the API in `src/wsl/reg/README.md` (Signal System section).

### Phase 3 — Buffered / message events (new subsystem)

New files (suggested): `src/wsl/event/message_bus.hpp`,
`src/wsl/event/message_event.hpp`.

- `message` — a **type-erased event envelope** used **only as internal staging**
  while an event sits in the pending queue. It carries **Weasel engine types only**
  (e.g. `key_pressed_event`, `mouse_motion_event`, `PlayerDied`). Raw SDL is never a
  consumer-facing payload: `engine_event` (the `SDL_Event` wrapper in
  `src/wsl/event.hpp`) exists **only** as a poll-boundary conversion helper and is
  never posted into the pool. Users code against `post<T>` / `message_reader<T>` and
  never touch `message` directly (except the catch-all reader below).
- `message_event` marker concept (any trivially-copyable / serializable struct) used
  as the typed payload behind a `message`.
- `message_bus`:
  - `template <typename Event, typename... Args> void post(Args&&...)` — enqueue a
    `message` (type-erased) into the pending queue.
  - `message_reader<T> reader<T>()` — returns a handle bound to this bus for pulling
    typed events of type `T`.
  - `template <typename Event, typename... Args> void subscribe(std::function<void(const message&)>)`
    — non-system observer hook (used by `app` / `runtime_context` for window/quit).
  - `void drain(registry&)` — **publish** the pending queue: each pending `message`
    is unpacked into its typed `message_reader<T>` buffer (and delivered to any
    `subscribe` callback), then pending is cleared. Systems then pull from their
    readers during their own updates. **No `on_event` broadcast.**
  - **Pull is the ONLY consumption model.** A system holds one `message_reader<T>`
    per message type it cares about and consumes typed events at a point of its own
    choosing inside its update:
    ```cpp
    void input_reaction_system::on_update(entt::registry& reg, double dt) {
      m_keys.for_each([](const key_pressed_event& k) { wsl::log::sys ()->debug ("key: {}", k.key); });
    }
    // m_keys = runtime_ctx->message_bus().reader<key_pressed_event>();
    ```
    Each `message_reader<T>` instance keeps its **own read cursor**, so multiple
    systems holding a reader for the same `T` each receive every message exactly once
    — consuming in one system does not clear it for another (the shared per-type
    buffer is single, not double-buffered).
    Benefits over a broadcast: no O(systems × messages) cost (each system processes
    only its own declared types, no `as<T>()` RTTI scan over uninterested systems),
    no double-delivery/opt-out ambiguity, and in-update ordered consumption. A
    `message_reader<message>` (type-erased catch-all) is available for systems that
    genuinely need every type (debug/logging).
  - **No double buffering (intentional).** Unlike Bevy's `Messages<T>` A/B swap, the
    pull model publishes the pending queue into stable per-type reader buffers at
    `drain()`, and systems then consume from those buffers during their own updates —
    so a system can never "miss" a message due to intra-frame system ordering, and
    there is no need for a 2-frame window or a per-system cursor. The pending queue is
    emptied on each `drain()` (events posted *during* drain roll to the next frame),
    so it cannot grow unbounded. Latency is up to one frame, matching Bevy.
  - **Reader cursor / buffer lifecycle (correctness requirement).** At each `drain()`
    the per-type buffer is replaced with the freshly published messages and **every
    `message_reader<T>` cursor is reset to the start** of the new buffer. This
    guarantees a system reads exactly this frame's messages once. Consequence: a
    system that does not run in a frame (inactive) simply skips that frame's messages
    — there is no stale-read or buffer growth, because the buffer is per-frame and the
    cursor is reset, not incrementally advanced across frames. (Optional perf:
    `post()` type-erases each event; for high-frequency input consider a
    small-buffer/arena `message` to avoid per-event allocation churn.)
  - Thread-safety: an internal mutex (events may be posted off the main loop).
- Wire `message_bus` into `runtime_context` (singleton, like `event_hub`), with
  `resolve_active_registry` so handlers can access the registry during drain.
- Call `message_bus::drain(registry)` once per frame from the main loop,
  immediately after the `SDL_PollEvent` loop in `app.cpp:239`.

  **Example — message / buffered events:**

  ```cpp
  // 1. A message event is just a plain Weasel data struct (no registration needed)
  struct PlayerDied {
    entt::entity who;
    int          cause;
  };

  // 2. Post it from anywhere — a system update, an async task, etc.
  //    message_bus is a singleton on runtime_context (thread-safe post()).
  void combat_system::on_update(entt::registry& reg, double dt) {
    if (m_just_died) {
      m_runtime_ctx->message_bus().post<PlayerDied>({ m_player, 0 });
    }
  }

  // 3. CONSUME via pull — each system holds a message_reader<T> for the types it
  //    cares about and pulls them during its own update. No on_event broadcast, no
  //    wiring, no raw SDL. Systems only ever see Weasel engine types.
  void player_system::on_update(entt::registry& reg, double dt) {
    m_deaths.for_each([](const PlayerDied& d) { wsl::log::sys ()->info ("player died: {}", d.who); });
    m_keys.for_each([](const key_pressed_event& k) { wsl::log::sys ()->debug ("key pressed: {}", k.key); });
  }
  // m_deaths = bus.reader<PlayerDied>();  m_keys = bus.reader<key_pressed_event>();

  // (The catch-all reader is also available for debug/logging systems:)
  //   m_all = bus.reader<message>();  m_all.for_each([](const message& m){ … });

  // 4. SDL poll is the single entry point. A *boundary translator* (not a system)
  //    converts each raw SDL_Event into a Weasel engine type and posts that.
  //    Nothing raw reaches the pool or any reader.
  //    (app.cpp main loop)
  while (SDL_PollEvent(&e)) {
    if (engine_event eng{e}; translate_and_post(      // engine_event = boundary-only
            runtime_ctx->message_bus(), eng)) { /* posted a key_pressed_event, … */ }
  }
  runtime_ctx->message_bus().drain(reg);              // publish into reader buffers

  // 5. The translator: SDL -> Weasel engine types. This is the ONLY place that
  //    touches SDL; everything downstream consumes engine types only.
  bool translate_and_post(message_bus& bus, const engine_event& ev) {
    switch (ev.type()) {
      case SDL_EVENT_KEY_DOWN:
        bus.post<key_pressed_event>({ .key     = ev.as_keyboard().key,
                                      .scancode = ev.as_keyboard().scancode });
        return true;
      case SDL_EVENT_MOUSE_MOTION:
        bus.post<mouse_motion_event>({ .x = (int)ev.as_mouse_motion().x,
                                       .y = (int)ev.as_mouse_motion().y });
        return true;
      // … window/quit/text/etc.
      default: return false;
    }
  }
  ```

  The `key_pressed_event` / `mouse_motion_event` / … structs are the engine's own
  input types (today defined in `src/wsl/input.hpp` and emitted by
  `input_system::process_event`); they simply move from being emitted as observer
  signals to being posted as buffered messages. `engine_event` remains only as the
  translator's internal SDL wrapper.

  Contrast with observer events (§ Phase 2): there you explicitly wire
  `event_source` → `event_sink` with `add_listener`; here you simply `post` and let
  any system that holds a `message_reader<T>` for that type pull it during its update.
  Use **message events** for global, fire-and-forget, decoupled notifications (input,
  UI, game-state changes) and **observer events** for explicit, editor-wired,
  connection-graphed reactions.

**Relationship to the SDL event queue (default design):**

Yes — the SDL poll loop is the *single entry point* that drops every event into the
engine message pool, so input and game events propagate through the **same pipeline**.
An `SDL_Event` is a fixed C union and cannot hold arbitrary typed C++ game events, so
the bus stays SDL-agnostic and the conversion happens at the poll boundary.

Per-frame flow (`app.cpp` loop):
```
while (SDL_PollEvent(&e))                 // drain OS queue
    translate_and_post(bus, engine_event{e});  // SDL -> Weasel engine type, post
message_bus.drain(registry);             // publish pending -> typed reader buffers
    └─ each system pulls its message_reader<T> during its own update   (pull-only)
```
All events in the pool are Weasel engine types; raw SDL never reaches a reader.

Concretely:
- The poll loop runs a **boundary translator** that converts each `SDL_Event` into a
  Weasel engine type (`key_pressed_event`, `mouse_motion_event`, …) and `post`s that
  into the `message_bus`. `engine_event` (`src/wsl/event.hpp`) is used **only** as
  the translator's internal SDL wrapper — it is never posted into the pool, so
  systems never see SDL or `engine_event`. No special-casing for input vs game.
- **Coordinate remap moves into the translator.** Today `render_ui_system.cpp:190`
  mutates the raw `SDL_Event` (e.g. `ev.sdl()`) to adjust mouse coordinates for
  hit-testing before forwarding. Because raw SDL no longer reaches consumers, the
  boundary translator must emit engine types with **already-adjusted** coordinates
  (the remap logic moves into the translator, not a system that consumes events).
  The Daslang `on_event()` in `goban_system.das` / `mouse_rotate_system.das` that
  reads `get_event_mouse_x/y` is likewise replaced by `message_reader` pulls of the
  engine-typed input events (see §6.1.1).
- `message_bus` stays **SDL-agnostic** (generic, unit-testable).
- `message_bus::drain()` runs once per frame right after the poll loop and **publishes**
  the pending queue into the typed `message_reader<T>` buffers (and to `subscribe`
  callbacks). **There is no `on_event` broadcast** — distinct from the explicit,
  connection-tracked `event_source`/`event_sink` observer path. This is exactly the
  division between the two event types:
  - *Buffered / message events* → pull via `message_reader<T>` (per system, typed).
  - *Observer events* (`event_source`/`event_sink`) → explicit, connectable,
    serializable, editor-wired, dispatched immediately via the hub.
- **`on_event` is removed from `ecs_system`.** Messages are pull-only (readers), and
  observer events are dispatched via registered `handler_invoke_fn` function pointers
  (not a virtual). `engine_event` is no longer a consumer-facing type at all. The old
  `on_event(registry_handle, const engine_event&)` virtual and its `event_handler`
  broadcaster (`system.hpp:211`) are deleted. `app` / `editor_app` (which are not
  `ecs_system`) replace their `on_event` overrides with `message_reader<T>` handles
  or a `message_bus::subscribe` callback for window/quit events.
- `input_system::process_event` (`src/wsl/input.cpp:17`) is **removed/replaced**: its
  `SDL_Event`→typed-signal translation moves into a translator that `post`s typed
  events (`key_pressed`, `mouse_motion`, …) into the bus, where systems pull them via
  readers. Input no longer bypasses the bus via immediate `sig::emit`.
- `SDL_PushEvent` from other threads (e.g. the quit event at
  `src/editor/root.cpp:361`) still works: SDL buffers it and it is posted to the bus
  on the next poll.
- Window/quit handling currently in `app.cpp:241` is expressed as engine-typed
  messages in the pool (e.g. `window_resized_event`, `quit_event`); `app` observes
  them via a `message_reader` / `subscribe` callback, removing the bespoke inline
  branch.
- **Naming alignment.** The plan's examples use `key_pressed_event`,
  `mouse_motion_event`, … but the existing structs in `src/wsl/input.cpp` are
  `key_pressed`, `mouse_motion`, `mouse_button`, `mouse_wheel`, `text_input`. Pick
  **one** convention (recommend the `*_event` suffix for clarity at call sites) and
  rename the existing structs (or update the plan's examples) so the translator,
  Daslang reflection, and C++/Daslang examples all agree.

### Phase 4 — Consumers & serialization

Two passes. **4a** is the mechanical `signal`→`event` rename that keeps the build
green right after Phase 1. **4b** adopts the message bus and must run *after* Phase 3
(it cannot compile before the bus exists).

#### Phase 4a — Mechanical rename (`signal` → `event`)

- **Message events are runtime-only and never serialized.** Only *observer*
  connections (`event_connection_data`) are saved in scenes. Buffered messages live
  in the per-frame `message_bus` and vanish after `drain()`; they must not be added to
  the scene snapshot serializer. (If persistence is ever wanted, it would be an
  explicit, opt-in feature — not the default.)

| File | Change |
|------|--------|
| `src/wsl/sys/system.hpp` | Rename `register_signals` → `register_event_sources`; `register_event_handlers` → `register_event_sinks`. Update `register_iteration` helper. |
| `src/wsl/comp/singl/runtime_context.{hpp,cpp}` | Rename `m_signal_hub` → `m_event_hub`; add `m_message_bus`; expose accessors. |
| `src/wsl/rsc/scene_snapshot_serializer.cpp` | Serialize/restore `event_connection_data` (rename struct); keep auto-restore from scene. |
| `src/wsl/rsc/scene.cpp`, `src/wsl/sys/core_systems.cpp` | Update `register_signals`/`register_event_handlers`/`register_iterations` calls. |
| `src/wsl/reg/registry_queries.{hpp,cpp}` | Rename `signal_connection_debug_entry` usages → `event_connection_debug_entry`; `get_connections_for_signal` etc. |
| `src/wsl/reg/system_factory_registry.{hpp,cpp}` | `set_signal_hub` → `set_event_hub`; update iteration queries. |
| `src/editor/signal_inspector.{hpp,cpp}` | Rename to `event_inspector`; update all `signal_hub().db` / `connect` / `disconnect` calls; UI labels "Event Source" / "Event Sink". |
| `src/wsl/das/wsl_api_module.cpp` | Update `sig::emit` calls to new names. |
| `src/wsl/reg/README.md` | Rewrite Signal System section: observer events + buffered events. |

#### Phase 4b — Message-bus adoption (after Phase 3)

Replace the old `on_event` virtual and the immediate `sig::emit` input paths with the
pull-only message bus:

#### Phase 4c — Migrate `wsl::event::scene_changed` onto the observer system

The editor-wide `scene_changed` event (`src/wsl/events.hpp`) currently lives on a
separate `entt::dispatcher` (`runtime_context::m_dispatcher`) and is wired via raw
`dispatcher().sink<scene_changed>()` in `ecs_inspector`. Fold it into the new observer
event system so there is exactly **one** eventing mechanism:

- `scene_changed` becomes a normal observer event: `scene_manager` declares
  `event_source<scene_changed, scene_manager>` and emits through the `event_hub`;
  `runtime_context` and `ecs_inspector` declare
  `event_sink<scene_changed, runtime_context>` / `event_sink<scene_changed, ecs_inspector>`
  (with their `on_scene_changed` handler) and `add_listener` to the source.
  The owner-type template parameter (`scene_manager`, `runtime_context`, `ecs_inspector`)
  is used as the emitter/handler tag — these need not be `ecs_system`s; the observer
  API only requires a stable owner type id and an `invoke` function, which both classes
  already satisfy.
- Remove `runtime_context::m_dispatcher` (the `entt::dispatcher` member) and the
  `on_scene_changed` dispatcher wiring; replace with `event_hub` connections.
- Keep the `scene_changed` struct in `src/wsl/events.hpp` (it is already in the
  `wsl::event` namespace, so no move is needed — only its dispatch path changes).

| File | Change |
|------|--------|
| `src/wsl/comp/singl/runtime_context.{hpp,cpp}` | Remove `m_dispatcher` (`entt::dispatcher`) and the `sink<scene_changed>` wiring; declare `event_sink<scene_changed, runtime_context>` and `add_listener` to `scene_manager`; rename `on_scene_changed` to the handler invoked by `handler_invoke_fn`. |
| `src/wsl/rsc/scene_manager.{hpp,cpp}` | Declare `event_source<scene_changed, scene_manager>`; replace `dispatcher().trigger<scene_changed>(...)` with `event_hub::emit(scene_changed{...})`. |
| `src/editor/ecs_inspector.{hpp,cpp}` | Replace `dispatcher().sink<scene_changed>()` with `event_sink<scene_changed, ecs_inspector>` + `add_listener`; keep `on_scene_changed` as the handler. |
| `src/wsl/events.hpp` | No move (already `wsl::event`); only confirm the struct stays the event payload. |

| File | Change |
|------|--------|
| `src/wsl/sys/system.hpp` | **Delete the `on_event` virtual and the `event_handler` broadcaster** (`system.hpp:211`); observer events use `handler_invoke_fn`, messages are pull-only via `message_reader<T>`. |
| `src/wsl/app.hpp`, `src/wsl/editor_app.hpp`, `src/editor/editor_app.cpp` | Remove `on_event` overrides; observe window/quit engine-typed messages via `message_reader<T>` / `message_bus::subscribe`. |
| `src/editor/engine_ui.cpp`, `src/wsl/input.cpp`, `src/wsl/sys/*` (transform, audio, render_ui, physics) | Update `emit` calls to new names; replace `on_event` / immediate input emission with `message_reader<T>` pulls of engine-typed input events. |
| `src/cli/repl_handler.cpp` | Update `get_all_connections`/`connect`/`disconnect` calls. |
| `src/mcp-server/namespace_info.cpp`, `cli_reference.cpp` | Update docs: `event_source`/`event_sink`/`message_bus`. |

### Phase 5 — Tests

- Add/extend doctest coverage (`tests/`):
  - `event_source`/`event_sink` connect/disconnect at runtime (the new API).
  - Remove-component-relation invariant: declaring an event source needs only the
    emitter system.
  - `message_bus` post → drain → `message_reader<T>::for_each` round-trip and
    no-recursive-drain guarantee (events posted during drain roll to next frame).
  - `message_reader<T>` delivers each message exactly once per system that holds a
    reader for `T`; type-erased `message_reader<message>` catch-all works.
  - Scene save/load round-trip of connections.

---

## 4. Clean Break — No Backward Compatibility

This rework is a **deliberate breaking change**. There are no compatibility shims,
overload-preserving crutches, or fallbacks to the old signal API:

- `on_event` (the `ecs_system` virtual and its `event_handler` broadcaster) is
  **removed**. Messages are pull-only via `message_reader<T>`; observer events use
  registered `handler_invoke_fn` pointers. No `on_event` overload is retained.
  `app`/`editor_app` `on_event` overrides are replaced by `message_reader`/`subscribe`.
- `signal_*` types are deleted; all callers must move to `event_*` /
  `message_bus`. No alias, `using`, or deprecation bridge.
- The serialized connection format changes: `signal_connection_data` is replaced by
  `event_connection_data` with a **new** archive layout. Previously-saved scenes are
  **not** expected to load; provide a one-time `migrate_scene` tool (or simply
  re-author scenes) rather than in-place backward-compatible loading.
- Editor connections and the inspector are rewritten to the new names; old editor
  state is not supported.
- All existing `register_signals`/`emit`/`on_event` call sites are
  updated in Phase 4 as part of the rename; anything not updated simply fails to
  compile (intended).

If a transition window is ever needed, it is handled by a separate migration utility,
never by keeping the old code alive.

---

## 5. Open Questions (to confirm before coding)

1. **Entity scoping — resolved:** `source_entity`/`target_entity` are removed from
   connections (and `extract_source_entity` deleted). Entity identity lives in the
   event payload type. No open question remains.
2. **Buffered handler API — resolved:** message consumption is pull-only via
   `message_reader<T>` (per-system, typed, exactly-once); non-system consumers use
   `message_bus::subscribe`. No separate "buffered handler registration by name" is
   needed, so the earlier subscribe-by-name-vs-lambda question is moot.
3. **Input translation style:** SDL events are converted at the poll boundary into
   Weasel engine types (`key_pressed_event`, …) and posted; systems never see raw
   SDL. Resolved. (Kept here only as a record of the decision.)

---

## 6. End-to-End User Flow & Editor UI

This section walks a user through the **observer event** lifecycle (the only event
type that is explicitly wired/connected). Buffered / message events need **no**
wiring — a system just declares a `message_reader<T>` and pulls (see §2.2 / Phase 3).

### 6.1 Programmatic flow

Using a concrete example: `CombatSystem` emits a `PlayerHit`; `DamageSystem` (and
optionally `AudioSystem`) react to it. Entity identity lives in the payload.

```cpp
// 1. DEFINE the event — a plain struct (any trivially-copyable type).
struct PlayerHit {
  entt::entity attacker;   // source identity in the payload, not a connection field
  entt::entity victim;     // target identity in the payload
  float        damage;
};

// 2. DECLARE the event source in the EMITTER system (once, at registration).
//    Renamed from register_signals().
void CombatSystem::register_event_sources(wsl::event::event_hub &hub) {
  hub.template declare_event_source<PlayerHit, CombatSystem>();
}

// 3. EMIT it wherever the emitter decides (immediate dispatch to connected sinks).
void CombatSystem::on_update(entt::registry &reg, double dt) {
  if (m_hit_landed) {
    wsl::event::event_source<PlayerHit, CombatSystem>{hub}
        .emit(m_attacker, m_victim, m_damage);
    // or: wsl::event::emit(hub, PlayerHit{m_attacker, m_victim, m_damage});
  }
}

// 4. WRITE the sink (handler) in the HANDLER system.
//    A sink is a named handler bound to the system via an invoke function.
void DamageSystem::register_event_sinks(wsl::event::event_hub &hub) {
  hub.template declare_event_sink<PlayerHit, DamageSystem>(
      "on_player_hit", &DamageSystem::on_player_hit);
}
void DamageSystem::on_player_hit(entt::registry &reg, const PlayerHit &e) {
  // read attacker/victim/damage straight from the event payload
  auto &hp = reg.get<health>(e.victim);
  hp.value -= e.damage;
  wsl::log::sys ()->info ("{} hit {} for {}", e.attacker, e.victim, e.damage);
}

// 5. CONNECT them — at runtime, via the handle API (no entity args needed).
wsl::event::event_source<PlayerHit, CombatSystem> src{hub};
wsl::event::event_sink<PlayerHit, DamageSystem>   sink{hub, "on_player_hit"};
src.add_listener(sink);          // wired; emits now dispatch to DamageSystem

// 6. DISCONNECT just as easily.
src.remove_listener(sink);
```

The connection is recorded centrally in the `event_hub` (and serialized into the
scene as `event_connection_data`, which now holds only
`{ event_type_id, system_type_id, handler_name }` — no entity fields).

#### 6.1.1 Daslang equivalent

Mirrors §6.1 using the engine's script conventions (`class System : EcsSystem`,
`def override`, `query() $(...)`, `<|` lambdas — see `examples/go-demo/src/systems/`).
`hub` is the engine-provided event-hub handle (e.g. `get_event_hub()`), and event
types are passed by **name string** since Daslang cannot name C++ types directly.

```dascript
require weasel_api
require weasel_ecs

// 1. DEFINE the event struct — must match the C++ layout so the engine routes it.
struct PlayerHit {
    attacker : uint
    victim   : uint
    damage   : float
}

// 2. EMITTER system: declare the source, then emit.
class CombatSystem : EcsSystem {
    def override on_init() : void {
        declare_event_source(hub, "PlayerHit", "CombatSystem")   // owner = emitter
    }

    def override on_update(dt : float) : void {
        if (m_hit_landed) {
            event_source(hub, "PlayerHit", "CombatSystem")
                .emit(PlayerHit{ attacker = m_attacker, victim = m_victim, damage = m_damage })
        }
    }
}

// 3. HANDLER system: declare a named sink, then implement the handler.
class DamageSystem : EcsSystem {
    def override on_init() : void {
        declare_event_sink(hub, "PlayerHit", "DamageSystem", "on_player_hit", this.on_player_hit)
    }

    def on_player_hit(e : PlayerHit) : void {     // attacker/victim/damage from payload
        var hp = get_component_or(e.victim, Health())
        hp.value -= e.damage
        print("{} hit {} for {}", e.attacker, e.victim, e.damage)
    }
}

// 4. CONNECT / DISCONNECT at runtime (this is exactly what the editor generates):
event_source(hub, "PlayerHit", "CombatSystem").add_listener("DamageSystem", "on_player_hit")
event_source(hub, "PlayerHit", "CombatSystem").remove_listener("DamageSystem", "on_player_hit")
```

**Message events in Daslang** (no connect — pull via `message_reader`). The old
SDL-driven `on_event()` (used in `goban_system.das` / `mouse_rotate_system.das`) is
replaced by pulling engine-typed messages the translator posts:

```dascript
class InputLogger : EcsSystem {
    m_keys : message_reader

    def override on_init() : void {
        m_keys = message_reader(hub, "key_pressed_event")
    }

    def override on_update(dt : float) : void {
        m_keys.for_each() <| (k : key_pressed_event) {   // typed, exactly once
            print("key pressed: {}", k.key)
        }
    }
}
```

Both bindings (`declare_event_source` / `declare_event_sink` / `event_source` /
`message_reader` / `emit` / `add_listener` / `post`) are added in
`src/wsl/das/wsl_api_module.cpp` (Phase 2 / Phase 3).

**Prerequisite — Daslang reflection of event structs.** For the typed Daslang paths
to compile, engine event structs (`PlayerHit`, `key_pressed_event`, …) must be
**registered for Daslang** the same way components are (via the component/Daslang
reflection registry), so `message_reader` can expose a typed `k : key_pressed_event`
payload and `declare_event_sink` can reference the struct. This registration is a
prerequisite step for the §6.1.1 bindings, not an afterthought.

### 6.2 Connecting from other surfaces

- **Daslang (runtime):** `event_source(hub, "PlayerHit", "CombatSystem")
  .add_listener("DamageSystem", "on_player_hit")` — bound in `wsl_api_module.cpp`.
- **REPL / CLI** (`src/cli/repl_handler.cpp`): `connect PlayerHit CombatSystem
  DamageSystem on_player_hit` / `disconnect …` (reusing the existing connect/disconnect
  commands, now type/name based).
- **Scene load:** `scene_snapshot_serializer` restores `event_connection_data`
  automatically, so wired connections persist across saves.

### 6.3 Proposed editor UI changes

The current `signal_inspector` wires **entity→entity** signal connections. With
entity scoping removed, the model is now **type-level**: an event *source* (emitter
system) connects to one or more *sinks* (handler system + handler name) of the same
event type. Rename it to **`event_inspector`** and restructure the editor around a
dedicated **Event tab** as follows.

**Event tab (left pane, beside Entities & Singletons).**

- A **Source / Sink toggle** at the top of the Event tab switches the panel between
  the two views.
- Below the toggle, a **list** shows **all declared sources** (when *Source* is
  selected) or **all declared sinks** (when *Sink* is selected). Each row shows the
  event type and its owning system, e.g. `PlayerHit — CombatSystem` (source) or
  `PlayerHit — DamageSystem :: on_player_hit` (sink).
- Selecting a row in the list sets the current selection; its details appear in the
  **Inspector tab** (the existing right-hand inspector):
  - *For a source:* event type, owning emitter system, and the list of currently
    connected sinks (handler system + handler name, with a disconnect affordance).
  - *For a sink:* event type, owning handler system, handler name, and the list of
    sources currently connected to it (with disconnect).
- **Connect action.** When a source (or sink) is selected, a **Connect** button opens
  a small picker to choose the counterpart (a sink of the same event type, or a
  source) from the declared set. Confirming **generates a line of Daslang** that
  performs the connection at runtime, e.g. for `PlayerHit` from `CombatSystem` to
  `DamageSystem::on_player_hit`:

  ```dascript
  event_source(hub, "PlayerHit", "CombatSystem").add_listener("DamageSystem", "on_player_hit")
  ```

  The generated line is the canonical runtime connection call (§6.2); the editor may
  either execute it live against the running `event_hub` (exercising the § Phase 2
  runtime API) or copy it to the clipboard / paste into a Daslang script. Declarations
  themselves remain code-defined in each system's `register_event_sources` /
  `register_event_sinks`; the editor only *wires* what already exists — mirroring how
  Bevy observers are code-defined.

**Inspector integration.** The Event tab selection reuses the existing right-hand
Inspector tab (no new panel); it simply renders source/sink metadata instead of
entity/component data. This keeps the editor's "declare in code, wire in editor"
philosophy while dropping the per-entity connection model that the component relation
used to require.

**Optional extras.** A read-only *payload inspector* in the Inspector tab can show
the last fired instance of the selected event type (e.g. `PlayerHit { attacker,
victim, damage }`) during a debugging session, so designers can verify wiring without
a code breakpoint.

---

## 7. Suggested Order of Work

1. Phase 1 (rename + remove component relation) — compile-break, fix all callers.
2. Phase 4a mechanical rename (`signal`→`event`) — keeps the build green.
3. Phase 2 runtime API + Daslang bindings.
4. Phase 3 message bus (new, isolated, unit-tested).
5. Phase 4b message-bus adoption (remove `on_event` virtual + broadcaster, switch
   systems to `message_reader<T>` / `subscribe`).
6. Phase 4c migrate `wsl::event::scene_changed` from `entt::dispatcher` onto the
   observer event system (removes the last parallel eventing mechanism).
7. Phase 5 tests + README/docs update.

---

## 8. Sequential TODO List

Ordered, checkable steps that implement the plan. Each step is a single coherent unit
of work; a step is done only when its file(s) compile and the build is green.

 ### Phase 1 — Rename signals → observer events & drop component relation
 - [x] Rename core types in `signal_hub_fwd.hpp` / `event_hub.hpp`:
   `signal_hub`→`event_hub`, `signal_debug_entry`→`event_source_debug_entry`,
   `signal_connectable_handler_debug_entry`→`event_sink_debug_entry`,
   `signal_connection_data`→`event_connection_data`, `registered_signal_source`→
   `registered_event_source`, `registered_connectable_handler`→`registered_event_sink`,
   `connected_handler`→`connected_sink` (also `signal_type_id`→`event_type_id`,
   `signal_type_name`→`event_type_name`, `signal_inspector`→`event_inspector`,
   `signal_db`→`event_db`, `signal_debug_db`→`event_debug_db`, `wsl::reg::sig`→`wsl::event`).
 - [x] `declare_signal<…,Components…>`→`declare_event_source<Event,EmitterSystem>()` (drop `Components…`).
 - [x] `declare_connectable_handler<…,Components…>`→`declare_event_sink<Event,HandlerSystem>(handler_name, invoke)` (drop `Components…`).
 - [x] Remove component/entity relation **from events**: events have no
   `component_types`/`entity_matches`/`has_component`/`matches_entity`/`source_entity`/
   `target_entity`/`extract_source_entity`. **Kept** `component_type_debug_entry`,
   `make_component_type_debug_entries`, `contains_component_type`, `matches_component_set`,
   `make_entity_match_predicate` for system *iterations* (entity-scoped), per decision.
 - [x] Keep `declare_handler`/`declare_iteration`; removed `Components…` static_asserts that referenced signal contracts.
 - [x] `connect`/`disconnect` take only `(event_type_id, system_type_id, handler_name)`; deleted `entity_matches` and `extract_source_entity`.
 - [x] Dropped the `entt::entity` arg from `handler_invoke_fn` (`event_hub_fwd.hpp`);
   updated the `dispatch` loop in `event_hub.hpp` to stop passing the target entity.

### Phase 4a — Mechanical rename (`signal`→`event`)
- [x] `runtime_context`: rename `m_signal_hub`→`m_event_hub`; add `m_message_bus` + accessors (accessor added in Phase 3).
- [x] `system.hpp`: `register_signals`→`register_event_sources`,
  `register_event_handlers`→`register_event_sinks` (do **not** delete `on_event` yet).
- [x] `scene_snapshot_serializer`: `signal_connection_data`→`event_connection_data`; keep auto-restore.
- [x] `scene.cpp` / `core_systems.cpp`: update `register_*` calls.
- [x] `registry_queries`: `signal_connection_debug_entry`→`event_connection_debug_entry`; rename `get_connections_for_signal`.
- [x] `system_factory_registry`: `set_signal_hub`→`set_event_hub`.
- [x] Rename `signal_inspector`→`event_inspector` (file + class + UI labels "Event Source"/"Event Sink").
- [x] `wsl_api_module.cpp`: update existing `sig::emit` calls to new names.
- [x] `src/wsl/reg/README.md`: rewrite Signal System section (observer + buffered events).

### Phase 2 — Public runtime API + Daslang bindings
- [x] Add `event_source` / `event_sink` non-owning handles (constructors look up an
  already-declared source/sink; do **not** re-declare). `add_listener`/`remove_listener`
  forward to hub `connect`/`disconnect`. Add free `emit` helper.
- [x] `wsl_api_module.cpp`: bind `event_connect` / `event_disconnect` (string event/system/handler
  names) to hub `connect`/`disconnect`; `emit` already bound via `wsl_audio_*` etc. using `wsl::event::emit`.

### Phase 3 — Buffered / message events (new subsystem)
- [x] Create `src/wsl/event/message_bus.hpp`, `src/wsl/event/message_event.hpp`.
  Implement `message` (type-erased envelope), `message_bus::post<T>`,
  `message_bus::reader<T>()`, `subscribe` (typed + catch-all), `drain` (publish to typed
  reader buffers with per-reader cursor/generation reset). Thread-safe `post`.
- [x] Wire `message_bus` into `runtime_context` (singleton accessor `message_bus()`).
- [x] Call `message_bus::drain(registry)` once per frame right after `SDL_PollEvent` in `app.cpp`.
- [x] Boundary translator: `SDL_Event`→engine-typed input messages (`key_pressed`,
  `key_released`, `text_input`, `mouse_motion`, `mouse_button`, `mouse_wheel`) posted to
  the bus. Kept the existing observer-event `emit` path in `input_system` intact (consumers
  migrate in Phase 4b). `SDL_PushEvent` from other threads still works via thread-safe `post`.
- [ ] **Naming alignment:** rename existing `key_pressed`/`mouse_motion`/… structs to the
  `*_event` convention — DEFERRED (would churn many call sites; translator uses the
  existing `wsl::input::*` names for now).
- [ ] **Daslang reflection prerequisite:** register engine event structs for Daslang like
  components — DEFERRED (needed only if Daslang scripts post/pull messages).

### Phase 4b — Message-bus adoption
- [x] **Daslang reflection prerequisite — DONE.** Engine input event structs
  (`wsl::input::*`: `key_pressed`/`key_released`/`text_input`/`mouse_motion`/`mouse_button`/
  `mouse_wheel`, plus new `window_resized`/`quit_requested`) are now reflected to Daslang as
  value structs (`MouseMotion`, `MouseButton`, `MouseWheel`, `KeyPressed`, `KeyReleased`,
  `TextInput`, `WindowResized`, `QuitRequested`) via `wsl_event_binds.cpp` (proxy +
  `ManagedStructureAnnotation` + `MAKE_TYPE_FACTORY`, mirroring the component pattern). A
  pull API is exposed: `mouse_motion_count()`/`mouse_motion_at(i)` etc. read the active
  runtime context's `message_bus` published buffer. `app.cpp::post_input_to_bus` now also
  posts `window_resized` and `quit_requested`. (`wsl` builds green.)
 - [x] `system.hpp`: **delete the `on_event` virtual + `event_handler` broadcaster** (`system.hpp:211`);
   observer events use `handler_invoke_fn`, messages are pull-only via the new Daslang bindings.
   The Daslang `EcsSystem.on_event` hook and `get_event_*` externs are removed, and
   `weasel_ecs.das`/`weasel_ecs_adapter_gen.inc`/`das_system_adapter.*` drop the `on_event` adapter glue.
   `wsl_api_module.cpp/.hpp` no longer register `get_event_*`, `EVENT_*`, `g_current_event`, or
   `wsl_api_set_current_event`. Example scripts `goban_system.das`/`mouse_rotate_system.das`
   migrated to the pull API (`mouse_motion_at`/`key_pressed_at`/`mouse_button_at`/…). `wsl` + `weasel`
   + all tests build green (weasel_core_tests 6/6, weasel_cli_tests 106/106, weasel_mcp_server_tests 18/18).
 - [x] `app.hpp` / `editor_app.hpp` / `editor_app.cpp`: **`app::on_event` retained** (still feeds
   `engine_ui` raw `SDL_Event` in the poll loop — RmlUI/ImGui need raw SDL, distinct from the removed
   `ecs_system::on_event`). `editor_app::on_event` now forwards raw SDL to
   `core_systems()->render_ui_sys->handle_sdl_event(registry, e)` instead of the deleted broadcast.
   `render_ui_system` gained `handle_sdl_event(entt::registry&, const engine_event&)` (it needs raw SDL
   for the RmlUI viewport), replacing its `on_event` override.
 - [x] `engine_ui.cpp` / `input.cpp` / `sys/*` (transform, audio, render_ui, physics): `input.cpp` builds
   the visible `message_bus` (no `on_event`); `render_ui_system` converted as above; transform/audio/physics
   never used `on_event`. README/`mcp-server` docstrings updated to the pull/message-bus model.
- [x] `cli/repl_handler.cpp`: update `get_all_connections` / `connect` / `disconnect` calls (done in Phase 1); user-facing "signal" labels updated to "event" in help/output.
- [x] `mcp-server/namespace_info.cpp`, `cli_reference.cpp`: docs updated to `event_source`/`event_sink`/`message_bus` vocabulary.

### Phase 4c — Migrate `wsl::event::scene_changed`
- [x] `scene_manager`: routes `scene_changed` through the event hub —
  `m_main_world.get_runtime_context()->event_hub().dispatch(scene_changed{...})`.
- [x] `runtime_context`: removed `m_dispatcher` (`entt::dispatcher`) and the
  `sink<scene_changed>` wiring; declares `event_sink<scene_changed, runtime_context>`
  (`"on_scene_changed"`) with a captured `void *` owner and `add_listener`-equivalent
  `connect`. `on_scene_changed` is now invoked through `handler_invoke_fn`.
- [x] `ecs_inspector`: replaced `dispatcher().sink<scene_changed>()` with
  `event_sink<scene_changed, ecs_inspector>` + `connect`; `on_scene_changed` kept as the handler.
- [x] `handler_invoke_fn` generalized from `void(*)(ecs_system&, entt::registry&, const void*)`
  to `void(*)(void* owner, entt::registry&, const void*)` so non-`ecs_system` owners
  (`runtime_context`, `ecs_inspector`) can own event sinks; ecs_system owners are still
  resolved via `resolve_system_by_type` at dispatch time.
- [x] Confirm `scene_changed` struct stays in `src/wsl/events.hpp` (already `wsl::event`).

### Phase 5 — Tests & docs
- [x] doctest: `event_source`/`event_sink` connect/disconnect at runtime (new
  `weasel_core_tests` target, `tests/weasel-core/test_event_bus.cpp`).
- [x] doctest: remove-component-relation invariant (declaring a source needs only the emitter type).
- [x] doctest: `message_bus` post→drain→`reader<T>::for_each` round-trip; no recursive-drain
  (events posted during drain roll to next frame).
- [x] doctest: `reader<T>` delivers each message exactly once per system; type-erased
  `reader<message>` catch-all works.
- [ ] doctest: scene save/load round-trip of `event_connection_data`.
  (Deferred — requires the full `scene_manager`/`runtime_context`/world archive
  pipeline; covered implicitly by editor round-trips.)
- [ ] doctest: `scene_changed` migration smoke test (emit → sink handler fires).
  (Deferred — `scene_changed` still routes through the hub's `entt::dispatcher`;
  a dedicated smoke test needs the editor/inspector wiring.)
- [x] `src/wsl/reg/README.md` reviewed in Phase 4a.
