Events
======

React to discrete input events -- key presses, mouse buttons, the wheel,
mouse motion, text input and window changes.

Events are delivered through a per-frame **message bus**: the engine polls SDL
and publishes each input occurrence once per frame, and your system *pulls*
them during its update. This is a pull model rather than a callback, which
means several systems can each observe the same event independently. The same
bus is visible from both daScript and C++ -- the examples below show the
daScript form first, followed by its C++ equivalent.

Read the events from inside your system's ``on_update`` (daScript) or its
update function (C++). Each event type exposes a ``*_count`` accessor and a
``*_at(index)`` accessor in daScript, and a typed ``message_reader`` in C++.

The following SDL constants are available from daScript: ``SDL_BUTTON_LEFT``,
``SDL_BUTTON_RIGHT`` and ``SDL_SCANCODE_ESCAPE`` (identical values apply in
C++ through ``SDL3/SDL_mouse.h`` and ``SDL3/SDL_keyboard.h``).

Mouse button
------------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(mouse_button_count()) {
           let e = mouse_button_at(i)
           if e.down && e.button == SDL_BUTTON_LEFT {
               log_info("left click at {e.x}, {e.y}")
           }
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::mouse_button>();
       reader.for_each([&](const wsl::input::mouse_button &e) {
           if (e.down && e.button == SDL_BUTTON_LEFT) {
               wsl::log::sys()->info("left click at {}, {}", e.x, e.y);
           }
       });
   }

Keyboard
--------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(key_pressed_count()) {
           let e = key_pressed_at(i)
           if e.scancode == SDL_SCANCODE_ESCAPE {
               log_info("escape pressed (repeat={e.repeat})")
           }
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::key_pressed>();
       reader.for_each([&](const wsl::input::key_pressed &e) {
           if (e.scancode == SDL_SCANCODE_ESCAPE) {
               wsl::log::sys()->info("escape pressed (repeat={})", e.repeat);
           }
       });
   }

Mouse motion
------------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(mouse_motion_count()) {
           let e = mouse_motion_at(i)
           log_debug("mouse at {e.x},{e.y} delta {e.xrel},{e.yrel}")
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::mouse_motion>();
       reader.for_each([&](const wsl::input::mouse_motion &e) {
           wsl::log::sys()->debug("mouse at {},{} delta {},{}",
                                  e.x, e.y, e.xrel, e.yrel);
       });
   }

Mouse wheel
-----------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(mouse_wheel_count()) {
           let e = mouse_wheel_at(i)
           log_info("wheel {e.x},{e.y} flipped={e.flipped}")
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::mouse_wheel>();
       reader.for_each([&](const wsl::input::mouse_wheel &e) {
           wsl::log::sys()->info("wheel {},{} flipped={}", e.x, e.y, e.flipped);
       });
   }

Text input
----------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(text_input_count()) {
           log_info("typed: {text_input_text(i)}")
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::text_input>();
       reader.for_each([&](const wsl::input::text_input &e) {
           wsl::log::sys()->info("typed: {}", e.text);
       });
   }

Window resized
--------------

.. code-block:: das
   :caption: daScript

   def on_update(dt : float) {
       for i in range(window_resized_count()) {
           let e = window_resized_at(i)
           log_info("window now {e.width}x{e.height}")
       }
   }

.. code-block:: cpp
   :caption: C++

   #include "wsl/comp/singl/runtime_context.hpp"
   #include "wsl/event/message_bus.hpp"
   #include "wsl/input.hpp"
   #include "wsl/log/log.hpp"

   void my_system::update(entt::registry &reg, float dt)
   {
       auto &rc = reg.ctx<wsl::comp::runtime_context>();
       auto reader = rc.message_bus().reader<wsl::input::window_resized>();
       reader.for_each([&](const wsl::input::window_resized &e) {
           wsl::log::sys()->info("window now {}x{}", e.width, e.height);
       });
   }
