#include "serialize.hpp"
#include "types.hpp"
#include "adapters.hpp"

// Phase B — serialize.cpp is intentionally minimal; all logic is header-only
// via rfl. This TU exists so xmake can add_files("src/wsl/serialize/*.cpp")
// and to provide a place for explicit template instantiations if needed.

namespace wsl::serialize
{
// Explicit instantiations for common types can be added here to reduce
// compile times, e.g.:
// template std::string json_write<wsl::rsc::scene_snapshot>(const
// wsl::rsc::scene_snapshot&, std::string*); etc.
}
