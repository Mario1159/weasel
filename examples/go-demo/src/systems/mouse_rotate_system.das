require weasel_api
require weasel_ecs
require weasel_helpers
require math
require mouse_rotate

class System : EcsSystem {
    m_accumulated_dx : float = 0.0
    m_accumulated_dy : float = 0.0

    def override on_init() : void {
        refresh_entities_with_transform()
        var count = get_entity_count()
        for (i in range(count)) {
            var e = get_entity_at(uint(i))
            if (!has_component_t(e, MouseRotate())) continue
            if (!has_component(TYPE_TRANSFORM(), e)) continue

            var yaw = get_rotation_yaw(e)
            var pitch = get_rotation_pitch(e)

            var mr = get_component_t(e, MouseRotate())
            mr.yaw = yaw
            mr.pitch = pitch
            set_component_t(e, mr)
        }
    }

    def override on_inactive() : void {
        set_relative_mouse_mode(false)
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
                if (!has_component_t(e, MouseRotate())) continue
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
        var active_cam = get_active_camera()
        var fp_active = false

        if (active_cam != 0) {
            refresh_entities_with_transform()
            var count = get_entity_count()
            for (i in range(count)) {
                var e = get_entity_at(uint(i))
                if (!has_component_t(e, MouseRotate())) continue
                if (!has_component(TYPE_CAMERA(), e)) continue
                if (e == active_cam) {
                    fp_active = true
                    break
                }
            }
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
            if (!has_component_t(e, MouseRotate())) continue
            if (!has_component(TYPE_TRANSFORM(), e)) continue

            var mr = get_component_t(e, MouseRotate())
            mr.yaw -= m_accumulated_dx * mr.sensitivity
            mr.pitch -= m_accumulated_dy * mr.sensitivity
            mr.pitch = clamp(mr.pitch, mr.pitch_min, mr.pitch_max)
            set_component_t(e, mr)

            set_rotation(e, mr.pitch, mr.yaw, 0.0)
        }

        m_accumulated_dx = 0.0
        m_accumulated_dy = 0.0
    }
}
