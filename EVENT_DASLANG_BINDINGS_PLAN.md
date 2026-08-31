# Plan: Daslang Bindings for Remaining Event Operations

## Goal

Make the engine's two event models — **subscription events** (`wsl::event::event_hub`)
and **messages** (`wsl::event::message_bus`) — fully usable from daScript, so that
the documentation pages `manual_events_subscription_events.rst` and
`manual_events_messages.rst` can show true daslang equivalents for every C++
operation (today several are C++-only and marked with notes).

## TODO

- [ ] **Phase 0 — Infrastructure**
  - [x] Add type-erased, non-template `declare_event_source`/`declare_event_sink`/dispatch-by-id APIs to `wsl::event::event_hub`/`event_debug_db`, plus a `size_t size` field on `event_source_debug_entry` (single source of truth for name lookup + size)
  - [x] Add a name → type registry (`entt::id_type`, `size_t` size) — event sizes live on `event_source_debug_entry`; messages use a name-keyed registry in `wsl_event_binds.cpp` (`register_message_type`, name-hashed ids for script-defined types)
  - [x] Register C++-defined event/message types statically at module init (built-in input messages registered via `register_message_type`; C++ events populate the debug DB through their regular declare hooks)
  - [x] Add `event_declare_source(event_name, system_name) : bool` for C++-defined event types
- [ ] **Phase 1 — Emit**
  - [x] Add `event_emit(event_name, payload)` (byte copy via registry → `wsl::event::emit`)
- [ ] **Phase 2 — Script handlers**
  - [x] Extend `das_system_adapter` to store event-handler declarations (event → handler method name)
  - [x] Override `register_event_sources`/`register_event_sinks` on `das_system_adapter` to replay declarations on lifecycle re-registration
  - [x] Add `event_declare_sink(event_name, system_name, handler_name)` — resolved as a class-method name on the calling script system (no `handler_fn` argument needed; name-based resolution survives script reloads)
  - [x] Implement C++ `handler_invoke_fn` thunk → daScript handler dispatch
- [ ] **Phase 3 — Messages**
  - [x] Add `message_post(message_name, payload) : bool` (type-erased bus post via `post_erased`, size-guarded)
  - [x] Add generic pull API: `message_count(message_name)` + `for_each_message(message_name) $(payload : T&) { ... }` (a typed reader *handle* returning arbitrary structs by value is not expressible in a static binding, so count+block iteration replaces `.count()/.at(i)`)
- [ ] **Phase 4 — Non-system subscribe**
  - [x] Add `message_subscribe(message_name, @@fn)` / `message_unsubscribe(message_name)` with POD → daScript struct delivery; callbacks fire during `drain()` on the main thread; subscriptions auto-drop when the subscribing script context is destroyed (`wsl_api_on_context_destroyed` hooked into das_engine re-execute/shutdown)
- [ ] **Phase 5 — Docs**
  - [x] Remove "C++-only" notes on `manual_events_subscription_events.rst` and `manual_events_messages.rst`
  - [x] Add real daslang declare/emit/post/reader/subscribe examples
  - [x] Rebuild docs and confirm 0 manual/tutorial warnings
- [ ] **Tests**
  - [ ] Extend `tests/weasel-das` with a smoke test for the new bindings: das-declared
        source/sink → emit → handler receives payload → disconnect; message post →
        reader round-trip; layout/size-mismatch rejection; nested `event_emit` from
        inside a handler; declaration survival across a `register_debug_metadata`
        replay pass
- [x] **Open questions**
  - [x] Decide scope: ~~C++-defined types only~~ → both sides supported. Script-defined event/message types work end-to-end: unknown names synthesize name-hashed ids (first `event_emit` / `event_declare_source` / `event_declare_sink` for events; first `message_post` for messages), sizes latch on first use, sink-before-source declarations stay `event_connect`-resolvable, and everything replays through the existing lifecycle hooks.

## Current State (from code research)

### Subscription events — `src/wsl/event/event_hub.hpp`
Bound to daScript today (in `src/wsl/das/wsl_api_module.cpp:2308-2350`;
`resolve_event_type_id` matches names against `event_hub().db->entries`):
- `event_connect(event_name, system_name, handler_name) : bool`
- `event_disconnect(event_name, system_name, handler_name) : bool`

