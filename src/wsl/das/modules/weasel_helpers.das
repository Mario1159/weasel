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

def _component_type_id_of(tag : Transform) : uint {
    return TYPE_TRANSFORM()
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
