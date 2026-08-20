World Components
================

**World components** (often just called "components") are plain data structures
that store per-entity state living in the simulation world. Examples include
``Transform`` (position, rotation, scale), ``ModelInstance3D`` (which mesh and
material to draw), ``RigidBody`` (physics state), and ``Sprite2D`` (a 2D
texture). A component holds *state only* -- it never contains the logic that
updates that state.

Because components are value types stored contiguously by component type, the
ECS can iterate them efficiently: a system that needs every ``Transform`` walks
a single tightly packed array rather than chasing pointers through a tree of
game objects. This is what makes the ECS pattern performant at scale.

Components are attached to and removed from entities at runtime (for example
spawning a prefab adds a bundle of components in one step). In the **Weasel
Editor**, the components of the selected entity are shown in the **Inspector**
panel, where you can edit their fields and use the **Add Component** menu to
attach additional world components.

Authors can also declare **user-defined world components** in daScript to add
new per-entity state -- see :doc:`User-Defined World Components
<manual_ecs_programming_user_defined_world_components>`.
