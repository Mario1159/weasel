Logging
=======

Weasel provides structured logging through an `spdlog <https://github.com/gabime/spdlog>`_-based
logger. Every message carries a severity level, the name of the subsystem that
emitted it, and a timestamp, and is fanned out to multiple *sinks* at once:
a colored console sink and a Tracy sink that forwards log lines into the
Tracy profiler (see :doc:`Tracy Quick Start Guide
<manual_profiling_tracy_quick_start_guide>`).

Subsystem loggers
-----------------

The engine exposes one named logger per major subsystem. In C++ you obtain them
through the ``wsl::log`` namespace:

- ``wsl::log::core()`` -- core engine and ECS
- ``wsl::log::gfx()`` -- rendering / GPU
- ``wsl::log::rsc()`` -- resources and scene loading
- ``wsl::log::sys()`` -- systems
- ``wsl::log::editor()`` -- the Weasel Editor
- ``wsl::log::cli()`` -- the command-line interface
- ``wsl::log::phys()`` -- physics
- ``wsl::log::net()`` -- networking
- ``wsl::log::cmake()`` -- build / CMake integration

Each returns a standard ``std::shared_ptr<spdlog::logger>``, so you use the
usual spdlog API on it.

Log levels
----------

Messages are filtered by severity. Weasel uses spdlog's levels, in increasing
order of severity:

``trace < debug < info < warn < error < critical``

The engine initializes the global level to ``debug``, so ``trace`` messages are
suppressed by default while ``debug`` and above are shown. Logging is
initialized once at startup via ``wsl::log::init()``.

Output format
-------------

Console lines follow the pattern::

   [2026-08-19 12:34:56.789] [gfx] [info] vkCreateBuffer succeeded

i.e. a timestamp, the ``[logger name]``, the ``[level]``, and the message. In
the editor the same stream is captured by the **Console** panel, and (when
Tracy is enabled) the line is also visible inside the Tracy profiler view.

Logging from C++
----------------

Use the subsystem logger that matches your code, with spdlog's ``{}``
positional formatting:

.. code-block:: cpp

   #include "wsl/log/log.hpp"

   wsl::log::sys ()->info ("player spawned at {}", position);
   wsl::log::gfx ()->warn ("frame took {} ms", dt * 1000.0);
   wsl::log::phys ()->error ("body {} fell out of world", body_id);

Logging from daslang
--------------------

In daScript (for example inside a system) four helpers are available through
the ``weasel_api`` module. They all route to the **sys** logger:

- ``log_info("message")``
- ``log_debug("message")``
- ``log_warn("message")``
- ``log_error("message")``

.. code-block:: das

   def on_init {
       log_info("My system initialized!")
   }

   def on_update(dt : float) {
       log_debug("frame dt = {dt}")
       if (health <= 0) {
           log_error("entity died")
       }
   }

These are the same routines used throughout the ECS Programming examples.
