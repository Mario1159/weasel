Singleton Components
====================

A **singleton component** is a component that exists exactly once per world.
Rather than being attached to an entity, a singleton is addressed by its type
and is globally accessible. Singletons are the ECS-friendly way to represent
global state that does not belong to any single entity.

Weasel distinguishes two flavors, both visible in the **Singletons** section of
the **Entities and Singletons** panel:

- **Core singletons** are created by the engine itself and hold essential
  services such as the runtime context, the rendering context, the input/keyboard
  state and the signal hub.
- **Scene singletons** are added by the active scene (for example a project or
  level configuration) and are available only while that scene is loaded.

Like world components, singletons are plain data; systems read and write them as
part of their iteration. Their single-instance guarantee makes them ideal for
configuration, global counters, or shared buffers.

Authors can declare **user-defined singleton components** in daScript to hold
custom global project state -- see :doc:`User-Defined Singleton Components
<manual_ecs_programming_user_defined_singleton_components>`.
