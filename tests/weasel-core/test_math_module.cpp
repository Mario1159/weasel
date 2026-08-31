// Consumer validation for the wsl.math named module (Phase 2 of
// CXX_MODULES_PLAN_V2.md). Compiled with module flags by CMake when
// WEASEL_ENABLE_MODULES is ON; otherwise it falls back to the legacy
// header include so the file stays in the suite either way.

// GCC 16's std declarations conflict with doctest's forward-declaration
// block when real std headers are included first (via wsl.math's deps).
#define DOCTEST_CONFIG_USE_STD_HEADERS

// In module mode, every include must precede the import declaration.
#include <doctest/doctest.h>
#include <glm/glm.hpp>

#if defined(WEASEL_MATH_MODULE_CONSUMER)

import wsl.math;

#else

#include "wsl/math/vector.hpp"
#include "wsl/math/matrix.hpp"

#endif

TEST_CASE ("wsl.math module consumer round-trip")
{
  wsl::math::vec3f v{ 1.0F, 2.0F, 3.0F };
  CHECK (v.x () == 1.0F);
  CHECK (v.y () == 2.0F);
  CHECK (v.z () == 3.0F);

  // glm interop survives the module boundary.
  glm::vec3 g = v;
  CHECK (g.x == 1.0F);

  wsl::math::quatf q{ 0.0F, 0.0F, 0.0F, 1.0F };
  CHECK (q.w () == 1.0F);

  wsl::math::mat44f m;
  m.data ()[0] = 2.0F;
  CHECK (m.data ()[0] == 2.0F);
}
