# `wsl::math` — Math Utilities

Core math types used across the engine, primarily vectors and quaternions with interop between GLM, Jolt, and ImGui.

## Types

| Type | Header | Description |
|------|--------|-------------|
| `vec3f` | `vector.hpp` | 3-component float vector with GLM ↔ Jolt ↔ ImGui conversions. Built-in `custom_inspect` for editor UI. |
| `quatf` | `vector.hpp` | Quaternion (x, y, z, w) with GLM conversion. |

## `vec3f`

```cpp
wsl::math::vec3f v{ 1, 2, 3 };

// Implicit conversions
glm::vec3 gv = v;
JPH::Vec3 jv = v;

// Math operators
v += glm::vec3{ 0, 1, 0 };
v -= glm::vec3{ 1, 0, 0 };

// Editor inspector (ImGui)
bool changed = v.custom_inspect("Position");

// Serialization
ar(cereal::make_nvp("v", v));

// EnTT meta reflection (for editor property grid)
v.register_meta();
```

## `quatf`

```cpp
wsl::math::quatf q{ 0, 0, 0, 1 }; // identity

// Implicit conversion to GLM
glm::quat gq = q;

// Serialization
ar(cereal::make_nvp("rotation", q));

// EnTT meta reflection
q.register_meta();
```

## Mikktspace

The `mikktspace*` files provide the MikkTSpace algorithm for computing tangent frames from vertex positions, normals, and UV coordinates. Used during model loading to generate tangent data for normal mapping.

```cpp
// Internal use during model import
mikktspace::generate_tangents(vertices, indices);
```
