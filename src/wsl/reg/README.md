# `wsl::reg` — Registry & Registration Infrastructure

Central registration system for components, singletons, system factories, and signal/event wiring. This namespace bridges the ECS core with the editor and serialization layers.

## Key Classes

| Class | Header | Description |
|-------|--------|-------------|
| `component_registry` | `component_registry.hpp` | Registers entity-owned world components with metadata, serialization, and generic copy/emplace/remove callbacks. |
| `singleton_registry` | `singleton_registry.hpp` | Registers singleton (registry-scoped) components with serialize/deserialize support. |
| `system_factory_registry` | `system_factory_registry.hpp` | Maps system names to factory functions for dynamic instantiation in scenes. |
| `registry_queries` | `registry_queries.hpp` | Cross-concept query interface — find which systems/iterations/signals relate to a given component or entity. |

## Signal System (`sig/`)

| Class | Header | Description |
|-------|--------|-------------|
| `signal_hub` | `sig/signal_hub.hpp` | Central hub for declaring signals, iterations, and event handlers. Supports typed signal emission and connection. |
| `signal_hub_fwd` | `sig/signal_hub_fwd.hpp` | Forward declarations for signal types. |

## Usage

```cpp
#include <wsl/reg/component_registry.hpp>
#include <wsl/reg/singleton_registry.hpp>
#include <wsl/reg/system_factory_registry.hpp>

// Register a world component
wsl::reg::component_registry comp_reg;
comp_reg.register_world_component<wsl::comp::transform>();
comp_reg.register_world_component<wsl::comp::camera>();

// Query registered components
auto desc = comp_reg.find("transform");
if (desc && desc->can_add_default) {
    desc->emplace_default(registry, entity);
}

// Copy a component between entities
comp_reg.copy_world_component(src_reg, src_ent, dst_reg, dst_ent, type_id);

// Register a singleton
wsl::reg::singleton_registry sing_reg;
sing_reg.register_singleton_component<wsl::comp::singl::runtime_context>(
    { .core = true });

// Ensure core singletons exist in a registry
sing_reg.apply_core_singleton_components(registry);

// Register a system factory
wsl::reg::system_factory_registry sys_reg;
sys_reg.register_system<wsl::sys::physics_system>("Physics");
sys_reg.register_system<wsl::sys::transform_system>("Transform");

// Create a system instance in a scene
auto system = sys_reg.create("Physics", scene);

// Declare dependencies and conflicts
sys_reg.declare_system_dependency<wsl::sys::shadow_system, wsl::sys::lighting_system>();
sys_reg.declare_system_conflict<wsl::sys::my_custom_physics, wsl::sys::physics_system>();
```

## Serialization Support

Component and singleton registries provide function pointers (`save_binary`, `load_binary`, `save_json`, `load_json`) for each registered type. These are used by the scene snapshot serializer:

```cpp
cereal::JSONOutputArchive archive(ss);
for (auto &desc : comp_reg.ordered()) {
    desc->save_json(archive, registry);
}
```

## Debug Queries

`registry_queries` provides introspection:

- `get_addable_world_components()` — components not yet on an entity
- `get_matching_iterations()` — system iterations that operate on an entity's components
- `find_signals_using_world_component()` — signals relevant to a component type
