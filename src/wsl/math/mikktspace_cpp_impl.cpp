/*! \file mikktspace_cpp_impl.cpp
 *  \brief C-compatible entry points that delegate to the C++ mikktspace generator.
 */

#include "mikktspace_header"
#include "mikktspace.hpp"

extern "C" {

tbool gen_tang_space_default(const SMikkTSpaceContext * p_context)
{
  wsl::math::mikktspace_generator gen;
  return gen.gen_tang_space_default(p_context) ? 1 : 0;
}

tbool gen_tang_space(const SMikkTSpaceContext * p_context, float f_angular_threshold)
{
  wsl::math::mikktspace_generator gen;
  return gen.gen_tang_space(p_context, f_angular_threshold) ? 1 : 0;
}

} // extern "C"
