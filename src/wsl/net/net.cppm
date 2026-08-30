// wsl.net — C++20 module interface veneer over src/wsl/net/command_protocol.hpp.
//
// See CXX_MODULES_PLAN.md (Phase 1). Leaf namespace: depends only on the
// standard library. <string>/<vector> are included in the global module
// fragment; they carry include guards, so command_protocol.hpp's own includes
// are skipped and stay out of the module purview.
module;
#include <string>
#include <vector>
export module wsl.net;

export {
#include "command_protocol.hpp"
}
