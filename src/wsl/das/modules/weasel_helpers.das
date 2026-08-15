options gen2
module weasel_helpers

// Requires weasel_api to be loaded first (for _get_component_* bindings)
require weasel_api

// AST manipulation + quasi-quote support for the `query_entities` call macro.
require daslib/ast
require daslib/ast_boost
require daslib/templates_boost
require daslib/macro_boost
require daslib/rtti
require daslib/templates_boost

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

// --- Component access proxies (get_component(entity, T())) ---
//
// The tag functions below default-construct a proxy value whose type fixes
// the overload of get_component. The native accessor externs (registered by
// Module_WeaselApi in interpreted mode) capture the component pointer once and
// expose it through properties.

/// Component tag for the 3D Transform proxy.
def transform() : TransformAccessor {
    return default<TransformAccessor>
}

/// Component tag for the 2D Transform proxy.
def transform_2d() : Transform2DAccessor {
    return default<Transform2DAccessor>
}

/// Component tag for the 2D Camera proxy.
def camera_2d() : Camera2DAccessor {
    return default<Camera2DAccessor>
}

/// Component tag for the 2D Sprite proxy.
def sprite_2d() : Sprite2DAccessor {
    return default<Sprite2DAccessor>
}

/// Component tag for the Point Light proxy.
def point_light() : PointLightAccessor {
    return default<PointLightAccessor>
}

/// Component tag for the Directional Light proxy.
def directional_light() : DirectionalLightAccessor {
    return default<DirectionalLightAccessor>
}

/// Component tag for the Spot Light proxy.
def spot_light() : SpotLightAccessor {
    return default<SpotLightAccessor>
}

/// Returns a live Transform proxy for the entity (null component if absent).
def get_component(entity : uint, tag : TransformAccessor) : TransformAccessor {
    return get_transform_accessor(entity)
}

/// Returns a live Transform2D proxy for the entity (null component if absent).
def get_component(entity : uint, tag : Transform2DAccessor) : Transform2DAccessor {
    return get_transform_2d_accessor(entity)
}

/// Returns a live Camera2D proxy for the entity (null component if absent).
def get_component(entity : uint, tag : Camera2DAccessor) : Camera2DAccessor {
    return get_camera_2d_accessor(entity)
}

/// Returns a live Sprite2D proxy for the entity (null component if absent).
def get_component(entity : uint, tag : Sprite2DAccessor) : Sprite2DAccessor {
    return get_sprite_2d_accessor(entity)
}

/// Returns a live PointLight proxy for the entity (null component if absent).
def get_component(entity : uint, tag : PointLightAccessor) : PointLightAccessor {
    return get_point_light_accessor(entity)
}

/// Returns a live DirectionalLight proxy for the entity (null component if absent).
def get_component(entity : uint, tag : DirectionalLightAccessor) : DirectionalLightAccessor {
    return get_directional_light_accessor(entity)
}

/// Returns a live SpotLight proxy for the entity (null component if absent).
def get_component(entity : uint, tag : SpotLightAccessor) : SpotLightAccessor {
    return get_spot_light_accessor(entity)
}

// --- DECS-style iteration: `query_entities()` call macro ---

/// `query_entities( $(var t : TransformAccessor; var c : Camera2DAccessor) { ... } )`
/// iterates every entity that owns ALL listed components and binds each, in
/// place, to a live proxy inside the block. It desugars onto the generic C++
/// primitive `each_entity_id_with` (see `wsl_api_module.cpp`).
///
/// The block parameters name the proxy types (`TransformAccessor`,
/// `Camera2DAccessor`, …); each is rewritten to
/// `var <name> = get_component(e, default<ProxyType>())` so mutation goes
/// straight into entt storage (no copy-out / write-back) for proxies, or a
/// copy bound via `get_component_t` (written back with `set_component_t`) for
/// plain struct components. The component type ids are resolved via
/// `_component_type_id_of` (proxy `TYPE_*` constants or name lookup).
[call_macro(name="query_entities")]
class private QueryEntitiesMacro : AstCallMacro {
    def override visit(prog : ProgramPtr; mod : Module?; var call : ExprCallMacro?) : ExpressionPtr {
        let blockArg = call.arguments[0]
        var blk : ExprBlock?
        if (blockArg is ExprMakeBlock) {
            blk = (blockArg as ExprMakeBlock)._block as ExprBlock
        } else {
            blk = blockArg as ExprBlock
        }
        assert(blk != null, "query_entities expects a block argument")
        var declStmts : array<Expression?>
        var idExprs : array<Expression?>
        for (p in blk.arguments) {
            declStmts |> push <| qmacro_expr() {
                var $i(p.name) = _bind_component($i("e"), default< $t(clone_type(p._type)) >)
            }
            idExprs |> push <| qmacro_expr() {
                _component_type_id_of(default< $t(clone_type(p._type)) >)
            }
        }
        var bodyStmts : array<Expression?>
        for (s in blk.list) {
            bodyStmts |> push <| clone_expression(s)
        }
        var allStmts : array<Expression?> <- declStmts
        for (s in bodyStmts) {
            allStmts |> push <| s
        }
        return qmacro_block() {
            each_entity_id_with([ $a(idExprs) ]) <| $( $i("e") : uint) {
                $b(allStmts)
            }
        }
    }
}

