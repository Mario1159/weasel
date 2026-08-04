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
/// Returns true if the entity ID refers to a live entity.
def entity_is_null(entity : uint) : bool => true
/// Returns the null entity sentinel value.
def null_entity() : uint => 0

// --- Generic component queries ---
/// Returns true if the entity owns a component with the given type ID.
def has_component(type_id : uint, entity : uint) : bool => false

// --- Generic component add/remove ---
/// Adds a component by type ID to the entity. Returns true on success.
def add_component(type_id : uint, entity : uint) : bool => false
/// Removes a component by type ID from the entity. Returns true on success.
def remove_component(type_id : uint, entity : uint) : bool => false

// --- Per-component add/remove ---
/// Adds a Transform component to the entity. Returns true on success.
def add_transform(entity : uint) : bool => false
/// Removes the Transform component from the entity. Returns true on success.
def remove_transform(entity : uint) : bool => false
/// Adds a Camera component to the entity. Returns true on success.
def add_camera(entity : uint) : bool => false
/// Removes the Camera component from the entity. Returns true on success.
def remove_camera(entity : uint) : bool => false
/// Adds a Hierarchy component to the entity. Returns true on success.
def add_hierarchy(entity : uint) : bool => false
/// Removes the Hierarchy component from the entity. Returns true on success.
def remove_hierarchy(entity : uint) : bool => false
/// Adds a World Transform component to the entity. Returns true on success.
def add_world_transform(entity : uint) : bool => false
/// Removes the World Transform component from the entity. Returns true on success.
def remove_world_transform(entity : uint) : bool => false

// --- Transform operations ---
/// Sets the entity's local position.
def set_position(entity : uint, x : float, y : float, z : float) { }
/// Sets the entity's rotation from Euler angles in degrees.
def set_rotation(entity : uint, pitch : float, yaw : float, roll : float) { }
/// Sets the entity's local scale.
def set_scale(entity : uint, x : float, y : float, z : float) { }
/// Returns the X component of the entity's local position.
def get_position_x(entity : uint) : float => 0.0
/// Returns the Y component of the entity's local position.
def get_position_y(entity : uint) : float => 0.0
/// Returns the Z component of the entity's local position.
def get_position_z(entity : uint) : float => 0.0
/// Returns the pitch (X) component of the entity's rotation in degrees.
def get_rotation_pitch(entity : uint) : float => 0.0
/// Returns the yaw (Y) component of the entity's rotation in degrees.
def get_rotation_yaw(entity : uint) : float => 0.0
/// Returns the roll (Z) component of the entity's rotation in degrees.
def get_rotation_roll(entity : uint) : float => 0.0
/// Returns the X component of the entity's local scale.
def get_scale_x(entity : uint) : float => 0.0
/// Returns the Y component of the entity's local scale.
def get_scale_y(entity : uint) : float => 0.0
/// Returns the Z component of the entity's local scale.
def get_scale_z(entity : uint) : float => 0.0

// --- Scene operations ---
/// Finds an entity by its scene name. Returns null if not found.
def find_entity_by_name(name : string) : uint => 0
/// Sets the name of an entity in the scene.
def set_entity_name(entity : uint, name : string) { }
/// Returns the entity ID of the active camera.
def get_active_camera() : uint => 0
/// Sets the active camera to the given entity.
def set_active_camera(entity : uint) { }
/// Instantiates a prefab scene and returns the root entity of the copy.
def instantiate_prefab(path : string) : uint => 0

// --- Component type ID constants ---
/// Stable type ID for the Transform component.
def TYPE_TRANSFORM() : uint => 0
/// Stable type ID for the Camera component.
def TYPE_CAMERA() : uint => 0
/// Stable type ID for the Hierarchy component.
def TYPE_HIERARCHY() : uint => 0
/// Stable type ID for the World Transform component.
def TYPE_WORLD_TRANSFORM() : uint => 0

// --- Camera field access ---
/// Returns the field of view of the camera in degrees.
def get_camera_fov(camera : uint) : float => 60.0
/// Sets the field of view of the camera in degrees.
def set_camera_fov(camera : uint, fov : float) { }
/// Returns the near clip plane of the camera.
def get_camera_near(camera : uint) : float => 0.5
/// Sets the near clip plane of the camera.
def set_camera_near(camera : uint, near_val : float) { }
/// Returns the far clip plane of the camera.
def get_camera_far(camera : uint) : float => 50.0
/// Sets the far clip plane of the camera.
def set_camera_far(camera : uint, far_val : float) { }
/// Returns the aspect ratio of the camera.
def get_camera_aspect_ratio(camera : uint) : float => 1.0
/// Sets the aspect ratio of the camera.
def set_camera_aspect_ratio(camera : uint, aspect : float) { }

