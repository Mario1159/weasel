require weasel_api
require weasel_ecs
require weasel_helpers
require math
require mouse_rotate

def quaternion_from_euler_degrees(pitch : float; yaw : float; roll : float) : float4 {
    let half_pitch = radians(pitch) * 0.5
    let half_yaw = radians(yaw) * 0.5
    let half_roll = radians(roll) * 0.5
    let cp = cos(half_pitch)
    let sp = sin(half_pitch)
    let cy = cos(half_yaw)
    let sy = sin(half_yaw)
    let cr = cos(half_roll)
    let sr = sin(half_roll)
    return float4(
        sp * cy * cr + cp * sy * sr,
        cp * sy * cr - sp * cy * sr,
        cp * cy * sr - sp * sy * cr,
        cp * cy * cr + sp * sy * sr)
}

class System : EcsSystem {
    m_accumulated_dx : float = 0.0
    m_accumulated_dy : float = 0.0

    def override on_init() : void {
        query() $(t : Transform&; mr : MouseRotate&) {
            mr.yaw = 0.0
            mr.pitch = 0.0
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
            query() $(mr : MouseRotate&) {
                if (has_component(TYPE_CAMERA(), e) && e == active_cam) {
                    fp_active = true
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
            query() $(mr : MouseRotate&) {
                if (has_component(TYPE_CAMERA(), e) && e == active_cam) {
                    fp_active = true
                }
            }
        }

        if (!fp_active) {
            m_accumulated_dx = 0.0
            m_accumulated_dy = 0.0
            return
        }

        if (m_accumulated_dx == 0.0 && m_accumulated_dy == 0.0) return

        query() $(t : Transform&; mr : MouseRotate&) {
            mr.yaw -= m_accumulated_dx * mr.sensitivity
            mr.pitch -= m_accumulated_dy * mr.sensitivity
            mr.pitch = clamp(mr.pitch, mr.pitch_min, mr.pitch_max)
            t.rotation = quaternion_from_euler_degrees(mr.pitch, mr.yaw, 0.0)
        }

        m_accumulated_dx = 0.0
        m_accumulated_dy = 0.0
    }
}
