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
namespace mikktspace {
bool genTangSpaceDefault(const SMikkTSpaceContext * p_context);
bool genTangSpace(const SMikkTSpaceContext * p_context, float f_angular_threshold);
} // namespace mikktspace

bool mikktspace_generator::gen_tang_space_default (const SMikkTSpaceContext *ctx) const noexcept
{
  // Keep behaviour identical to the legacy API for now.
  return mikktspace::genTangSpaceDefault(ctx) == true;
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
  return mikktspace::genTangSpace(ctx, angular_threshold) == true;
}

} // namespace math
} // namespace wsl
