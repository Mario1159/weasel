require weasel_api
require math
require mouse_rotate

class EcsSystem {
    def on_init() : void { }
    def on_update(dt : float) : void { }
    def on_event() : void { }
    def on_inactive() : void { }
}

// --- Generic component access helpers (defined here for AOT compatibility) ---

/// Check if an entity has a component of the given type.
def has_component_t(entity : uint; dummy : auto(component_type)) : bool {
    var type_name = typeinfo typename(type<component_type>)
    var type_id = _get_component_type_id_by_name(type_name)
    if (type_id == 0) return false
    return has_component(type_id, entity)
}

/// Returns a typed copy of the component of type T on the entity.
def get_component_t(entity : uint; dummy : auto(component_type)) : component_type {
    var type_name = typeinfo typename(type<component_type>)
    var type_id = _get_component_type_id_by_name(type_name)
    var kind = _get_component_kind_by_name(type_name)
    if (type_id == 0 || !has_component(type_id, entity)) {
        return default<component_type>
    }
    var result : component_type = default<component_type>
    _get_component_into(entity, type_id, kind, unsafe(reinterpret<void?>(unsafe(addr(result)))))
    return result
}

/// Writes the component struct back to the entity's component storage.
def set_component_t(entity : uint; comp : auto(component_type)) {
    var type_name = typeinfo typename(type<component_type>)
    var type_id = _get_component_type_id_by_name(type_name)
    var kind = _get_component_kind_by_name(type_name)
    if (type_id == 0 || !has_component(type_id, entity)) {
        return
    }
    _set_component_from(entity, type_id, kind, unsafe(reinterpret<void?>(unsafe(addr(comp)))))
}

class System : EcsSystem {
    m_accumulated_dx : float = 0.0
    m_accumulated_dy : float = 0.0
    m_relative_mode_active : bool = false

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