Emit precedent: `wsl_audio_play/stop/pause/resume` (`wsl_api_module.cpp:2352`)
already emit specific C++ events from daScript via hardcoded bindings;
`event_emit` generalizes this into a name-keyed path.

**Not** bound (C++-only):
- `event_hub::declare_event_source<Signal, OwnerSystem>()` — emitter declaration hook
- `event_hub::declare_event_sink<Signal, OwnerSystem>(name, invoke, owner)` — handler declaration
- `wsl::event::emit(hub, event)` / `event_source<...>::emit(...)` — dispatch

C++ systems declare sources/sinks through the virtual hooks
`sys::ecs_system::register_event_sources(event_hub&)` /
`register_event_sinks(event_hub&)` (`src/wsl/sys/system.hpp:42,46`), invoked by
`core_systems::register_debug_metadata()` after a
`clear_system_declarations(system_id)` wipe (`core_systems.cpp:101-113`). The
daScript system wrapper is `das_system_adapter`
(`src/wsl/das/das_system_adapter.{hpp,cpp}` — dual-inheritance `sys::ecs_system` +
generated `EcsSystemAdapter`, daScript-tutorial-19 class-adapter pattern); it
overrides `on_init`/`on_update`/`on_inactive` but has **no**
`register_event_sources/sinks` overrides today.

Two facts that shape the design:
- `event_hub::dispatch` also forwards to `entt::dispatcher::trigger`, but nothing
  in engine/editor subscribes via `entt::dispatcher` sinks (sole trigger site:
  `event_hub.hpp:670`). Byte-level dispatch can safely skip that leg — the
  connected-handler loop is already fully type-erased.
- All `declare_event_source/sink`/`dispatch` entry points on `event_hub` are C++
  templates on `Signal`/`OwnerSystem`; name-keyed bindings require new
  non-template, type-erased variants, plus a `size_t size` field on
  `event_source_debug_entry` (it currently stores only ids/names/counts).

`event_connect` resolves the target system by name via
`runtime_context::system_factory_registry().find_system(...)`, so daScript systems
*are* already registered there (via `register_cached_runtime_system`) and can be
connection targets/hosts in principle.

### Messages — `src/wsl/event/message_bus.hpp`
Bound to daScript today (in `src/wsl/das/wsl_event_binds.cpp`):
- `*_count()` / `*_at(i)` pull accessors **for the built-in input messages only**
  (`mouse_motion`, `mouse_button`, `mouse_wheel`, `key_pressed`, `key_released`,
  `text_input`, `window_resized`, `quit_requested`).

**Not** bound (C++-only):
- `message_bus::post<Event>(...)` — for **custom/user-defined** messages
- `message_bus::reader<Event>()` + `message_reader<T>::for_each(...)` — typed pull from script
- `message_bus::subscribe<Event>(cb)` — non-system consumer callbacks

Note: `message_bus` is already **type-erased** (`detail::erased_buffer` of raw bytes +
`element_size`), and `post<Event>` is just a `memcpy` of `sizeof(Event)`. This makes a
name-keyed, byte-level `post` feasible without per-type C++ template plumbing.

## Proposed Daslang API Surface (as shipped)

### Subscription events
```das
event_declare_source(event_name : string, system_name : string) : bool
event_declare_sink  (event_name : string, system_name : string,
                     handler_name : string) : bool   // class-method name on the calling script system
event_emit          (event_name : string, payload : EventStruct) : void
```
`event_connect` / `event_disconnect` stay as-is (they wire already-declared ends).

### Messages
```das
message_post      (name : string, payload : MessageStruct) : bool
message_count     (name : string) : int
for_each_message  (name : string) $(payload : T&) { ... }
message_subscribe (name : string, fn : function) : bool   // no-capture @@fn
message_unsubscribe(name : string) : bool
```
Built-in input accessors (`*_count`/`*_at`) stay as the convenience fast-path.

## Implementation Approach

