Subscription Events
===================

Subscription events are Weasel's explicit, connection-tracked eventing model
(formerly the *signal* system). An emitter system **declares** an event source;
handler systems **declare** named sinks; and connections are **wired at runtime**
between a source and a sink of the same event type. When the source emits, the
event is dispatched immediately to every connected sink.

Key properties:

* An event source is owned by exactly **one emitter system** -- there is no
  component relation and no entity scoping.
* A sink is a named handler (``handler_name``) owned by a handler system.
* Connections are identified by ``(event_type_id, system_type_id, handler_name)``
  only. Any entity identity a handler needs is a **field of the event payload
  itself**, not a property of the connection.
* Connections are centralized in the :cpp:any:`wsl::event::event_hub` and
  serialized into scenes, so wired connections persist across saves.

Daslang: wiring existing connections
------------------------------------

From a Daslang system you can wire (and unwire) connections that were declared
in C++. Event types are passed as strings because Daslang cannot name C++
types directly:

.. code-block:: das

   event_connect("PlayerHit", "CombatSystem", "on_player_hit")
   event_disconnect("PlayerHit", "CombatSystem", "on_player_hit")

The emitted payload is just a Daslang struct whose layout matches the C++ event
struct, so a connected handler can read ``attacker`` / ``victim`` / ``damage``
fields directly. Declaring the source/sink and emitting remain C++ steps (see
below) -- the script only wires what already exists.

Defining the event payload (C++)
--------------------------------

.. note:: Event payloads are declared as C++ structs; there is no Daslang
   equivalent for *defining* an event type. The fields are visible to a
   connected Daslang handler as a plain struct with matching layout.

An event is a plain, trivially-copyable struct. Put any entity identity in the
payload:

.. code-block:: cpp

   struct PlayerHit {
     entt::entity attacker;   // source identity in the payload
     entt::entity victim;     // target identity in the payload
     float        damage;
   };

Declaring the source -- emitter (C++)
----------------------------------------

.. note:: Source/sink declaration runs in C++ registration hooks; Daslang
   cannot declare them. Wire them from script with ``event_connect`` above.

Each system declares the events it owns from its registration hook. The emitter
declares an *event source*:

.. code-block:: cpp

   void CombatSystem::register_event_sources(wsl::event::event_hub &hub) {
     hub.declare_event_source<PlayerHit, CombatSystem>();
   }

Declaring a sink -- handler (C++)
------------------------------------

The handler system declares a named sink. The handler is a lambda with the
signature ``void(void *owner, entt::registry &, const void *)`` that downcasts
the owner and the event payload:

.. code-block:: cpp

   void DamageSystem::register_event_sinks(wsl::event::event_hub &hub) {
     hub.declare_event_sink<PlayerHit, DamageSystem>(
         "on_player_hit",
         +[] (void *owner, entt::registry &reg, const void *sig) {
           const auto &e = *static_cast<const PlayerHit *>(sig);
           static_cast<DamageSystem &>(*static_cast<sys::ecs_system *>(owner))
               .on_player_hit(reg, e);
         });
   }

   void DamageSystem::on_player_hit(entt::registry &reg, const PlayerHit &e) {
     auto &hp = reg.get<health>(e.victim);
     hp.value -= e.damage;   // attacker/victim read straight from the payload
   }

Emitting (C++)
-----------------

.. note:: Emitting is a C++-only operation; there is no Daslang ``emit`` for
   arbitrary events. A connected Daslang handler still receives the payload
   through the wiring established with ``event_connect``.

Construct a handle from the hub and emit. The handle is a non-owning view; the
source must already be declared. Entity identity travels in the payload, never in
the connection:

.. code-block:: cpp

   void CombatSystem::on_update(entt::registry &reg, double dt) {
     if (m_hit_landed) {
       wsl::event::event_source<PlayerHit, CombatSystem>{hub}
           .emit(m_attacker, m_victim, m_damage);
       // equivalently:
       wsl::event::emit(hub, PlayerHit{m_attacker, m_victim, m_damage});
     }
   }

Connecting and disconnecting at runtime (C++)
------------------------------------------------

.. note:: The Daslang equivalent is ``event_connect`` / ``event_disconnect``
   shown above.

After both sides are declared, wire them with the typed handle API (no entity
arguments):

.. code-block:: cpp

   wsl::event::event_source<PlayerHit, CombatSystem> src{hub};
   wsl::event::event_sink<PlayerHit, DamageSystem>   sink{hub, "on_player_hit"};

   src.add_listener(sink);        // connect
   src.remove_listener(sink);     // disconnect

You can also drive the hub directly with type ids:

.. code-block:: cpp

   hub.connect(event_type_id, system_type_id, "on_player_hit");
   hub.disconnect(event_type_id, system_type_id, "on_player_hit");
