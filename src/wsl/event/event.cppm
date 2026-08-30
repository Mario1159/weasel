// wsl.event — C++20 module interface veneer over src/wsl/event/*.hpp.
//
// Phase 1 veneer (see CXX_MODULES_PLAN.md). wsl.event depends on comp
// (component_meta) and 3rd-party entt/cereal/std, and its *exported* API names
// EnTT types (entt::id_type, entt::registry&, function-pointer typedefs) plus a
// cereal serialize template. Unlike wsl.math, event->comp is a ONE-WAY dependency
// (comp does not include event), so there is no cycle to break: comp is consumed
// TEXTUALLY in this module's global fragment, exactly like the other 3rd-party
// headers. The header-unit path is intentionally NOT used here — cereal cannot be
// built as a header unit in GCC 16, and importing comp as a header unit would
// clash with the cereal/entt wsl.event needs in its own purview.
//
// Crucially, WSL_MODULE_BUILD is deliberately NOT defined here: component_meta.hpp
// drops its entt/cereal includes when that macro is set (it expects them from the
// imported header unit), so a textual include must leave it unset so the engine
// header pulls entt/cereal in normally. All external includes live in the *global
// module fragment* so the engine header graph does not become illegal module
// `import` cycles; the event headers' own comp/entt includes are guarded no-ops
// once present.
module;

#include <algorithm>
#include <cereal/cereal.hpp>
#include "../comp/component_meta.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <entt/entt.hpp>
#include <functional>
#include <mutex>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module wsl.event;

export {
#include "event_hub_fwd.hpp"
#include "event_hub.hpp"
#include "message_bus.hpp"
#include "message_event.hpp"
}
