#pragma once

/*! \file mikktspace.hpp
 *  \brief C++ wrapper and staged port of MikkTSpace tangent space generator.
 *
 *  This file provides a C++-style interface while keeping the original
 *  C implementation available for a phased port. The goal is to fully
 *  port the logic into idiomatic C++ (std::vector, RAII, namespaces,
 *  and math wrappers) in follow-up commits.
 */

#include <cstddef>
#include <cstdint>

// Keep the original C header available for compatibility and a safe fallback.
#include "mikktspace_header"

namespace wsl {
namespace math {

/*! \brief Thin C++ wrapper around the legacy MikkTSpace API.
 *
 *  This wrapper preserves the original callback-based API but provides
 *  a more C++-friendly entry point. The full implementation will be
 *  ported into C++ incrementally; for now the wrapper delegates to the
 *  original functions to keep behaviour stable.
 */
class mikktspace_generator {
public:
  explicit mikktspace_generator () noexcept = default;

  /*! \brief Generate tangent space using the default angular threshold.
   *  \param ctx Pointer to an SMikkTSpaceContext (as defined by the legacy API).
   *  \return true on success, false on failure.
   */
  bool gen_tang_space_default (const SMikkTSpaceContext *ctx) const noexcept;

  /*! \brief Generate tangent space with a custom angular threshold in degrees.
   *  \param ctx Pointer to an SMikkTSpaceContext (as defined by the legacy API).
   *  \param angular_threshold Threshold in degrees used when splitting groups.
   *  \return true on success, false on failure.
   */
  bool gen_tang_space (const SMikkTSpaceContext *ctx, float angular_threshold) const noexcept;
};

} // namespace math
} // namespace wsl
