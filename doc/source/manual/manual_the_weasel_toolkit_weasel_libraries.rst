Weasel Libraries
=================

Weasel is built from a set of libraries that can be used together or
individually. Two libraries form the foundation of every Weasel project: the
core engine library and the Daslang bindings that expose it to scripting.

Weasel Core Library (libwsl)
----------------------------

The core library is also referred to as **libwsl**. It is written in **C++** and
is distributed as a **shared library** (``libwsl.so`` on Linux, ``wsl.dll`` on
Windows).

libwsl contains the entire Weasel API. It implements the low-level engine
systems and is where the bulk of the engine lives, including:

- The **Entity Component System** (ECS) used to model entities, world
  components, singleton components and systems.
- The **rendering** pipeline built on SDL3's GPU API (2D and 3D, PBR, lighting
  clusters, shader graph).
- **Physics** integration (3D through Jolt).
- **Audio**, **input**, **resources** and **scene** management.
- **Engine and editor abstractions** -- the data structures, registries and
  services that both the runtime and the Weasel Editor build upon.

Because libwsl is a plain C++ shared library, it can be linked directly into
native applications or consumed by higher-level tooling and bindings.

Weasel Daslang Bindings
-----------------------

The Daslang bindings wrap libwsl so that game and application logic can be
written in daScript. They are oriented toward **application-level** code: the
systems, components and glue that make up a playable project, rather than the
engine internals.

The bindings are split into the following modules:

- **weasel_ecs** -- the ECS system base class and helpers for writing daslang
  systems (``on_init``, ``on_update``, ``on_event``, ``on_inactive``) and
  declaring components.
- **weasel_api** -- the core engine API exposed to daScript: entity
  management, transforms, scene queries, events, window operations, raycasting
  and more.
- **weasel_helpers** -- convenience utilities and helpers that simplify common
  daslang-side tasks when working with the engine.

Together these modules let authors drive the full libwsl API from daScript
without dropping down into C++.

The Daslang bindings are the **main way to create user-defined components and
systems**: authors declare their components and implement system callbacks in
daScript rather than in C++.

daScript is **interpreted by default in the editor**, which enables fast
iteration -- changes to systems and scenes are picked up live without a
rebuild. For distribution, the same daslang code can be **compiled ahead of
time through CMake** into a native build. See
:doc:`Application Build & Playback <manual_application_build_playback>` for the
interpreted and AOT compilation workflows.
