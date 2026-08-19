options gen2
options remove_unused_symbols = false
module weasel_helpers

require weasel_api
require daslib/ast
require daslib/ast_boost
require daslib/templates_boost
require daslib/macro_boost
require daslib/rtti

def get_component_or(entity : uint; tag : Transform) : Transform {
    return get_transform_accessor(entity)
}

def get_component_or(entity : uint; tag : Transform2D) : Transform2D {
    return get_transform_2d_accessor(entity)
}

def get_component_or(entity : uint; tag : Camera2D) : Camera2D {
    return get_camera_2d_accessor(entity)
}

def get_component_or(entity : uint; tag : Sprite2D) : Sprite2D {
    return get_sprite_2d_accessor(entity)
}

def get_component_or(entity : uint; tag : PointLight) : PointLight {
    return get_point_light_accessor(entity)
}

def get_component_or(entity : uint; tag : DirectionalLight) : DirectionalLight {
    return get_directional_light_accessor(entity)
}

def get_component_or(entity : uint; tag : SpotLight) : SpotLight {
    return get_spot_light_accessor(entity)
}

def get_component_or(entity : uint; tag : Hierarchy) : Hierarchy {
    return get_hierarchy_accessor(entity)
}

def get_component_or(entity : uint; tag : WorldTransform) : WorldTransform {
    return get_world_transform_accessor(entity)
}

def get_component_or(entity : uint; tag : RigidBody) : RigidBody {
    return get_rigid_body_accessor(entity)
}

def get_component_or(entity : uint; tag : CharacterBody) : CharacterBody {
    return get_character_body_accessor(entity)
}

def get_component_or(entity : uint; tag : ModelInstance3D) : ModelInstance3D {
    return get_model_instance_3d_accessor(entity)
}

def get_component_or(entity : uint; tag : Area3D) : Area3D {
    return get_area_3d_accessor(entity)
}

def get_component_or(entity : uint; tag : Audio) : Audio {
    return get_audio_accessor(entity)
}

def get_component_or(entity : uint; tag : PrefabInstance) : PrefabInstance {
    return get_prefab_instance_accessor(entity)
}

def get_component_or(entity : uint; tag : Subviewport) : Subviewport {
    return get_subviewport_accessor(entity)
}

