require weasel_api
require weasel_ecs
require math

class System : EcsSystem {
    m_fp_camera : uint = 0
    m_goban_camera : uint = 0
    m_is_black_turn : bool = true

    def override on_init() : void {
        refresh_cameras()
        set_relative_mouse_mode(true)
        hide_cursor()
    }

    def override on_inactive() : void {
        set_relative_mouse_mode(false)
    }

    def override on_event() : void {
        var kind = get_event_kind()

        if (kind == EVENT_KEY_DOWN) {
            var scancode = get_event_key_scancode()
            if (scancode == SDL_SCANCODE_ESCAPE) {
                refresh_cameras()
                var active = get_active_camera()
                if (active == m_goban_camera) {
                    switch_to_camera(m_fp_camera)
                }
            }
        } elif (kind == EVENT_MOUSE_BUTTON_DOWN) {
            var btn = get_event_mouse_button()

            if (btn == int(SDL_BUTTON_LEFT)) {
                refresh_cameras()
                var active = get_active_camera()
                if (active != m_goban_camera) {
                    switch_to_camera(m_goban_camera)
                } else {
                    var mx = get_event_mouse_x()
                    var my = get_event_mouse_y()
                    place_stone_at(mx, my)
                }
            } elif (btn == int(SDL_BUTTON_RIGHT)) {
                refresh_cameras()
                var active = get_active_camera()
                if (active == m_goban_camera) {
                    var mx = get_event_mouse_x()
                    var my = get_event_mouse_y()
                    remove_stone_at(mx, my)
                }
            }
        }
    }

    def override on_update(dt : float) : void {
        refresh_cameras()

        var active = get_active_camera()
        var on_fp = (active == m_fp_camera)
        var on_goban = (active == m_goban_camera)

        if (on_fp) {
            if (cursor_visible()) {
                hide_cursor()
            }
        } elif (on_goban) {
            if (!cursor_visible()) {
                show_cursor()
            }
        }
    }

    def refresh_cameras() : void {
        if (m_fp_camera == 0 || !entity_valid(m_fp_camera)) {
            m_fp_camera = find_entity_by_name("FP Camera")
        }
        if (m_goban_camera == 0 || !entity_valid(m_goban_camera)) {
            m_goban_camera = find_entity_by_name("Goban Camera")
        }
    }

    def switch_to_camera(camera : uint) : void {
        set_active_camera(camera)

        var fp_active = (camera == m_fp_camera)
        set_relative_mouse_mode(fp_active)
        if (fp_active) {
            hide_cursor()
        } else {
            show_cursor()
        }
    }

    def get_grid_at(mx : int, my : int) : tuple<int,int,bool> {
        refresh_cameras()

        if (m_goban_camera == 0) return (0, 0, false)
        if (!has_component(TYPE_CAMERA(), m_goban_camera)) return (0, 0, false)
        if (!has_component(TYPE_WORLD_TRANSFORM(), m_goban_camera)) return (0, 0, false)

        refresh_window_size()
        var w = get_window_width()
        var h = get_window_height()
        if (w == 0 || h == 0) return (0, 0, false)

        refresh_editor_viewport()
        var img_min_x = get_editor_img_min_x()
        var img_min_y = get_editor_img_min_y()
        var img_size_x = get_editor_img_size_x()
        var img_size_y = get_editor_img_size_y()

        var mouse_x = float(mx)
        var mouse_y = float(my)

        if (img_size_x > 0.0 && img_size_y > 0.0) {
            if (mouse_x < img_min_x || mouse_x > img_min_x + img_size_x) return (0, 0, false)
            if (mouse_y < img_min_y || mouse_y > img_min_y + img_size_y) return (0, 0, false)

            var nx = (mouse_x - img_min_x) / img_size_x
            var ny = (mouse_y - img_min_y) / img_size_y
            mouse_x = nx * float(w)
            mouse_y = ny * float(h)
        }

        if (!make_pick_ray(m_goban_camera, mouse_x, mouse_y, 0.0, 0.0, float(w), float(h))) {
            return (0, 0, false)
        }

        var ox = get_ray_origin_x()
        var oy = get_ray_origin_y()
        var oz = get_ray_origin_z()
        var dx = get_ray_dir_x()
        var dy = get_ray_dir_y()
        var dz = get_ray_dir_z()

        if (!ray_plane_intersect(ox, oy, oz, dx, dy, dz, 0.0, 2.21, 0.0, 0.0, 1.0, 0.0)) {
            return (0, 0, false)
        }

        var hit_x = get_hit_x()
        var hit_y = get_hit_y()
        var hit_z = get_hit_z()

        var board_min = -2.21
        var board_max = 2.21
        if (hit_x < board_min || hit_x > board_max) return (0, 0, false)
        if (hit_z < board_min || hit_z > board_max) return (0, 0, false)

        var grid_size = 18
        var step = (board_max - board_min) / float(grid_size)

        var grid_x = int(round((hit_x - board_min) / step))
        var grid_z = int(round((hit_z - board_min) / step))

        grid_x = clamp(grid_x, 0, grid_size)
        grid_z = clamp(grid_z, 0, grid_size)

        return (grid_x, grid_z, true)
    }

    def place_stone_at(mx : int, my : int) : void {
        var (grid_x, grid_z, valid) = get_grid_at(mx, my)
        if (!valid) return

        var board_min = -2.21
        var board_max = 2.21
        var grid_size = 18
        var step = (board_max - board_min) / float(grid_size)
        var world_x = board_min + float(grid_x) * step
        var world_z = board_min + float(grid_z) * step

        var prefab_path = "res://rsc/scenes/Black_Stone.prefab"
        var color = "Black"
        if (!m_is_black_turn) {
            prefab_path = "res://rsc/scenes/White_Stone.prefab"
            color = "White"
        }

        var stone_name = "{color} Stone ({grid_x},{grid_z})"

        var existing_black = find_entity_by_name("Black Stone ({grid_x},{grid_z})")
        if (existing_black != 0 && !entity_is_null(existing_black)) {
            entity_destroy(existing_black)
        }

        var existing_white = find_entity_by_name("White Stone ({grid_x},{grid_z})")
        if (existing_white != 0 && !entity_is_null(existing_white)) {
            entity_destroy(existing_white)
        }

        var new_stone = instantiate_prefab(prefab_path)
        if (new_stone == 0 || entity_is_null(new_stone)) return

        set_entity_name(new_stone, stone_name)

        if (has_component(TYPE_TRANSFORM(), new_stone)) {
            set_position(new_stone, world_x, 2.21, world_z)
            set_scale(new_stone, 2.0, 2.0, 2.0)
        }

        m_is_black_turn = !m_is_black_turn
    }

    def remove_stone_at(mx : int, my : int) : void {
        var (grid_x, grid_z, valid) = get_grid_at(mx, my)
        if (!valid) return

        var existing_black = find_entity_by_name("Black Stone ({grid_x},{grid_z})")
        if (existing_black != 0 && !entity_is_null(existing_black)) {
            entity_destroy(existing_black)
            return
        }

        var existing_white = find_entity_by_name("White Stone ({grid_x},{grid_z})")
        if (existing_white != 0 && !entity_is_null(existing_white)) {
            entity_destroy(existing_white)
            return
        }
    }
}
