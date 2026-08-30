module;

#include <daScript/ast/ast.h>
#include <daScript/ast/ast_handle.h>
#include <daScript/misc/vectypes.h>
#include <daScript/simulate/runtime_matrices.h>
#include "../comp/component_meta.hpp"
#include <entt/entt.hpp>
#include <entt/core/type_info.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

export module wsl.das;

export {
#include "das_ecs_binds.hpp"
#include "das_engine.hpp"
#include "das_interop.hpp"
#include "das_system_adapter.hpp"
#include "wsl_api_component_accessors.hpp"
#include "wsl_api_module.hpp"
#include "wsl_event_binds.hpp"
#include "das_api_catalog.gen.hpp"
}
