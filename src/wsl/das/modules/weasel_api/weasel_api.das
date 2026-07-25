options gen2
module weasel_api

// Weasel API declarations for AOT compilation.
// Stub implementations - at runtime the C++ engine provides the real ones.

def get_delta_time() : float => 0.0
def log_info(msg : string) { }
def log_debug(msg : string) { }
def log_warn(msg : string) { }
def log_error(msg : string) { }
def entity_create() : uint => 0
def entity_destroy(entity : uint) { }
def entity_valid(entity : uint) : bool => false
def entity_is_null(entity : uint) : bool => true
def null_entity() : uint => 0
def has_component(type_id : uint, entity : uint) : bool => false
def add_transform(entity : uint) { }
def remove_transform(entity : uint) { }
def add_camera(entity : uint) { }
def remove_camera(entity : uint) { }
def add_hierarchy(entity : uint) { }
def remove_hierarchy(entity : uint) { }
def add_world_transform(entity : uint) { }
def remove_world_transform(entity : uint) { }
def get_position(entity : uint) { }
def set_position(entity : uint, x : float, y : float, z : float) { }
def get_rotation(entity : uint) { }
def set_rotation(entity : uint, pitch : float, yaw : float, roll : float) { }
def get_scale(entity : uint) { }
def set_scale(entity : uint, x : float, y : float, z : float) { }
def get_transform_x() : float => 0.0
def get_transform_y() : float => 0.0
def get_transform_z() : float => 0.0
def find_entity_by_name(name : string) : uint => 0
def get_active_camera() : uint => 0
def set_active_camera(entity : uint) { }
def TYPE_TRANSFORM() : uint => 0
def TYPE_CAMERA() : uint => 0
def TYPE_HIERARCHY() : uint => 0
def TYPE_WORLD_TRANSFORM() : uint => 0
def get_event_kind() : uint => 0
def get_event_mouse_dx() : float => 0.0
def get_event_mouse_dy() : float => 0.0
def get_event_mouse_x() : float => 0.0
def get_event_mouse_y() : float => 0.0
def get_event_mouse_button() : int => 0
def set_relative_mouse_mode(enabled : bool) { }
def cursor_visible() : bool => true
def show_cursor() { }
def hide_cursor() { }
def refresh_window_size() { }
def get_window_width() : int => 0
def get_window_height() : int => 0
def refresh_entities_with_transform() { }
def get_entity_count() : uint => 0
def get_entity_at(index : uint) : uint => 0
def get_component_type_id(display_name : string) : uint => 0
def refresh_entities_with_component(type_id : uint) { }
def get_component_field_f(entity : uint, type_id : uint, offset : int) : float => 0.0
def set_component_field_f(entity : uint, type_id : uint, offset : int, value : float) { }
def make_pick_ray(x : float, y : float) { }
def get_ray_origin_x() : float => 0.0
def get_ray_origin_y() : float => 0.0
def get_ray_origin_z() : float => 0.0
def get_ray_dir_x() : float => 0.0
def get_ray_dir_y() : float => 0.0
def get_ray_dir_z() : float => 0.0
def ray_plane_intersect(plane_y : float) : bool => false
def get_hit_x() : float => 0.0
def get_hit_y() : float => 0.0
def get_hit_z() : float => 0.0

// Event constants
let EVENT_MOUSE_MOTION : uint = 2
let EVENT_MOUSE_BUTTON_DOWN : uint = 3
let EVENT_MOUSE_BUTTON_UP : uint = 4
let EVENT_QUIT : uint = 0
