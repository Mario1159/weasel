// wsl.debug — C++20 module interface veneer over src/wsl/debug/debug_renderer.hpp.
//
// See CXX_MODULES_PLAN.md (Phase 1). Leaf namespace: depends only on 3rd-party
// glm. glm is included in the global module fragment so its (macro-heavy)
// declarations are not injected into the module purview; glm/glm.hpp carries
// include guards, so debug_renderer.hpp's own `#include <glm/glm.hpp>` is
// skipped here.
module;
#include <glm/glm.hpp>
export module wsl.debug;

export {
#include "debug_renderer.hpp"
}
