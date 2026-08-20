AOT Compilation Through CMake
=============================

For distribution, a Weasel project can be **compiled ahead of time (AOT)** into
a native executable using CMake. The engine ships a CMake helper,
``weasel_aot_das()`` (from ``WeaselDasAOT.cmake``, included automatically when
you ``find_package(Weasel REQUIRED)``), that turns the project's dasScript
systems into C++.

How it works:

- ``weasel_aot_das()`` drives the ``weasel-cli aot`` command, which runs the
  AOT generation *inside the engine process*. Because it runs in-process,
  ``require weasel_api`` resolves to the real C++ module, so the emitted ``.cpp``
  references the real proxy types and genuinely runs as compiled code instead
  of silently falling back to the interpreter.
- System/entry ``.das`` files are passed via ``FILES`` and are AOT-compiled to
  C++. Component ``.das`` files (which define ``require``-able modules such as
  ``class MouseRotate``) are passed via ``COMPONENTS`` and are added as
  ``-I`` search roots rather than compiled standalone.
- The generated ``.cpp`` sources are appended to your target, which links
  against the ``wsl`` shared library. The result is a single native executable
  that no longer needs the interpreter at runtime.

A minimal ``CMakeLists.txt`` looks like:

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.22)
   project(my-game LANGUAGES C CXX)
   set(CMAKE_CXX_STANDARD 20)

   find_package(Weasel REQUIRED)

   file(GLOB_RECURSE DAS_SYSTEMS "src/systems/*.das")
   file(GLOB_RECURSE DAS_COMPONENTS "src/components/*.das")

   if(DAS_SYSTEMS)
       weasel_aot_das(FILES ${DAS_SYSTEMS}
                      COMPONENTS ${DAS_COMPONENTS}
                      OUTPUT_VAR AOT_SRCS)
   endif()

   add_executable(${PROJECT_NAME} src/main.cpp ${AOT_SRCS})
   target_link_libraries(${PROJECT_NAME} PRIVATE wsl)

Because the output is a plain CMake target, you can package it with CPack
(``TGZ``/``DEB``/``RPM``) or integrate it into any existing build system. The
compiled application runs the same scenes and ECS behavior as the interpreted
editor workflow -- only the dasScript code path is now native.
