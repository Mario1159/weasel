module;
// Global module fragment: everything the exported event headers skip under
// WSL_MODULE_BUILD, provided textually. comp/component_meta.hpp supplies
// stable_type_id + the component tag concepts used by the hub's template
// API; being in the GMF, its entities stay attached to the global module
// (shared safely with every other module and with legacy TUs).
#include <entt/entt.hpp>
#include "../comp/component_meta.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define WSL_MODULE_BUILD

export module wsl.event;

export {
#include "event_hub_fwd.hpp"
#include "event_hub.hpp"
#include "message_event.hpp"
#include "message_bus.hpp"
}
