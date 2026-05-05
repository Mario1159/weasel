#pragma once

/*! \file mikktspace_internal.hpp
 *  \brief Internal math helpers and data types for the C++ mikktspace port.
 *
 *  This header provides small, self-contained helpers that wrap the
 *  project's vec3f type and prepare the ground for porting the full
 *  MikkTSpace algorithm into idiomatic C++.
 */

#include "vector.hpp" // wsl::math::vec3f

#include <vector>
#include <cassert>
#include <cstddef>

// Include legacy C header for SMikkTSpaceContext and legacy APIs
#include "mikktspace.h"

namespace wsl {
namespace math {
namespace mikktspace {

using vec3f = ::wsl::math::vec3f;

/*! \brief 3-component helper functions used by the port. */
inline bool veq (const vec3f &a, const vec3f &b) noexcept
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline vec3f vadd (const vec3f &a, const vec3f &b) noexcept
{
  return vec3f{ a.x + b.x, a.y + b.y, a.z + b.z };
}

inline vec3f vsub (const vec3f &a, const vec3f &b) noexcept
{
  return vec3f{ a.x - b.x, a.y - b.y, a.z - b.z };
}

inline vec3f vscale (float s, const vec3f &v) noexcept
{
  return vec3f{ s * v.x, s * v.y, s * v.z };
}

inline float vdot (const vec3f &a, const vec3f &b) noexcept
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length_sq (const vec3f &v) noexcept
{
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length (const vec3f &v) noexcept
{
  return std::sqrt(length_sq(v));
}

inline vec3f normalize (const vec3f &v) noexcept
{
  const float L = length(v);
  if (L == 0.0F) return v;
  return vscale(1.0F / L, v);
}

inline bool vnot_zero (const vec3f &v) noexcept
{
  return (v.x != 0.0F) || (v.y != 0.0F) || (v.z != 0.0F);
}

struct tspace {
  vec3f v_os{ 1.0F, 0.0F, 0.0F };
  float f_mag_s{ 1.0F };
  vec3f v_ot{ 0.0F, 1.0F, 0.0F };
  float f_mag_t{ 1.0F };
  int counter{ 0 };
  bool orient{ false };
};

inline int make_index (int face, int vert)
{
  assert(vert >= 0 && vert < 4 && face >= 0);
  return (face << 2) | (vert & 0x3);
}

inline void index_to_data (int &face_out, int &vert_out, int index)
{
  vert_out = index & 0x3;
  face_out = index >> 2;
}

// Forward declarations for the algorithmic port (implemented in later phases).
int generate_initial_vertices_index_list (std::vector<int> &tri_list_out,
                                          std::vector<tspace> &out_tspaces,
                                          const SMikkTSpaceContext *ctx);

// helpers to query attributes from the legacy callback context
vec3f get_position(const SMikkTSpaceContext *ctx, int index);
vec3f get_normal(const SMikkTSpaceContext *ctx, int index);
vec3f get_texcoord(const SMikkTSpaceContext *ctx, int index);

// weld identical vertices (pos, norm, tex) by updating tri_list in-place
void generate_shared_vertices_index_list(std::vector<int> &tri_list_in_and_out, const SMikkTSpaceContext *ctx);

} // namespace mikktspace
} // namespace math
} // namespace wsl
