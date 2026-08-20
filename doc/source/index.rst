Weasel Engine Documentation
===========================

.. toctree::
   :maxdepth: 2
   :caption: Manual

   manual/index

.. toctree::
   :maxdepth: 2
   :caption: Tutorials

   tutorials/index

.. toctree::
   :maxdepth: 2
   :caption: daslang API Reference

   stdlib/weasel_api
   stdlib/weasel_ecs
   stdlib/components

.. toctree::
   :maxdepth: 2
   :caption: C++ API Reference

   cpp/index

The Weasel engine exposes its API to daScript through two modules:

- **weasel_api** -- Core engine functions: entity management, transforms,
  scene queries, events, window operations, and raycasting.
- **weasel_ecs** -- ECS system base class for writing game logic in daScript.

Quick Example
-------------

.. code-block:: cpp

    require weasel_api

    def on_update(dt : float)
        let e = entity_create()
        add_component(e, Transform())
        let t = get_component_or(e, Transform())
        t.position = float3(1.0, 2.0, 3.0)