// Generic Daslang-component accessor. For user-defined components the engine
// cannot express a single function that returns a live reference when present
// and a value when absent, so this returns a value copy: a copy of the pool
// bytes when the component exists, or `default<T>` when it does not. Use
// `query()` (which binds a live `T&`) for in-place mutation of Daslang
// components.
def get_component_or(entity : uint; tag : auto(T)) : T {
    let type_id = _component_type_id_of(tag)
    var data = _get_component_data(entity, type_id)
    if (data == null) {
        return default<T>
    }
    unsafe {
        return *(reinterpret<T -#?>(data))
    }
}

// Typed component predicates/operations. These resolve the stable component id
// from a type witness and forward to the id-based engine primitives.
def has_component(entity : uint; tag : auto(T)) : bool {
    return has_component(_component_type_id_of(tag), entity)
}

def add_component(entity : uint; tag : auto(T)) : bool {
    return add_component(_component_type_id_of(tag), entity)
}

def remove_component(entity : uint; tag : auto(T)) : bool {
    return remove_component(_component_type_id_of(tag), entity)
}

def _component_type_id_of(tag : Transform) : uint {
    return TYPE_TRANSFORM()
}

def _component_type_id_of(tag : Transform2D) : uint {
    return TYPE_TRANSFORM_2D()
}

def _component_type_id_of(tag : Camera2D) : uint {
    return TYPE_CAMERA_2D()
}

def _component_type_id_of(tag : Sprite2D) : uint {
    return TYPE_SPRITE_2D()
}

def _component_type_id_of(tag : PointLight) : uint {
    return TYPE_POINT_LIGHT()
}

def _component_type_id_of(tag : DirectionalLight) : uint {
    return TYPE_DIRECTIONAL_LIGHT()
}

def _component_type_id_of(tag : SpotLight) : uint {
    return TYPE_SPOT_LIGHT()
}

def _component_type_id_of(tag : Hierarchy) : uint {
    return TYPE_HIERARCHY()
}

def _component_type_id_of(tag : WorldTransform) : uint {
    return TYPE_WORLD_TRANSFORM()
}

def _component_type_id_of(tag : RigidBody) : uint {
    return TYPE_RIGID_BODY()
}

def _component_type_id_of(tag : CharacterBody) : uint {
    return TYPE_CHARACTER_BODY()
}

def _component_type_id_of(tag : ModelInstance3D) : uint {
    return TYPE_MODEL_INSTANCE_3D()
}

def _component_type_id_of(tag : Area3D) : uint {
    return TYPE_AREA_3D()
}

def _component_type_id_of(tag : Audio) : uint {
    return TYPE_AUDIO()
}

def _component_type_id_of(tag : PrefabInstance) : uint {
    return TYPE_PREFAB_INSTANCE()
}

def _component_type_id_of(tag : Subviewport) : uint {
    return TYPE_SUBVIEWPORT()
}

def _component_type_id_of(tag : auto(T)) : uint {
    return _get_component_type_id_by_name(typeinfo typename(type<T>))
}

def _get_component_ref(entity : uint; type_id : uint;
                       tag : auto(T)) : T -const& {
    var data = _get_component_data(entity, type_id)
    if (data == null) {
        panic("query component payload is missing")
    }
    unsafe {
        return <- *(reinterpret<T -const -& -#?>(data))
    }
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Transform) : Transform {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Transform2D) : Transform2D {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Camera2D) : Camera2D {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Sprite2D) : Sprite2D {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : PointLight) : PointLight {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : DirectionalLight) : DirectionalLight {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : SpotLight) : SpotLight {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Hierarchy) : Hierarchy {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : WorldTransform) : WorldTransform {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : RigidBody) : RigidBody {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : CharacterBody) : CharacterBody {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : ModelInstance3D) : ModelInstance3D {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Area3D) : Area3D {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Audio) : Audio {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : PrefabInstance) : PrefabInstance {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : Subviewport) : Subviewport {
    return get_component_or(entity, tag)
}

def _bind_query_component(entity : uint; type_id : uint;
                          tag : auto(T)) : T -const& {
    unsafe {
        return <- _get_component_ref(entity, type_id, tag)
    }
}

[call_macro(name="query")]
class private QueryMacro : AstCallMacro {
    def override visit(prog : ProgramPtr; mod : Module?;
                       var call : ExprCallMacro?) : ExpressionPtr {
        let blockArg = call.arguments[0]
        var blk : ExprBlock?
        if (blockArg is ExprMakeBlock) {
            blk = (blockArg as ExprMakeBlock)._block as ExprBlock
        } else {
            blk = blockArg as ExprBlock
        }
        assert(blk != null, "query expects a block argument")

        var outerStmts : array<Expression?>
        var idExprs : array<Expression?>
        var declStmts : array<Expression?>

        for (p in blk.arguments) {
            if (p._type == null || !p._type.isRefType) {
                macro_error(prog, p.at,
                            "query component parameters must be references")
                return null
            }

            var baseType = clone_type(p._type)
            baseType.flags.ref = false
            baseType.flags.constant = true
            var mutableType = clone_type(baseType)
            mutableType.flags.constant = false
            if (!p._type.isHandle) {
                mutableType.flags.ref = true
            }
            let idName = "__query_id_" + string(p.name)

            outerStmts |> push <| qmacro_expr() {
                var $i(idName) : uint = _component_type_id_of(
                    default< $t(baseType) >)
            }
            idExprs |> push <| qmacro_expr() { $i(idName) }
            declStmts |> push <| qmacro_expr() {
                var $i(p.name) : $t(mutableType) = _bind_query_component(
                    $i("e"), $i(idName), default< $t(baseType) >)
            }
        }

        var callbackStmts : array<Expression?> <- declStmts
        for (s in blk.list) {
            callbackStmts |> push <| clone_expression(s)
        }

        outerStmts |> push <| qmacro_expr() {
            each_entity_id_with([ $a(idExprs) ]) <| $( $i("e") : uint) {
                $b(callbackStmts)
            }
        }
        return qmacro_block() { $b(outerStmts) }
    }
}
