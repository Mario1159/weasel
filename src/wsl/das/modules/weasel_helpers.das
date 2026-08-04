options gen2
module weasel_helpers

// Requires weasel_api to be loaded first (for _get_component_* bindings)
require weasel_api

// --- Generic component access helpers ---

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
