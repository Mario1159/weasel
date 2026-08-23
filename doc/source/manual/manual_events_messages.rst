Messages
========

Messages are Weasel's buffered, **pull-only** event model. Unlike subscription
events, there is **no wiring and no broadcast**: a producer ``post``\ s a typed
message into a per-frame queue, and any system that cares about that type holds a
``message_reader<T>`` and pulls the messages during its own update.

This keeps the cost proportional to *(systems × their own message types)* rather
than *(systems × all messages)*, and avoids double-delivery and opt-out ambiguity.
It is the model to use for global, fire-and-forget, decoupled notifications -- input,
UI and game-state changes.

Daslang: reading messages
-------------------------

From Daslang, the engine's built-in input messages are exposed through a pull
API that reads the active runtime context's published buffer. There is no
``on_event`` hook anymore -- iterate the per-frame accessors inside your system's
``on_update``:

.. code-block:: das

   def on_update(dt : float) {
       for i in range(key_pressed_count()) {
           let e = key_pressed_at(i)
           log_info("key pressed: {e.keycode}")
       }
   }

The available pull accessors are ``mouse_motion_count`` / ``mouse_motion_at(i)``,
``mouse_button_count`` / ``mouse_button_at(i)``, ``mouse_wheel_count`` /
``mouse_wheel_at(i)``, ``key_pressed_count`` / ``key_pressed_at(i)``,
``key_released_count`` / ``key_released_at(i)``, ``text_input_count`` /
``text_input_at(i)`` / ``text_input_text(i)``, ``window_resized_count`` /
``window_resized_at(i)`` and ``quit_requested_count`` / ``quit_requested_at(i)``.
The typed payloads are ``MouseMotion``, ``MouseButton``, ``MouseWheel``,
``KeyPressed``, ``KeyReleased``, ``TextInput``, ``WindowResized`` and
``QuitRequested``.

.. note:: Custom (user-defined) messages are defined, posted and pulled in C++;
   only the built-in input messages above are directly readable from Daslang.

Defining a message (C++)
------------------------

.. note:: Messages are C++ structs; there is no Daslang equivalent for *defining*
   a new message type. The engine's own input messages are posted by the SDL
   poll boundary and pulled from Daslang via the accessors above.

A message is a plain Weasel data struct. No registration is required:

.. code-block:: cpp

   struct PlayerDied {
     entt::entity who;
     int          cause;
   };

Posting (C++)
--------------

.. note:: Posting is a C++-only operation; there is no Daslang ``post`` for
   arbitrary messages.

The :cpp:any:`wsl::event::message_bus` is a singleton on the active
:cpp:any:`wsl::comp::runtime_context`. Post from anywhere -- a system update, an
async task, etc. Posting is thread-safe:

.. code-block:: cpp

   void combat_system::on_update(entt::registry &reg, double dt) {
     if (m_just_died) {
       m_runtime_ctx->message_bus().post<PlayerDied>({m_player, 0});
     }
   }

Pulling with a reader
------------------------

Daslang first (the built-in pull accessors shown above). For a C++-defined
message, each consuming system holds one ``message_reader<T>`` per message type
and pulls it during its own update. The reader keeps its own cursor, so multiple
systems holding a reader for the same type each receive every message exactly
once:

.. code-block:: das

   def on_update(dt : float) {
       for i in range(mouse_button_count()) {
           let e = mouse_button_at(i)
           log_info("click at {e.x},{e.y}")
       }
   }

.. code-block:: cpp

   void player_system::on_update(entt::registry &reg, double dt) {
     m_deaths.for_each([](const PlayerDied &d) {
       wsl::log::sys()->info("player died: {}", d.who);
     });
   }

   // m_deaths = m_runtime_ctx->message_bus().reader<PlayerDied>();

The engine's own input types (``key_pressed``, ``key_released``, ``text_input``,
``mouse_motion``, ``mouse_button``, ``mouse_wheel``, ``window_resized``,
``quit_requested``) are posted by the SDL poll boundary translator and pulled the
same way. A catch-all ``message_reader<message>`` is available for debug/logging
systems that genuinely need every type.

Frame lifecycle
---------------

``message_bus::drain(registry)`` is called once per frame, right after the SDL poll
loop. It publishes the pending queue into the per-type reader buffers (and delivers
any ``subscribe`` callbacks), then clears the pending queue. Events posted *during*
a drain roll to the next frame, so there is no infinite loop and the queue cannot
grow unbounded. At each drain every reader cursor is reset to the start of the new
buffer, so a system reads exactly that frame's messages once.

.. code-block:: text

   while (SDL_PollEvent(&e))                  // drain OS queue
       translate_and_post(bus, engine_event{e});  // SDL -> engine type, post
   message_bus.drain(registry);               // publish pending -> reader buffers
       └─ each system pulls its message_reader<T> during its own update

Non-system consumers (C++)
--------------------------

.. note:: ``subscribe`` callbacks are a C++-only API.

For consumers that are not systems (e.g. the application or runtime context
handling window/quit), use a typed or catch-all ``subscribe`` callback instead of a
reader:

.. code-block:: cpp

   bus.subscribe<window_resized>([](const window_resized &e) {
     wsl::log::sys()->info("window resized to {}x{}", e.width, e.height);
   });
