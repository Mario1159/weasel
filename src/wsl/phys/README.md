# `wsl::phys` — Physics Engine

Thin C++ wrapper around [Jolt Physics](https://github.com/jrouwe/JoltPhysics) providing body creation, stepping, collision queries, and sensor overlap detection.

## Key Classes

| Class | Header | Description |
|-------|--------|-------------|
| `engine` | `physics_engine.hpp` | Owns the Jolt `PhysicsSystem`, job system, temp allocator, and layer filters. Main API for physics simulation. |
| `contact_listener_impl` | `physics_engine.hpp` | Jolt `ContactListener` that generates `sensor_overlap_event` for sensor/non-sensor pairs. |
| `broad_phase_layer_interface` | `broad_phase_layer_interface.hpp` | Maps object layers to broad-phase layers for collision filtering. |
| `object_vs_broad_phase_layer_filter` | `object_vs_broad_phase_layer_filter.hpp` | Filter for broad-phase vs object layer queries. |
| `object_layer_pair_filter` | `object_layer_pair_filter.hpp` | Filter for object vs object layer collision pairs. |

## Type Aliases

```cpp
using body_id = JPH::BodyID;
using motion_type = JPH::EMotionType;       // Static, Kinematic, Dynamic
using allowed_do_fs = JPH::EAllowedDOFs;    // All, TranslationX, RotationZ, etc.
using object_layer = JPH::ObjectLayer;
```

## Usage

```cpp
#include <wsl/phys/physics_engine.hpp>

wsl::phys::engine physics;

// Configure
physics.set_gravity(-9.8);
physics.set_fixed_step(1.0 / 60.0);

// Per-frame step
physics.step(dt);

// Body creation (via Jolt BodyInterface)
JPH::BodyInterface &bi = physics.get_body_interface();

JPH::BodyCreationSettings settings(
    new JPH::BoxShape(JPH::Vec3(1, 1, 1)),
    JPH::RVec3(0, 5, 0),
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Dynamic,
    Layers::MOVING);

JPH::BodyID body_id = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);

// Sensor overlap events
std::vector<wsl::phys::sensor_overlap_event> events = physics.drain_sensor_events();
for (auto &ev : events) {
    if (ev.entered) {
        // sensor hit something
    }
}

// Cleanup
bi.RemoveBody(body_id);
bi.DestroyBody(body_id);
physics.clear();
```

## Collision Layers

Defined in `layers.hpp`. Customize `broad_phase_layer_interface` and pair filters to control which object layers interact.
