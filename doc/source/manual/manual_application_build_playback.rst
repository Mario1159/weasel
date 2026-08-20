Application Build & Playback
============================

In Weasel, an **application** is a *project*: a directory described by a
project manifest (``wslpro.json``) that points at the scenes, resources and
daScript source folders, plus the scenes themselves (stored as ``.wscn.json``
files) and the ``.das`` system/component/singleton scripts. The manifest records
paths such as ``systems_path``, ``components_path``, ``singletons_path``,
``scenes_path`` and ``default_scene_path`` so the engine knows where to load
content from.

Once a project exists, there are two ways to actually *run* it:

- **Interpretation through the editor** -- the editor loads the manifest and
  runs the dasScript systems live, with no separate build step. This is the
  default workflow for authoring and fast iteration.
- **AOT compilation through CMake** -- the same project is compiled ahead of
  time into a native executable that links the engine, which is what you ship
  and distribute.

Both paths run the exact same ECS, rendering and physics code; only the way the
dasScript logic is turned into executable code differs.

.. toctree::
   :maxdepth: 1

   manual_application_build_playback_interpretation_through_editor
   manual_application_build_playback_aot_compilation_through_cmake
