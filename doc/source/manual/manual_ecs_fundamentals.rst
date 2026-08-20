ECS Fundamentals
================

The Entity Component System (ECS) is the architectural backbone of Weasel.
Instead of modeling game objects as deeply nested class hierarchies, ECS
separates **data** from **behavior**:

- **Entities** are lightweight identifiers (plain integers) that group a set of
  components together. An entity has no logic and no data of its own; it is
  simply a "bag" of components.
- **Components** are plain data structures (transforms, meshes, bodies, ...)
  attached to entities. They hold *state*, never behavior.
- **Systems** contain the *behavior*. Each system iterates over all entities
  that own a specific combination of components and updates them every frame.

Weasel's ECS is built on top of `entt <https://github.com/skypjack/entt>`_.
Because components are stored in contiguous arrays grouped by type, the engine
can process thousands of entities by walking tightly packed memory, which is
cache-friendly and fast. Systems declare *queries* (for example "every entity
that has a ``Transform`` and a ``Camera``") and the ECS hands them exactly the
matching entities.

A fourth concept, unique to Weasel, is the **singleton component**: a component
that exists exactly once per world (for example the rendering context or the
input state). Singletons are addressed by type rather than by entity.

Where to find ECS in the Weasel Editor
---------------------------------------

The editor exposes the live ECS state through three main panels:

- The **Entities and Singletons** panel (docked on the left) lists every
  entity -- organized as a hierarchy tree when entities are parented -- and,
  below it, the **Singletons** list (core singletons first, then scene
  singletons).
- The **Inspector** panel shows the components attached to the currently
  selected entity or singleton, and provides an **Add Component** menu to
  attach new world components at authoring time.
- The **Systems** panel lists the active systems (the built-in core systems
  plus any user-defined daslang systems) and lets you inspect their signal
  connections and iterations.

The sections below describe each pillar in more detail.

.. toctree::
   :maxdepth: 2

   manual_ecs_fundamentals_entities
   manual_ecs_fundamentals_world_components
   manual_ecs_fundamentals_singleton_components
   manual_ecs_fundamentals_systems

