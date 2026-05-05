/*! \file mikktspace.cpp
 *  \brief Staged C++ port: wrapper that begins using C++ internals.
 *
 *  The implementation currently delegates to the proven C implementation
 *  for full correctness. The incremental port provides typed helpers and
 *  a first step toward a full C++ algorithm by exercising the new helpers
 *  where practical.
 */

#include "mikktspace.hpp"
#include "mikktspace_header" // original C header (renamed)
#include "mikktspace_internal.hpp"

#include <vector>

namespace wsl {
namespace math {

bool mikktspace_generator::gen_tang_space_default (const SMikkTSpaceContext *ctx) const noexcept
{
  // Keep behaviour identical to the legacy API for now.
  return ::gen_tang_space_default(ctx) == 1 ? true : false;
}

bool mikktspace_generator::gen_tang_space (const SMikkTSpaceContext *ctx, float angular_threshold) const noexcept
{
  // Example usage of the new C++ helper: build the initial triangle list
  // and tspace placeholder data. This is preparatory and does not yet
  // replace the proven C implementation.
  std::vector<int> tri_list;
  std::vector<mikktspace::tspace> tspaces;
  const int total_tspaces = mikktspace::generate_initial_vertices_index_list(tri_list, tspaces, ctx);
  (void)total_tspaces; // unused for now

  // Delegate to the stable C implementation for the actual generation.
  return ::gen_tang_space(ctx, angular_threshold) == 1 ? true : false;
}

} // namespace math
} // namespace wsl