### 1. Name → type registry (shared infrastructure)
Both `post`/`emit` need to turn a daScript struct (bytes) into a registered C++ type
id. Add a runtime table mapping an event/message **name** →
`(entt::id_type, size_t, copy/destroy fns)`, mirroring how
`resolve_event_type_id` already scans `event_hub::db->entries`. This lets all new
bindings reference engine- and script-defined types uniformly by string.

- For **C++-defined** types: register statically at module init via
  `comp::stable_type_id<T>()` + `sizeof(T)` (compare to the existing
  `MAKE_TYPE_FACTORY` / `register_event_message_bindings` pattern).
- For **daScript-defined** types: expose a registration helper (e.g. a
  `WSL_MESSAGE_TYPE` / `WSL_EVENT_TYPE` macro or a `register_message_type` function)
  that records the type id, size, and a trampoline. Validate layout at registration
  instead of trusting byte compatibility: reuse the script-component reflection
  already in `das_engine::impl` (`get_struct_info`, `classify_field_type`,
  `field_type_size` — `das_engine.cpp:673-772`) to fingerprint the das struct
  (field order/types/offsets) and fail-fast against the registered C++ POD's
  layout / `sizeof(T)` — the same fail-fast contract used when registering script
  world components. Note the built-in input messages deliberately avoid raw byte
  reinterpretation (proxy structs + getters); custom types get a validated
  byte-copy path instead.

### 2. `event_declare_source` / `event_declare_sink` (`wsl_api_module.cpp`)
Wrap the existing templated `declare_event_source` / `declare_event_sink` using the
name→type registry instead of C++ template args. `event_declare_sink` stores the
daScript `handler_fn` and installs a C++ `handler_invoke_fn` thunk
(`void(void* owner, entt::registry&, const void*)`) that:
- downcasts `owner` to the daScript `EcsSystem` instance,
- reinterprets the `const void*` payload as the registered message struct,
- invokes the daScript handler function.

This requires extending `das_system_adapter` (the actual "EcsSystem" wrapper) to
hold event-handler declarations — event id/name → handler *method name*, with a
lazily cached `adapt_field_offset` slot resolved once per script load — plus a
dispatch trampoline in `wsl_api_module.cpp`.

**Declaration lifecycle (critical):** `core_systems::register_debug_metadata()`
wipes each system's declarations (`clear_system_declarations`) and re-invokes the
hooks on scene reload / play-mode toggles. Imperative script-only declarations
would be silently dropped on the next pass. Therefore `das_system_adapter` must
override `register_event_sources` / `register_event_sinks` and **replay its
stored declarations there**, plugging into the existing lifecycle rather than
working around it. Handler functions resolve by name at dispatch time (exactly
like `on_update` via `get_on_update`/`invoke_on_update`), never retained as
`Func` pointers across script reloads. **This hook-replay mechanism is the most
important part of Phase 2** — without it the feature breaks on the first scene
reload.

### 3. `event_emit` (`wsl_api_module.cpp`)
Look up the type id + size from the registry, `memcpy` the daScript struct bytes into
a stack buffer, and dispatch through the new type-erased `event_hub` API (skipping
the unused `entt::dispatcher::trigger` leg). For C++-defined event types this
reuses the existing C++ type; for daScript-defined types it uses the registered
layout.

### 4. Messages: `message_post` / `message_reader` / `message_subscribe`
(`wsl_event_binds.cpp`)
- `message_post(name, payload)`: resolve type id + size, `memcpy` the daScript struct
  into the bus via a new type-erased post entry point
  (`wsl_api_get_active_message_bus()`; the bus already locks on post). Must guard
  size: the bus latches `element_size` on first post
  (`if (buf.element_size == 0)`, `message_bus.hpp:67`), so reject any post whose
  payload size ≠ registered element_size — otherwise two different structs sharing
  a name silently corrupt the buffer. (Today's templated `post<T>` cannot be reused
  for script types since it requires a real C++ type.)
- `message_reader(name)`: return a handle bound to a named type; expose `count()` and
  `at(i)` (copy bytes out into a daScript struct) and/or `for_each(cb)` mirroring the
  existing `wsl_mouse_button_at` accessors but generalized over the registry.
