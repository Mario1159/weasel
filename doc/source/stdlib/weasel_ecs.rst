weasel_ecs
==========

.. das:module:: weasel_ecs

.. das:class:: EcsSystem

   Base class for daslang-backed ECS systems.

   Inherit from this class and override the lifecycle methods
   to create game logic in daslang.

   .. das:function:: on_init() : void

      Called once when the system is first loaded.

   .. das:function:: on_update(dt : float) : void

      Called every frame with the delta time in seconds.

   .. das:function:: on_event() : void

      Called when an engine event occurs (mouse, keyboard, window).

      Use ``get_event_kind()`` and the ``EVENT_*`` constants to
      determine the event type.

   .. das:function:: on_inactive() : void

      Called when the system is disabled or unloaded.

