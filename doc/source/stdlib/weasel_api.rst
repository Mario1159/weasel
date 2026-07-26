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

Per-component add/remove
------------------------

Add or remove specific components from entities.

.. das:function:: add_transform(entity : uint) : bool

   Adds a Transform component to the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: remove_transform(entity : uint) : bool

   Removes the Transform component from the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: add_camera(entity : uint) : bool

   Adds a Camera component to the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: remove_camera(entity : uint) : bool

   Removes the Camera component from the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: add_hierarchy(entity : uint) : bool

   Adds a Hierarchy component to the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: remove_hierarchy(entity : uint) : bool

   Removes the Hierarchy component from the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: add_world_transform(entity : uint) : bool

   Adds a World Transform component to the entity.

   :param:
      entity (uint)

   :returns: bool

.. das:function:: remove_world_transform(entity : uint) : bool

   Removes the World Transform component from the entity.

   :param:
      entity (uint)

   :returns: bool

Scene operations
----------------

Query and modify the active scene (find entities, cameras).

.. das:function:: find_entity_by_name(name : var) : uint

   Finds an entity by its scene name. Returns null if not found.

   :param:
      name (var)

   :returns: uint

.. das:function:: get_active_camera() : uint

   Returns the entity ID of the active camera.

   :returns: uint

.. das:function:: set_active_camera(entity : uint)

   Sets the active camera to the given entity.

   :param:
      entity (uint)

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

Entity iteration by component
-----------------------------

Iterate over entities that own a given component type.

.. das:function:: refresh_entities_with_component(entity : uint)

   Populates the entity buffer with all entities owning the given component type.

   :param:
      entity (uint)

Component type lookup
---------------------

Look up component type IDs at runtime.

.. das:function:: get_component_type_id(display_name : var) : uint

   Looks up a component type ID by its display name (e.g. ``"Transform"``).

   :param:
      display_name (var)

   :returns: uint

Component field access
----------------------

Read and write component fields by byte offset.

.. das:function:: get_component_field_f(entity : uint, type_id : uint, offset : int) : float

   Reads a float field from a component at the given byte offset.

   :param:
      entity (uint)
      type_id (uint)
      offset (int)

   :returns: float

.. das:function:: set_component_field_f(entity : uint, type_id : uint, offset : int, value : float)

   Writes a float field to a component at the given byte offset.

   :param:
      entity (uint)
      type_id (uint)
      offset (int)
      value (float)

Raycasting (global-state)
-------------------------

.. das:function:: get_delta_time() : float

   Returns the time elapsed since the last frame, in seconds.

   :returns: float

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

Transform operations (global-state)
-----------------------------------

.. das:function:: get_position(entity : uint)

   Reads the entity's local position. Use ``get_transform_x/y/z`` to extract components.

   :param:
      entity (uint)

.. das:function:: set_position(entity : uint, x : float, y : float, z : float)

   Sets the entity's local position.

   :param:
      entity (uint)
      x (float)
      y (float)
      z (float)

.. das:function:: get_rotation(entity : uint)

   Reads the entity's rotation as Euler angles (pitch, yaw, roll) in degrees.

   :param:
      entity (uint)

.. das:function:: set_rotation(entity : uint, pitch : float, yaw : float, roll : float)

   Sets the entity's rotation from Euler angles in degrees.

   :param:
      entity (uint)
      pitch (float)
      yaw (float)
      roll (float)

.. das:function:: get_scale(entity : uint)

   Reads the entity's local scale. Use ``get_transform_x/y/z`` to extract components.

   :param:
      entity (uint)

.. das:function:: set_scale(entity : uint, x : float, y : float, z : float)

   Sets the entity's local scale.

   :param:
      entity (uint)
      x (float)
      y (float)
      z (float)

.. das:function:: get_transform_x() : float

   Returns the X component of the last ``get_position``/``get_scale`` call.

   :returns: float

.. das:function:: get_transform_y() : float

   Returns the Y component of the last ``get_position``/``get_scale`` call.

   :returns: float

.. das:function:: get_transform_z() : float

   Returns the Z component of the last ``get_position``/``get_scale`` call.

   :returns: float

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

Entity iteration (global-state)
-------------------------------

.. das:function:: refresh_entities_with_transform()

   Populates the entity buffer with all entities that have a Transform.

.. das:function:: get_entity_count() : uint

   Returns the number of entities in the current iteration buffer.

   :returns: uint

.. das:function:: get_entity_at(index : uint) : uint

   Returns the entity ID at the given index in the iteration buffer.

   :param:
      index (uint)

   :returns: uint