// --- `query_entities` component dispatch helpers ---------------------------------
//
// `query_entities` accepts block parameters of any component type, both the
// engine's live proxies (`TransformAccessor`, …) and plain daslang struct
// components (`MouseRotate`, …). For each parameter the macro needs (a) the
// runtime component type id used to filter the iteration, and (b) a value bound
// to the current entity inside the block body.
//
// Proxy overloads are listed first so they win overload resolution over the
// generic `auto(T)` fallbacks. Proxy ids come from the `TYPE_*` type-hash
// constants (matching `reg->storage(id)`, no runtime_context needed); generic
// components resolve by name via `_get_component_type_id_by_name` — the SAME
// lookup used by `get_component_t`/`set_component_t`, so the iteration id
// matches the storage id that the read/write path uses.

// Runtime component type id for a component value.
def _component_type_id_of(tag : TransformAccessor) : uint { return TYPE_TRANSFORM() }
def _component_type_id_of(tag : Transform2DAccessor) : uint { return TYPE_TRANSFORM_2D() }
def _component_type_id_of(tag : Camera2DAccessor) : uint { return TYPE_CAMERA_2D() }
def _component_type_id_of(tag : Sprite2DAccessor) : uint { return TYPE_SPRITE_2D() }
def _component_type_id_of(tag : PointLightAccessor) : uint { return TYPE_POINT_LIGHT() }
def _component_type_id_of(tag : DirectionalLightAccessor) : uint { return TYPE_DIRECTIONAL_LIGHT() }
def _component_type_id_of(tag : SpotLightAccessor) : uint { return TYPE_SPOT_LIGHT() }
def _component_type_id_of(tag : auto(T)) : uint {
    return _get_component_type_id_by_name(typeinfo typename(type<T>))
}

// Bind a component value for `entity` inside the `query_entities` block. Proxies
// are bound through the live `get_component` (pointer to entt storage, so
// in-place mutation works); generic struct components are copied via
// `get_component_t` and must be written back with `set_component_t`.
def _bind_component(entity : uint; tag : TransformAccessor) : TransformAccessor {
    return get_component(entity, default<TransformAccessor>)
}
def _bind_component(entity : uint; tag : Transform2DAccessor) : Transform2DAccessor {
    return get_component(entity, default<Transform2DAccessor>)
}
def _bind_component(entity : uint; tag : Camera2DAccessor) : Camera2DAccessor {
    return get_component(entity, default<Camera2DAccessor>)
}
def _bind_component(entity : uint; tag : Sprite2DAccessor) : Sprite2DAccessor {
    return get_component(entity, default<Sprite2DAccessor>)
}
def _bind_component(entity : uint; tag : PointLightAccessor) : PointLightAccessor {
    return get_component(entity, default<PointLightAccessor>)
}
def _bind_component(entity : uint; tag : DirectionalLightAccessor) : DirectionalLightAccessor {
    return get_component(entity, default<DirectionalLightAccessor>)
}
def _bind_component(entity : uint; tag : SpotLightAccessor) : SpotLightAccessor {
    return get_component(entity, default<SpotLightAccessor>)
}
def _bind_component(entity : uint; tag : auto(T)) : T {
    return get_component_t(entity, default<T>)
}
