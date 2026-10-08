# `wsl::phys` — Physics Engine

Physics is backed by [Box3D](https://github.com/erincatto/box2d) (v0.1.0,
pulled via `xmake.lua`). `phys::engine` is the public, backend-neutral API:
body creation, fixed-step simulation, collision queries, and sensor overlap
detection. Box3D headers and IDs never leak past the adapter.

```sh
xmake f --toolchain=clang
xmake build -j2 wsl
```

## Key Classes

| Class | Header | Description |
|-------|--------|-------------|
| `engine` | `physics_engine.hpp` | Public physics API. Owns the `box3d::world`, fixed-step accumulator, and sensor event queue. |
| `box3d::world` | `box3d_adapter.hpp` | Weasel-owned boundary over the Box3D C API. Exposes only value types and opaque `body_handle`s; Box3D headers stay private to `box3d_adapter.cpp`. |

## Backend-neutral types

```cpp
using body_id = std::uint64_t;
enum class motion_type;      // Static, Kinematic, Dynamic
enum class allowed_dofs;    // All, TranslationX, RotationZ, etc.
enum class shape_type;       // box, sphere
using object_layer = std::uint16_t;
```

New physics-facing code should use `phys::engine` and these Weasel-owned
types — never Box3D types directly.

## Usage

```cpp
#include <wsl/phys/physics_engine.hpp>

wsl::phys::engine physics;

// Configure
physics.set_gravity (-9.8);
physics.set_fixed_step (1.0 / 60.0);

// Body creation
wsl::phys::body_desc desc;
desc.shape = wsl::phys::shape_type::box;
desc.half_extents = { 0.5F, 0.5F, 0.5F };
desc.position = { 0.0F, 5.0F, 0.0F };
desc.motion = wsl::phys::motion_type::Dynamic;
wsl::phys::body_id id = physics.create_body (desc);

// Per-frame step
physics.step (dt);

// Read back state
wsl::phys::vector3 pos = physics.get_body_position (id);

// Sensor overlap events
physics.register_sensor (sensor_id);
std::vector<wsl::phys::sensor_overlap_event> events = physics.drain_sensor_events ();
for (auto &ev : events) {
    if (ev.entered) {
        // sensor hit something
    }
}

// Cleanup
physics.clear ();
```

## Box3D adapter

The adapter is the only translation layer between `phys::engine` and Box3D:

```cpp
#include <wsl/phys/box3d_adapter.hpp>

wsl::phys::box3d::world world;
wsl::phys::box3d::body_desc desc;
auto body = world.create_body (desc);
world.step (1.0F / 60.0F);
auto position = world.body_position (body);
world.destroy_body (body);
```

## Debug drawing

`phys::engine::draw_debug` appends collider wireframes as world-space line
segments:

```cpp
std::vector<wsl::phys::debug_line> lines;
physics.draw_debug (lines);   // appends; the caller owns the list
```

Box3D reports shapes through `b3World_Draw`, which only fires when the world
definition installs a `createDebugShape` hook — the adapter installs one when
the world is created. Hulls become edge wireframes, spheres and capsules are
approximated with circles, and contact/joint geometry comes through the
segment callback. The editor hands the result to
`gfx::scene_renderer::draw_debug_lines` whenever the physics manager's
**Show Debug** flag is set.

## Collision Layers

Defined in `layers.hpp` as backend-neutral `object_layer` values. Map them
onto Box3D collision categories inside the adapter to control which layers
interact.
