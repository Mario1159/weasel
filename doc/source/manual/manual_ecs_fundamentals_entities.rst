Entities
========

An **entity** is a lightweight, unique identifier that groups a set of
components together. In Weasel (and the underlying ``entt`` library) an entity
is essentially an index plus a version number used to detect stale references
after an entity is destroyed and its slot is reused.

Entities carry no data and no behavior by themselves. Their meaning comes
entirely from the components attached to them: a "player" is just an entity
that happens to own a ``Transform``, a ``Camera``, an ``Input`` state and a
physics body, while a "light" is an entity that owns a ``Transform`` and a
``PointLight``. This composition-over-inheritance approach means you build new
kinds of objects by mixing existing components rather than writing new classes.

Entities can be **hierarchical**: an entity may have a parent and children, in
which case its world transform is derived from its parent's. Flat (non-parented)
entities are also fully supported.

In the **Weasel Editor**, entities appear in the **Entities and Singletons**
panel, either as a flat list or as a tree reflecting the parenting hierarchy.
Selecting an entity reveals its components in the **Inspector**.
