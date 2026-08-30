// wsl.log — C++20 module interface veneer over src/wsl/log/log.hpp.
//
// See CXX_MODULES_PLAN.md (Phase 1). This is the first engine namespace to be
// modularized because it has no cross-namespace dependencies (only 3rd-party
// spdlog). 3rd-party headers are included in the *global module fragment*
// (before `export module`) so their macros/types are NOT injected into the
// module purview. spdlog/spdlog.h carries include guards, so log.hpp's own
// `#include <spdlog/spdlog.h>` is skipped here and spdlog stays in the global
// fragment — keeping the exported `wsl::log` signatures reachable without
// leaking spdlog macros to importers. (Validated: clang compiles and links an
// importer that uses these spdlog-typed return values.)
module;
#include <spdlog/spdlog.h>
#include <memory>
export module wsl.log;

export {
#include "log.hpp"
}