// --- Event query functions ---
/// Returns the type of the current event (see EVENT_* constants).
def get_event_kind() : uint => 0
/// Returns the horizontal mouse delta for mouse motion events.
def get_event_mouse_dx() : float => 0.0
/// Returns the vertical mouse delta for mouse motion events.
def get_event_mouse_dy() : float => 0.0
/// Returns the mouse X position for button events.
def get_event_mouse_x() : int => 0
/// Returns the mouse Y position for button events.
def get_event_mouse_y() : int => 0
/// Returns which mouse button was pressed/released.
def get_event_mouse_button() : int => 0

// --- Keyboard event functions ---
/// Returns the SDL scancode of the key for key events.
def get_event_key_scancode() : int => 0
/// Returns the SDL keycode of the key for key events.
def get_event_key_keycode() : int => 0
/// Returns true if the key event is a repeat.
def get_event_key_repeat() : bool => false

// --- Keyboard state ---
/// Returns true if the given SDL scancode is currently pressed.
def is_key_pressed(scancode : int) : bool => false

// --- SDL window operations ---
/// Enables or disables relative mouse mode (FPS-style mouse). Returns true on success.
def set_relative_mouse_mode(enabled : bool) : bool => false
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

// --- Editor viewport ---
/// Queries the editor for the game-view image bounds (call before get_editor_img_*).
def refresh_editor_viewport() { }
/// Returns the X position of the editor game-view image. Call refresh_editor_viewport first.
def get_editor_img_min_x() : float => 0.0
/// Returns the Y position of the editor game-view image. Call refresh_editor_viewport first.
def get_editor_img_min_y() : float => 0.0
/// Returns the width of the editor game-view image. Call refresh_editor_viewport first.
def get_editor_img_size_x() : float => 0.0
/// Returns the height of the editor game-view image. Call refresh_editor_viewport first.
def get_editor_img_size_y() : float => 0.0

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

// --- Component type lookup (for generic dispatch) ---

/// Component kind enum for generic dispatch.
enum ComponentKind : int {
    CPP_NATIVE = 0
    CPP_PROJECT = 1
    DAS_SCRIPT = 2
}

/// Looks up component type ID by daScript type name. Returns 0 if not found.
def _get_component_type_id_by_name(type_name : string) : uint => 0

/// Looks up component kind by daScript type name. Returns 0 (CPP_NATIVE) if not found.
def _get_component_kind_by_name(type_name : string) : int => 0

/// Looks up component struct size by daScript type name. Returns 0 if not found.
def _get_component_struct_size_by_name(type_name : string) : int => 0

/// Type-erased component getter.
/// Fills dest with the component data for the given entity.
def _get_component_into(entity : uint, type_id : uint, kind : int, dest : void?) { }

/// Type-erased component setter.
/// Writes src to component storage for the given entity.
def _set_component_from(entity : uint, type_id : uint, kind : int, src : void?) { }

// --- Raycasting ---
/// Casts a ray from the camera through the given screen coordinates.
def make_pick_ray(camera_entity : uint, mouse_x : float, mouse_y : float, vp_x : float, vp_y : float, vp_w : float, vp_h : float) : bool => false
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
/// Intersects a ray with a plane defined by a point and normal. Returns true if hit.
def ray_plane_intersect(origin_x : float, origin_y : float, origin_z : float, dir_x : float, dir_y : float, dir_z : float, plane_x : float, plane_y : float, plane_z : float, plane_nx : float, plane_ny : float, plane_nz : float) : bool => false
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
/// Key pressed.
let EVENT_KEY_DOWN : uint = 5
/// Key released.
let EVENT_KEY_UP : uint = 6
/// Application quit requested.
let EVENT_QUIT : uint = 0

// --- SDL button constants ---
/// Left mouse button.
let SDL_BUTTON_LEFT : uint = 1
/// Right mouse button.
let SDL_BUTTON_RIGHT : uint = 3

// --- SDL scancode constants ---
/// Escape key scancode.
let SDL_SCANCODE_ESCAPE : int = 41
