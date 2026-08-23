Events
======

Weasel decouples systems, entities and components through two independent
eventing mechanisms, both living under the :cpp:any:`wsl::event` namespace:

* **Subscription events** (observer / pub-sub) — explicit, connection-tracked
  reactions. An emitter *system* declares an event source; one or more handler
  *systems* declare named sinks; connections are wired at runtime and serialized
  into scenes. Firing an event dispatches it immediately to every connected sink.
  Use these for editor-wired, connection-graphed reactions (e.g. a combat hit that
  triggers damage and audio). See
  :doc:`manual_events_subscription_events`.
* **Messages** (buffered / pull) — fire-and-forget, decoupled notifications. A
  producer ``post``\ s a typed message into a per-frame queue; each consuming
  system holds a typed ``message_reader<T>`` and pulls the messages it cares about
  during its own update. There is no wiring and no broadcast. Use these for global,
  high-frequency, decoupled notifications such as input, UI and game-state changes.
  See :doc:`manual_events_messages`.

Both are fed from the same SDL poll boundary. Raw ``SDL_Event``\ s are translated
into engine-typed events at the poll loop and dropped into the pool, so input and
game events travel through one pipeline:

.. code-block:: text

   emit()  ──► event_hub  ──► connected event_sinks           (explicit, immediate)
   post()  ──► message_bus ── drain() ──► typed reader buffers ──► systems pull via message_reader<T>

Every operation in both models is available from C++ **and** from Daslang.
Scripts address event and message types by string name (e.g.
``event_emit("PlayerHit", hit)``, ``message_post("PlayerDied", msg)``) with a
Daslang struct mirroring the payload layout; see the two subpages for the full
API surface on each side.

.. toctree::
   :maxdepth: 1

   manual_events_subscription_events
   manual_events_messages
