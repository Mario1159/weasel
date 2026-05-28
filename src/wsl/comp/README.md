# `wsl::comp` — ECS Components

Entity-owned data types that define *what an entity is* in the ECS. Components are plain data structs registered with `entt::meta` for serialization and editor reflection.

## World Components (per-entity)

| Component | Header | Description |
|-----------|--------|-------------|
| `transform` | `transform.hpp` | Local position, rotation (quat), scale. Model matrix computed from these. |
| `world_transform` | `world_transform.hpp` | Cached world-space transform (parent-relative). Updated by `transform_system`. |
| `hierarchy` | `hierarchy.hpp` | Parent/child/sibling links for entity tree structure. |
| `camera` | `camera.hpp` | Perspective projection (fov, near, far, aspect). View derived from `world_transform`. |
| `model_instance_3d` | `model_instance_3d.hpp` | Reference to a `gfx::model_3d` resource for rendering. |
| `rigid_body` | `rigid_body.hpp` | Physics body definition (shape, collision layer, motion type). Wraps Jolt body lifecycle. |
| `area` | `area3d.hpp` | 3D trigger zone for overlap events. |
| `character_body` | `character_body.hpp` | Character controller physics body. |
| `audio` | `audio.hpp` | Audio source reference. |
| `point_light` | `point_light.hpp` | Point light source. |
| `spot_light` | `spot_light.hpp` | Spot (cone) light source. |
| `directional_light` | `directional_light.hpp` | Directional light (sun). |
| `prefab_instance` | `prefab_instance.hpp` | Links an entity back to its prefab scene. |

## Singleton Components (registry-scoped)

Located in `singl/` — one instance per registry/context:

| Component | Header | Description |
|-----------|--------|-------------|
| `runtime_context` | `singl/runtime_context.hpp` | Global runtime state (project path, play mode, etc.) |
| `editor_context` | `singl/editor_context.hpp` | Editor session state. |
| `rendering_manager` | `singl/rendering_manager.hpp` | Renderer configuration and pipeline cache. |
| `physics_manager` | `singl/physics_manager.hpp` | Physics engine instance reference. |
| `ui_manager` | `singl/ui_manager.hpp` | UI layout and interaction state. |
| `skybox_instance_3d` | `singl/skybox_instance_3d.hpp` | Active skybox cubemap reference. |

## Usage

```cpp
#include <wsl/comp/components.hpp>

// Create an entity with a transform
entt::entity e = registry.create();
registry.emplace<wsl::comp::transform>(e, math::vec3f{0, 2, 0});
registry.emplace<wsl::comp::camera>(e);

// Set rotation via Euler degrees
auto &t = registry.get<wsl::comp::transform>(e);
t.set_rotation_xyz({0, 45, 0}); // yaw 45 degrees

// Query and iterate
auto view = registry.view<wsl::comp::transform, wsl::comp::camera>();
for (auto [entity, tf, cam] : view.each()) {
    glm::mat4 vp = cam.proj() * wsl::comp::camera::view(
        registry.get<wsl::comp::world_transform>(entity));
}
```

## Registration

All components are listed in `components.hpp` as `component_types` and `singleton_types` type lists. Register with `reg::component_registry` and `reg::singleton_registry` for serialization and editor support.
