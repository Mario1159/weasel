Core Systems
============

**Core systems** are the built-in engine systems that Weasel registers
automatically. They provide the essential services every project needs --
transform propagation, physics, rendering, lighting, shadows, audio and the
application UI -- so authors only have to write the game-specific logic on top.

When a world is initialized, Weasel constructs and caches the following core
systems (the display name shown in the editor **Systems** panel is given in
parentheses):

- **Transform System** (``Transform``) -- propagates hierarchical transforms,
  computing each entity's world transform from its parent.
- **Jolt Physics System** (``Physics``) -- runs the 3D physics simulation
  through Jolt: rigid bodies, areas, character controllers and ray-casts.
- **3D Render System** (``3D Render``) -- renders 3D model instances with PBR
  materials, dynamic lighting clusters and shadows.
- **2D Render System** (``2D Render``) -- batches and renders 2D sprites and
  transforms.
- **Application UI System** (``UI``) -- renders the RML-based in-application
  user interface.
- **Lighting System** (``Lighting``) -- organizes dynamic lights into lighting
  clusters consumed by the forward renderer.
- **Skybox System** (``Skybox``) -- renders the environment skybox / cubemap.
- **Shadow System** (``Shadow``) -- renders shadow maps for shadow-casting
  lights.
- **Audio System** (``Audio``) -- updates and plays spatial and non-spatial
  audio sources.

These systems are registered through the engine's system factory (under the
names listed above), which is also what lets scenes and the editor instantiate
them. They appear together with any user-defined systems in the editor's
**Systems** panel.
