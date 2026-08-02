require weasel_api
require math

class EcsSystem {
    def on_init() : void { }
    def on_update(dt : float) : void { }
    def on_event() : void { }
    def on_inactive() : void { }
}

class System : EcsSystem {
    m_accumulated_dx : float = 0.0
    m_accumulated_dy : float = 0.0
    m_relative_mode_active : bool = false
    m_mouse_rot_type : uint = 0

    // MouseRotate field byte offsets (float fields)
    OFF_SENSITIVITY : int = 0
    OFF_YAW : int = 4
    OFF_PITCH : int = 8
    OFF_PITCH_MIN : int = 12
    OFF_PITCH_MAX : int = 16

    def override on_init() : void {
        m_mouse_rot_type = get_component_type_id("Mouse Rotate")
        if (m_mouse_rot_type == 0) return

        refresh_entities_with_transform()
        var count = get_entity_count()
        for (i in range(count)) {
            var e = get_entity_at(uint(i))
            if (!has_component(m_mouse_rot_type, e)) continue
            if (!has_component(TYPE_TRANSFORM(), e)) continue

            var yaw = get_rotation_yaw(e)
            var pitch = get_rotation_pitch(e)

            set_component_field_f(e, m_mouse_rot_type, OFF_YAW, yaw)
            set_component_field_f(e, m_mouse_rot_type, OFF_PITCH, pitch)
        }
    }

    def override on_inactive() : void {
        set_relative_mouse_mode(false)
        m_relative_mode_active = false
    }

    def override on_event() : void {
        var kind = get_event_kind()

        if (kind == EVENT_MOUSE_MOTION) {
            var active_cam = get_active_camera()
            if (active_cam == 0) return

            var fp_active = false
            refresh_entities_with_transform()
            var count = get_entity_count()
            for (i in range(count)) {
                var e = get_entity_at(uint(i))
                if (!has_component(m_mouse_rot_type, e)) continue
                if (!has_component(TYPE_CAMERA(), e)) continue
                if (e == active_cam) {
                    fp_active = true
                    break
                }
            }

            if (!fp_active) return

            m_accumulated_dx += get_event_mouse_dx()
            m_accumulated_dy += get_event_mouse_dy()
        }
    }

    def override on_update(dt : float) : void {
        if (m_mouse_rot_type == 0) return

        var active_cam = get_active_camera()
        var fp_active = false

        if (active_cam != 0) {
            refresh_entities_with_transform()
            var count = get_entity_count()
            for (i in range(count)) {
                var e = get_entity_at(uint(i))
                if (!has_component(m_mouse_rot_type, e)) continue
                if (!has_component(TYPE_CAMERA(), e)) continue
                if (e == active_cam) {
                    fp_active = true
                    break
                }
            }
        }

        var should_be_relative = fp_active
        if (m_relative_mode_active != should_be_relative) {
            m_relative_mode_active = should_be_relative
            set_relative_mouse_mode(should_be_relative)
        }

        if (!fp_active) {
            m_accumulated_dx = 0.0
            m_accumulated_dy = 0.0
            return
        }

        if (m_accumulated_dx == 0.0 && m_accumulated_dy == 0.0) return

        refresh_entities_with_transform()
        var count = get_entity_count()
        for (i in range(count)) {
            var e = get_entity_at(uint(i))
            if (!has_component(m_mouse_rot_type, e)) continue
            if (!has_component(TYPE_TRANSFORM(), e)) continue

            var sensitivity = get_component_field_f(e, m_mouse_rot_type, OFF_SENSITIVITY)
            var yaw = get_component_field_f(e, m_mouse_rot_type, OFF_YAW)
            var pitch = get_component_field_f(e, m_mouse_rot_type, OFF_PITCH)
            var pitch_min = get_component_field_f(e, m_mouse_rot_type, OFF_PITCH_MIN)
            var pitch_max = get_component_field_f(e, m_mouse_rot_type, OFF_PITCH_MAX)

            yaw -= m_accumulated_dx * sensitivity
            pitch -= m_accumulated_dy * sensitivity
            pitch = clamp(pitch, pitch_min, pitch_max)

            set_component_field_f(e, m_mouse_rot_type, OFF_YAW, yaw)
            set_component_field_f(e, m_mouse_rot_type, OFF_PITCH, pitch)

            set_rotation(e, pitch, yaw, 0.0)
        }

        m_accumulated_dx = 0.0
        m_accumulated_dy = 0.0
    }
}