- `message_subscribe(name, cb)`: wrap the C++ `subscribe<Event>(cb)` with a thunk that
  converts the posted POD back into a daScript struct before invoking `cb`.

## Files to Touch
| File | Change |
|------|--------|
| `src/wsl/event/event_hub.hpp` | Non-template, type-erased `declare_event_source/sink` + dispatch-by-id (skip unused `entt::dispatcher` leg); add `size_t size` to `event_source_debug_entry` |
| `src/wsl/das/das_system_adapter.hpp` / `.cpp` | Store event-handler declarations; override `register_event_sources/sinks` to replay them on lifecycle re-registration |
| `src/wsl/das/wsl_api_module.cpp` | Add `event_declare_source`, `event_declare_sink`, `event_emit`; dispatch thunk resolving handler functions by name at invoke time |
| `src/wsl/das/wsl_event_binds.cpp` | Add `message_post`, generic `message_reader`, `message_subscribe`; name→type registry helpers |
| `src/wsl/event/message_bus.hpp` | Type-erased post entry point with element-size guard |
| `src/wsl/das/wsl_api_module.hpp` / `wsl_event_binds.hpp` | Declarations |
| `src/wsl/reg/system_factory_registry.hpp` | (reuse) system-by-name resolution for sink declaration |
| `doc/source/manual/manual_events_subscription_events.rst` | Drop C++-only notes; add daslang declare/emit examples |
| `doc/source/manual/manual_events_messages.rst` | Drop C++-only notes; add daslang post/reader/subscribe examples |

## Phasing
1. **Phase 0 — Infrastructure:** type-erased `declare_*`/dispatch-by-id APIs +
   `size` field on `event_hub`/`event_debug_db`; name→type registry backed by
   those entries; `event_declare_source` for C++-defined event types (no handler
   yet).
2. **Phase 1 — Emit:** `event_emit` for C++-defined events from daScript.
3. **Phase 2 — Script handlers:** extend `das_system_adapter` with handler
   declaration storage; override `register_event_sources/sinks` to replay
   declarations; `event_declare_sink` with a daScript `handler_fn`; wire dispatch
   thunk.
4. **Phase 3 — Messages:** `message_post` + generic `message_reader`/`for_each` for
   named message types (custom + built-in).
5. **Phase 4 — Non-system subscribe:** `message_subscribe` for app/runtime-context consumers.
6. **Phase 5 — Docs:** remove the "C++-only" notes added in the previous step and
   replace them with real daslang examples, rebuilding the docs to 0 warnings.

## Open Questions / Risks
- **Handler lifetime & VM context (decided):** store handler *method names* on
  `das_system_adapter`; replay declarations from the overridden
  `register_event_sinks` hook; resolve the function at dispatch time like
  `on_update`. Never retain `Func` pointers across script reloads.
- **Layout compatibility:** daScript struct must match the registered C++ POD.
  Fail fast at registration using the existing script-component reflection
  (`das_engine::impl::get_struct_info` / `classify_field_type` /
  `field_type_size`) instead of a post-hoc assert.
- **Post size guard:** reject posts whose payload size ≠ registered
  `element_size` (the bus latches element size on first post).
- **Threading & reentrancy:** `message_bus` locks internally, but
  `event_hub::dispatch` mutates/iterates connection vectors unlocked and invokes
  handlers synchronously; daScript contexts are not thread-safe and depend on
  `safe_invoke` stack watermarks. Contract: emit/post/handlers are
  main-thread-only. Cover nested `event_emit` from inside a handler (re-entry
  through watermarks) in the smoke test.
- **Declaration timing:** daScript types must be registered before `declare`/`post`/
  `emit` use them; for script types, register during module init or lazily on first use.
- **Scope decision (resolved):** both C++-defined and script-defined
  event/message types are supported. Script-defined types ride the same
  type-erased dispatch loop (nothing consumes `entt::dispatcher` sinks), a
  name-hashed id, and first-use size latching.

## Success Criteria
- Every C++ code block on the two event pages has a working daslang equivalent.
- `doc` build passes with 0 manual/tutorial warnings.
- New bindings covered by a small daScript smoke test (extend `tests/weasel-das`).
