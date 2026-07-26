options gen2
module weasel_api

// Weasel API declarations for AOT compilation.
// Stub implementations - at runtime the C++ engine provides the real ones.

// --- Time ---
/// Returns the time elapsed since the last frame, in seconds.
def get_delta_time() : float => 0.0

// --- Logging ---
/// Logs a message at info level.
def log_info(msg : string) { }
/// Logs a message at debug level.
def log_debug(msg : string) { }
/// Logs a message at warning level.
def log_warn(msg : string) { }
/// Logs a message at error level.
def log_error(msg : string) { }

// --- Entity operations ---
/// Creates a new empty entity and returns its ID.
def entity_create() : uint => 0
/// Destroys an entity and all of its components.
def entity_destroy(entity : uint) { }
/// Returns true if the entity ID refers to a live entity.
def entity_valid(entity : uint) : bool => false
/// Returns true if the entity ID is the null sentinel.
def entity_is_null(entity : uint) : bool => true
/// Returns the null entity sentinel value.
def null_entity() : uint => 0

// --- Generic component queries ---
/// Returns true if the entity owns a component with the given type ID.
def has_component(type_id : uint, entity : uint) : bool => false

// --- Per-component add/remove ---
/// Adds a Transform component to the entity.
def add_transform(entity : uint) { }
/// Removes the Transform component from the entity.
def remove_transform(entity : uint) { }
/// Adds a Camera component to the entity.
def add_camera(entity : uint) { }
/// Removes the Camera component from the entity.
def remove_camera(entity : uint) { }
/// Adds a Hierarchy component to the entity.
def add_hierarchy(entity : uint) { }
/// Removes the Hierarchy component from the entity.
def remove_hierarchy(entity : uint) { }
/// Adds a World Transform component to the entity.
def add_world_transform(entity : uint) { }
/// Removes the World Transform component from the entity.
def remove_world_transform(entity : uint) { }

// --- Transform operations ---
/// Reads the entity's local position. Use get_transform_x/y/z to extract components.
def get_position(entity : uint) { }
/// Sets the entity's local position.
def set_position(entity : uint, x : float, y : float, z : float) { }
/// Reads the entity's rotation as Euler angles (pitch, yaw, roll) in degrees.
def get_rotation(entity : uint) { }
/// Sets the entity's rotation from Euler angles in degrees.
def set_rotation(entity : uint, pitch : float, yaw : float, roll : float) { }
/// Reads the entity's local scale. Use get_transform_x/y/z to extract components.
def get_scale(entity : uint) { }
/// Sets the entity's local scale.
def set_scale(entity : uint, x : float, y : float, z : float) { }
/// Returns the X component of the last get_position/get_scale call.
def get_transform_x() : float => 0.0
/// Returns the Y component of the last get_position/get_scale call.
def get_transform_y() : float => 0.0
/// Returns the Z component of the last get_position/get_scale call.
def get_transform_z() : float => 0.0

// --- Scene operations ---
/// Finds an entity by its scene name. Returns null if not found.
def find_entity_by_name(name : string) : uint => 0
/// Returns the entity ID of the active camera.
def get_active_camera() : uint => 0
/// Sets the active camera to the given entity.
def set_active_camera(entity : uint) { }

// --- Component type ID constants ---
/// Stable type ID for the Transform component.
def TYPE_TRANSFORM() : uint => 0
/// Stable type ID for the Camera component.
def TYPE_CAMERA() : uint => 0
/// Stable type ID for the Hierarchy component.
def TYPE_HIERARCHY() : uint => 0
/// Stable type ID for the World Transform component.
def TYPE_WORLD_TRANSFORM() : uint => 0

// --- Event query functions ---
/// Returns the type of the current event (see EVENT_* constants).
def get_event_kind() : uint => 0
/// Returns the horizontal mouse delta for mouse motion events.
def get_event_mouse_dx() : float => 0.0
/// Returns the vertical mouse delta for mouse motion events.
def get_event_mouse_dy() : float => 0.0
/// Returns the mouse X position for button events.
def get_event_mouse_x() : float => 0.0
/// Returns the mouse Y position for button events.
def get_event_mouse_y() : float => 0.0
/// Returns which mouse button was pressed/released.
def get_event_mouse_button() : int => 0

// --- SDL window operations ---
/// Enables or disables relative mouse mode (FPS-style mouse).
def set_relative_mouse_mode(enabled : bool) { }
/// Returns true if the cursor is currently visible.
def cursor_visible() : bool => true
/// Shows the system cursor.
def show_cursor() { }
/// Hides the system cursor.
def hide_cursor() { }

// --- Window size ---
/// Queries the OS for the current window size (must call before get_window_width/Height).
def refresh_window_size() { }
/// Returns the window width. Call refresh_window_size first.
def get_window_width() : int => 0
/// Returns the window height. Call refresh_window_size first.
def get_window_height() : int => 0

// --- Entity iteration ---
/// Populates the entity buffer with all entities that have a Transform.
def refresh_entities_with_transform() { }
/// Returns the number of entities in the current iteration buffer.
def get_entity_count() : uint => 0
/// Returns the entity ID at the given index in the iteration buffer.
def get_entity_at(index : uint) : uint => 0

// --- Component type lookup ---
/// Looks up a component type ID by its display name (e.g. "Transform").
def get_component_type_id(display_name : string) : uint => 0

// --- Entity iteration by component ---
/// Populates the entity buffer with all entities owning the given component type.
def refresh_entities_with_component(type_id : uint) { }

// --- Component field access ---
/// Reads a float field from a component at the given byte offset.
def get_component_field_f(entity : uint, type_id : uint, offset : int) : float => 0.0
/// Writes a float field to a component at the given byte offset.
def set_component_field_f(entity : uint, type_id : uint, offset : int, value : float) { }

// --- Raycasting ---
/// Casts a ray from the camera through the given screen coordinates.
def make_pick_ray(x : float, y : float) { }
/// Returns the X origin of the last pick ray.
def get_ray_origin_x() : float => 0.0
/// Returns the Y origin of the last pick ray.
def get_ray_origin_y() : float => 0.0
/// Returns the Z origin of the last pick ray.
def get_ray_origin_z() : float => 0.0
/// Returns the X direction of the last pick ray.
def get_ray_dir_x() : float => 0.0
/// Returns the Y direction of the last pick ray.
def get_ray_dir_y() : float => 0.0
/// Returns the Z direction of the last pick ray.
def get_ray_dir_z() : float => 0.0
/// Intersects the last pick ray with a plane. Returns true if hit.
def ray_plane_intersect(plane_y : float) : bool => false
/// Returns the X coordinate of the last ray-plane intersection.
def get_hit_x() : float => 0.0
/// Returns the Y coordinate of the last ray-plane intersection.
def get_hit_y() : float => 0.0
/// Returns the Z coordinate of the last ray-plane intersection.
def get_hit_z() : float => 0.0

// --- Event constants ---
/// Mouse moved (no button pressed).
let EVENT_MOUSE_MOTION : uint = 2
/// Mouse button pressed.
let EVENT_MOUSE_BUTTON_DOWN : uint = 3
/// Mouse button released.
let EVENT_MOUSE_BUTTON_UP : uint = 4
/// Application quit requested.
let EVENT_QUIT : uint = 0
