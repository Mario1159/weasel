weasel_api
==========

.. das:module:: weasel_api

Constants
----------

.. das:data:: SDL_BUTTON_LEFT

   :type: uint32_t

   Value: ``1``

.. das:data:: SDL_BUTTON_RIGHT

   :type: uint32_t

   Value: ``3``

.. das:data:: SDL_SCANCODE_ESCAPE

   :type: int32_t

   Value: ``41``

.. das:data:: EVENT_MOUSE_MOTION

   :type: uint32_t

   Value: ``2 (mouse_motion``

.. das:data:: EVENT_MOUSE_BUTTON_DOWN

   :type: uint32_t

   Value: ``3 (mouse_button_down``

.. das:data:: EVENT_MOUSE_BUTTON_UP

   :type: uint32_t

   Value: ``4 (mouse_button_up``

.. das:data:: EVENT_QUIT

   :type: uint32_t

   Value: ``0 (quit``

.. das:data:: EVENT_KEY_DOWN

   :type: uint32_t

   Value: ``static_cast<uint32_t> (event_kind::key_down``

.. das:data:: EVENT_KEY_UP

   :type: uint32_t

   Value: ``static_cast<uint32_t> (event_kind::key_up``

Component accessor proxies
--------------------------

Component data is read and written through live proxy values created with
``get_component(entity, Tag)``.  The proxy captures the component pointer
once at call time; every later property access reads or writes the actual
entity component in place.  Example::

    def on_update (entity : uint) {
        let t = get_component(entity, transform())
        t.scale.x = 2.0                    // chained leaf write
        t.scale = float3(1.0, 2.0, 3.0)    // full-struct write
        t.position.y = 3.5
        let sx = t.scale.x                 // chained read
    }

The available tags and their properties:

.. list-table::
   :header-rows: 1

   * - Tag function
     - Proxy type
     - Properties
   * - ``transform()``
     - ``TransformAccessor``
     - ``position.x/y/z``, ``scale.x/y/z``
   * - ``transform_2d()``
     - ``Transform2DAccessor``
     - ``position.x/y``, ``scale.x/y``, ``rotation``
   * - ``camera_2d()``
     - ``Camera2DAccessor``
     - ``zoom``
   * - ``sprite_2d()``
     - ``Sprite2DAccessor``
     - ``color.x/y/z/w``, ``size.x/y``
   * - ``point_light()``
     - ``PointLightAccessor``
     - ``color.x/y/z``, ``intensity``
   * - ``directional_light()``
     - ``DirectionalLightAccessor``
     - ``color.x/y/z``, ``intensity``
   * - ``spot_light()``
     - ``SpotLightAccessor``
     - ``color.x/y/z``, ``intensity``

A proxy stays valid only while the entity is alive and still has the
component.  Re-call ``get_component`` after structural changes to the
entity.

Entity operations
-----------------

Functions for creating, destroying, and querying entities.

.. das:function:: entity_create() : uint

   Creates a new empty entity and returns its ID.

   :returns: uint

.. das:function:: entity_destroy(entity : uint)

   Destroys an entity and all of its components.

   :param:
      entity (uint)

.. das:function:: entity_valid(entity : uint) : bool

   Returns ``true`` if the entity ID refers to a live entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: entity_is_null(entity : uint) : bool

   Returns ``true`` if the entity ID is the null sentinel.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: null_entity() : uint

   Returns the null entity sentinel value.

   :returns: uint

Generic component queries
-------------------------

Generic queries that work with any component type.

.. das:function:: has_component(type_id : uint, entity : uint) : bool

   Returns ``true`` if the entity owns a component with the given type ID.

   :param:
      type_id (uint)
      entity (uint)

   :returns: bool

Scene operations
----------------

Query and modify the active scene (find entities, cameras).

.. das:function:: find_entity_by_name(entity : var) : uint

   Finds an entity by its scene name. Returns null if not found.

   :param:
      entity (var)

   :returns: uint

.. das:function:: set_entity_name(entity : uint, name : var)

   :param:
      entity (uint)
      name (var)

.. das:function:: get_active_camera() : uint

   Returns the entity ID of the active camera.

   :returns: uint

.. das:function:: set_active_camera(entity : uint)

   Sets the active camera to the given entity.

   :param:
      entity (uint)

.. das:function:: instantiate_prefab(path : var) : uint

   :param:
      path (var)

   :returns: uint

Component type ID constants
---------------------------

Stable type IDs for built-in component types.

.. das:function:: TYPE_TRANSFORM() : uint

   Stable type ID for the Transform component.

   :returns: uint

.. das:function:: TYPE_CAMERA() : uint

   Stable type ID for the Camera component.

   :returns: uint

.. das:function:: TYPE_HIERARCHY() : uint

   Stable type ID for the Hierarchy component.

   :returns: uint

.. das:function:: TYPE_WORLD_TRANSFORM() : uint

   Stable type ID for the World Transform component.

   :returns: uint

.. das:function:: TYPE_TRANSFORM_2D() : uint

   :returns: uint

.. das:function:: TYPE_CAMERA_2D() : uint

   :returns: uint

.. das:function:: TYPE_SPRITE_2D() : uint

   :returns: uint

.. das:function:: TYPE_POINT_LIGHT() : uint

   :returns: uint

.. das:function:: TYPE_DIRECTIONAL_LIGHT() : uint

   :returns: uint

.. das:function:: TYPE_SPOT_LIGHT() : uint

   :returns: uint

.. das:function:: TYPE_RIGID_BODY() : uint

   :returns: uint

.. das:function:: TYPE_CHARACTER_BODY() : uint

   :returns: uint

.. das:function:: TYPE_MODEL_INSTANCE_3D() : uint

   :returns: uint

.. das:function:: TYPE_AREA_3D() : uint

   :returns: uint

.. das:function:: TYPE_AUDIO() : uint

   :returns: uint

.. das:function:: TYPE_PREFAB_INSTANCE() : uint

   :returns: uint

.. das:function:: TYPE_SUBVIEWPORT() : uint

   :returns: uint

Event query functions
---------------------

Query input events (mouse motion, button clicks).

.. das:function:: get_event_kind() : uint

   Returns the type of the current event (see EVENT_* constants).

   :returns: uint

.. das:function:: get_event_mouse_dx() : float

   Returns the horizontal mouse delta for mouse motion events.

   :returns: float

.. das:function:: get_event_mouse_dy() : float

   Returns the vertical mouse delta for mouse motion events.

   :returns: float

.. das:function:: get_event_mouse_x() : int

   Returns the mouse X position for button events.

   :returns: int

.. das:function:: get_event_mouse_y() : int

   Returns the mouse Y position for button events.

   :returns: int

.. das:function:: get_event_mouse_button() : uint

   Returns which mouse button was pressed/released.

   :returns: uint

SDL window operations
---------------------

Window management: cursor, mouse mode, window size.

.. das:function:: set_relative_mouse_mode(enabled : bool) : bool

   Enables or disables relative mouse mode (FPS-style mouse).

   :param:
      enabled (bool)

   :returns: bool

.. das:function:: cursor_visible() : bool

   Returns ``true`` if the cursor is currently visible.

   :returns: bool

.. das:function:: show_cursor()

   Shows the system cursor.

.. das:function:: hide_cursor()

   Hides the system cursor.

Component type lookup
---------------------

Look up component type IDs at runtime.

.. das:function:: get_component_type_id(type_ids : var) : uint

   Looks up a component type ID by its display name (e.g. ``"Transform"``).

   :param:
      type_ids (var)

   :returns: uint

Model instance
--------------

Swap models and material overrides on entities.

.. das:function:: log_info(msg : var)

   Logs a message at info level.

   :param:
      msg (var)

.. das:function:: log_debug(msg : var)

   Logs a message at debug level.

   :param:
      msg (var)

.. das:function:: log_warn(msg : var)

   Logs a message at warning level.

   :param:
      msg (var)

.. das:function:: log_error(msg : var)

   Logs a message at error level.

   :param:
      msg (var)

.. das:function:: set_model(entity : uint, path : var)

   Sets the model on the entity from a resource path (e.g. "res://...").

   :param:
      entity (uint)
      path (var)

.. das:function:: set_model_material_override(entity : uint, path : var)

   Sets a per-instance material override from a resource path.

   :param:
      entity (uint)
      path (var)

.. das:function:: set_model_visibility_range(entity : uint, range : float)

   Sets the model max draw distance in world units (0 = unlimited).

   :param:
      entity (uint)
      range (float)

Generic component add/remove
----------------------------

.. das:function:: add_component(name : uint) : bool

   :param:
      name (uint)

   :returns: bool

.. das:function:: remove_component(entity : uint, name : uint) : bool

   :param:
      entity (uint)
      name (uint)

   :returns: bool

Editor viewport (global-state)
------------------------------

.. das:function:: refresh_editor_viewport()

.. das:function:: get_editor_img_min_x() : float

   :returns: float

.. das:function:: get_editor_img_min_y() : float

   :returns: float

.. das:function:: get_editor_img_size_x() : float

   :returns: float

.. das:function:: get_editor_img_size_y() : float

   :returns: float

Keyboard event functions
------------------------

.. das:function:: get_event_key_scancode() : int

   :returns: int

.. das:function:: get_event_key_keycode() : int

   :returns: int

.. das:function:: get_event_key_repeat() : bool

   :returns: bool

Keyboard state
--------------

.. das:function:: is_key_pressed(camera : int) : bool

   :param:
      camera (int)

   :returns: bool

Camera field access
-------------------

.. das:function:: get_camera_fov(camera : uint) : float

   :param:
      camera (uint)

   :returns: float

.. das:function:: set_camera_fov(camera : uint, fov : float)

   :param:
      camera (uint)
      fov (float)

.. das:function:: get_camera_near(camera : uint) : float

   :param:
      camera (uint)

   :returns: float

.. das:function:: set_camera_near(camera : uint, near_val : float)

   :param:
      camera (uint)
      near_val (float)

.. das:function:: get_camera_far(camera : uint) : float

   :param:
      camera (uint)

   :returns: float

.. das:function:: set_camera_far(camera : uint, far_val : float)

   :param:
      camera (uint)
      far_val (float)

.. das:function:: get_camera_aspect_ratio(camera : uint) : float

   :param:
      camera (uint)

   :returns: float

.. das:function:: set_camera_aspect_ratio(camera : uint, aspect : float)

   :param:
      camera (uint)
      aspect (float)

Window size (global-state)
--------------------------

.. das:function:: refresh_window_size()

   Queries the OS for the current window size (must call before get_window_width/Height).

.. das:function:: get_window_width() : uint

   Returns the window width. Call ``refresh_window_size`` first.

   :returns: uint

.. das:function:: get_window_height() : uint

   Returns the window height. Call ``refresh_window_size`` first.

   :returns: uint

Other
-----

.. das:function:: each_entity_id_with()

Generic component type lookup
-----------------------------

.. das:function:: _get_component_type_id_by_name(entity : var) : uint

   :param:
      entity (var)

   :returns: uint

.. das:function:: _get_component_data(entity : uint, type_id : uint)

   :param:
      entity (uint)
      type_id (uint)

Raycasting (global-state)
-------------------------

.. das:function:: make_pick_ray(camera_entity : uint, mouse_x : float, mouse_y : float, vp_x : float, vp_y : float, vp_w : float, vp_h : float) : bool

   Casts a ray from the camera through the given screen coordinates.

   :param:
      camera_entity (uint)
      mouse_x (float)
      mouse_y (float)
      vp_x (float)
      vp_y (float)
      vp_w (float)
      vp_h (float)

   :returns: bool

.. das:function:: get_ray_origin_x() : float

   Returns the X origin of the last pick ray.

   :returns: float

.. das:function:: get_ray_origin_y() : float

   Returns the Y origin of the last pick ray.

   :returns: float

.. das:function:: get_ray_origin_z() : float

   Returns the Z origin of the last pick ray.

   :returns: float

.. das:function:: get_ray_dir_x() : float

   Returns the X direction of the last pick ray.

   :returns: float

.. das:function:: get_ray_dir_y() : float

   Returns the Y direction of the last pick ray.

   :returns: float

.. das:function:: get_ray_dir_z() : float

   Returns the Z direction of the last pick ray.

   :returns: float

.. das:function:: ray_plane_intersect(origin_x : float, origin_y : float, origin_z : float, dir_x : float, dir_y : float, dir_z : float, plane_x : float, plane_y : float, plane_z : float, plane_nx : float, plane_ny : float, plane_nz : float) : bool

   Intersects the last pick ray with a plane. Returns true if hit.

   :param:
      origin_x (float)
      origin_y (float)
      origin_z (float)
      dir_x (float)
      dir_y (float)
      dir_z (float)
      plane_x (float)
      plane_y (float)
      plane_z (float)
      plane_nx (float)
      plane_ny (float)
      plane_nz (float)

   :returns: bool

.. das:function:: get_hit_x() : float

   Returns the X coordinate of the last ray-plane intersection.

   :returns: float

.. das:function:: get_hit_y() : float

   Returns the Y coordinate of the last ray-plane intersection.

   :returns: float

.. das:function:: get_hit_z() : float

   Returns the Z coordinate of the last ray-plane intersection.

   :returns: float

Time
----

Monotonic clock accessible from any system callback.

.. das:function:: get_time() : float

   Returns the elapsed time in seconds since the module was first queried.

   :returns: float

.. das:function:: get_elapsed_time() : float

   Returns the elapsed time in seconds since the module was first queried.

   :returns: float

Physics: rigid body
-------------------

.. das:function:: apply_impulse(entity : uint, x : float, y : float, z : float)

   Applies an impulse to the rigid body.

   :param:
      entity (uint)
      x (float)
      y (float)
      z (float)

.. das:function:: apply_force(entity : uint, x : float, y : float, z : float)

   Applies a continuous force to the rigid body.

   :param:
      entity (uint)
      x (float)
      y (float)
      z (float)

Audio
-----

Control playback of entity audio components.

.. das:function:: audio_play(entity : uint)

   Starts playback of the entity's audio component.

   :param:
      entity (uint)

.. das:function:: audio_stop(entity : uint)

   Stops playback of the entity's audio component.

   :param:
      entity (uint)

.. das:function:: audio_pause(entity : uint)

   Pauses playback of the entity's audio component.

   :param:
      entity (uint)

.. das:function:: audio_resume(entity : uint)

   Resumes playback of the entity's audio component.

   :param:
      entity (uint)

.. das:function:: audio_set_volume(entity : uint, volume : float)

   Sets the playback volume (0.0 to 1.0) of the entity's audio component.

   :param:
      entity (uint)
      volume (float)

