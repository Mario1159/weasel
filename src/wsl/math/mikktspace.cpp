/*! \file mikktspace.cpp
 *  \brief Pure C++ port of the MikkTSpace tangent space generator.
 *
 *  Original algorithm by Morten S. Mikkelsen.
 *  This implementation uses direct array access instead of callbacks.
 */

#include "mikktspace.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace wsl::math
{

namespace
{

// ---------------------------------------------------------------------------
// Local 3-component vector for internal algorithm use.
// ---------------------------------------------------------------------------
struct vec3
{
  float x{ 0.0F };
  float y{ 0.0F };
  float z{ 0.0F };
};

static inline bool
veq (const vec3 &a, const vec3 &b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static inline vec3
vadd (const vec3 &a, const vec3 &b)
{
  return { a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline vec3
vsub (const vec3 &a, const vec3 &b)
{
  return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline vec3
vscale (float s, const vec3 &v)
{
  return { s * v.x, s * v.y, s * v.z };
}

static inline float
vdot (const vec3 &a, const vec3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float
length_sq (const vec3 &v)
{
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float
length (const vec3 &v)
{
  return std::sqrt (length_sq (v));
}

static inline vec3
vnormalize (const vec3 &v)
{
  const float len = length (v);
  if (len == 0.0F) {
    return v;
  }
  return vscale (1.0F / len, v);
}

static inline bool
not_zero (float f)
{
  return std::fabs (f) > FLT_MIN;
}

static inline bool
vnot_zero (const vec3 &v)
{
  return not_zero (v.x) || not_zero (v.y) || not_zero (v.z);
}

// ---------------------------------------------------------------------------
// Algorithm constants
// ---------------------------------------------------------------------------
static constexpr int MARK_DEGENERATE = 1;
static constexpr int GROUP_WITH_ANY = 4;
static constexpr int ORIENT_PRESERVING = 8;
static constexpr int GI_CELLS = 2048;

// ---------------------------------------------------------------------------
// Algorithm-internal types
// ---------------------------------------------------------------------------
struct tmp_vert
{
  float vert[3]{};
  int index{ 0 };
};

struct edge
{
  int array[3]{};
};

struct sub_group
{
  int nr_faces{ 0 };
  int *members{ nullptr };
};

struct group
{
  int nr_faces{ 0 };
  int *face_indices{ nullptr };
  int vertex_representative{ 0 };
  bool orient_preserving{ false };
};

struct tri_info
{
  int face_neighbors[3]{};
  group *assigned_group[3]{};

  vec3 v_os{};
  vec3 v_ot{};
  float mag_s{ 0.0F };
  float mag_t{ 0.0F };

  int org_face_number{ 0 };
  int flags{ 0 };
  int tspaces_offs{ 0 };
  unsigned char vert_num[4]{};
};

struct tspace
{
  vec3 v_os{};
  float mag_s{ 0.0F };
  vec3 v_ot{};
  float mag_t{ 0.0F };
  int counter{ 0 };
  bool orient{ false };
};

// ---------------------------------------------------------------------------
// Index encoding: (face << 2) | vert  --  uniquely identifies a (face, vert)
// ---------------------------------------------------------------------------
static inline int
make_index (int face, int vert)
{
  assert (vert >= 0 && vert < 4 && face >= 0);
  return (face << 2) | (vert & 0x3);
}

static inline void
index_to_data (int &face, int &vert, int index)
{
  vert = index & 0x3;
  face = index >> 2;
}

// ---------------------------------------------------------------------------
// Attribute accessors: decode encoded index -> (face, vert) -> vertex index
// -> position/normal/texcoord from flat arrays.
// ---------------------------------------------------------------------------
static inline vec3
get_position (const std::vector<vec3> &positions,
              const std::vector<uint32_t> &tri_indices,
              const std::vector<int> & /*tris */, int encoded_index)
{
  int f, v;
  index_to_data (f, v, encoded_index);
  const int vi = static_cast<int> (tri_indices[f * 3 + v]);
  return positions[vi];
}

static inline vec3
get_normal (const std::vector<vec3> &normals,
            const std::vector<uint32_t> &tri_indices,
            const std::vector<int> & /*tris */, int encoded_index)
{
  int f, v;
  index_to_data (f, v, encoded_index);
  const int vi = static_cast<int> (tri_indices[f * 3 + v]);
  return normals[vi];
}

static inline vec3
get_texcoord (const std::vector<vec2f> &texcoords,
              const std::vector<uint32_t> &tri_indices,
              const std::vector<int> & /*tris */, int encoded_index)
{
  int f, v;
  index_to_data (f, v, encoded_index);
  const int vi = static_cast<int> (tri_indices[f * 3 + v]);
  return { texcoords[vi].x (), texcoords[vi].y (), 1.0F };
}

// ---------------------------------------------------------------------------
// tspace average
// ---------------------------------------------------------------------------
static inline tspace
avg_tspace (const tspace &a, const tspace &b)
{
  tspace res;
  if (a.mag_s == b.mag_s && a.mag_t == b.mag_t && veq (a.v_os, b.v_os)
      && veq (a.v_ot, b.v_ot)) {
    res = a;
  } else {
    res.mag_s = 0.5F * (a.mag_s + b.mag_s);
    res.mag_t = 0.5F * (a.mag_t + b.mag_t);
    res.v_os = vadd (a.v_os, b.v_os);
    res.v_ot = vadd (a.v_ot, b.v_ot);
    if (vnot_zero (res.v_os)) {
      res.v_os = vnormalize (res.v_os);
    }
    if (vnot_zero (res.v_ot)) {
      res.v_ot = vnormalize (res.v_ot);
    }
  }
  return res;
}

// ---------------------------------------------------------------------------
// Grid hash for vertex merging
// ---------------------------------------------------------------------------
#ifdef _MSC_VER
#define MIKK_NOINLINE __declspec (noinline)
#else
#define MIKK_NOINLINE __attribute__ ((noinline))
#endif

static MIKK_NOINLINE int
find_grid_cell (float f_min, float f_max, float f_val)
{
  const float f_index
      = static_cast<float> (GI_CELLS) * ((f_val - f_min) / (f_max - f_min));
  const int i_index = static_cast<int> (f_index);
  return i_index < GI_CELLS ? (i_index >= 0 ? i_index : 0) : (GI_CELLS - 1);
}

// ---------------------------------------------------------------------------
// Vertex merging
// ---------------------------------------------------------------------------
static void
merge_verts_fast (std::vector<int> &tri_list, std::vector<tmp_vert> &tmp_verts,
                  const std::vector<vec3> &positions,
                  const std::vector<vec3> &normals,
                  const std::vector<vec2f> &texcoords,
                  const std::vector<uint32_t> &tri_indices, int i_left_in,
                  int i_right_in)
{
  float fv_min[3], fv_max[3];
  for (int c = 0; c < 3; c++) {
    fv_min[c] = tmp_verts[i_left_in].vert[c];
    fv_max[c] = fv_min[c];
  }
  for (int l = i_left_in + 1; l <= i_right_in; l++) {
    for (int c = 0; c < 3; c++) {
      if (fv_min[c] > tmp_verts[l].vert[c]) {
        fv_min[c] = tmp_verts[l].vert[c];
      }
      if (fv_max[c] < tmp_verts[l].vert[c]) {
        fv_max[c] = tmp_verts[l].vert[c];
      }
    }
  }

  const float dx = fv_max[0] - fv_min[0];
  const float dy = fv_max[1] - fv_min[1];
  const float dz = fv_max[2] - fv_min[2];

  int channel = 0;
  if (dy > dx && dy > dz) {
    channel = 1;
  } else if (dz > dx) {
    channel = 2;
  }

  const float f_sep = 0.5F * (fv_max[channel] + fv_min[channel]);

  if (!std::isfinite (f_sep)) {
    return;
  }

  if (f_sep >= fv_max[channel] || f_sep <= fv_min[channel]) {
    for (int l = i_left_in; l <= i_right_in; l++) {
      const int i = tmp_verts[l].index;
      const int index = tri_list[i];
      const vec3 v_p = get_position (positions, tri_indices, tri_list, index);
      const vec3 v_n = get_normal (normals, tri_indices, tri_list, index);
      const vec3 v_t = get_texcoord (texcoords, tri_indices, tri_list, index);

      bool not_found = true;
      int l2 = i_left_in, i2_rec = -1;
      while (l2 < l && not_found) {
        const int i2 = tmp_verts[l2].index;
        const int index2 = tri_list[i2];
        const vec3 v_p2
            = get_position (positions, tri_indices, tri_list, index2);
        const vec3 v_n2 = get_normal (normals, tri_indices, tri_list, index2);
        const vec3 v_t2
            = get_texcoord (texcoords, tri_indices, tri_list, index2);
        i2_rec = i2;

        if (v_p.x == v_p2.x && v_p.y == v_p2.y && v_p.z == v_p2.z
            && v_n.x == v_n2.x && v_n.y == v_n2.y && v_n.z == v_n2.z
            && v_t.x == v_t2.x && v_t.y == v_t2.y && v_t.z == v_t2.z) {
          not_found = false;
        } else {
          ++l2;
        }
      }

      if (!not_found) {
        tri_list[i] = tri_list[i2_rec];
      }
    }
  } else {
    int i_l = i_left_in, i_r = i_right_in;
    assert ((i_right_in - i_left_in) > 0);

    while (i_l < i_r) {
      bool ready_left = false, ready_right = false;
      while (!ready_left && i_l < i_r) {
        ready_left = !(tmp_verts[i_l].vert[channel] < f_sep);
        if (!ready_left) {
          ++i_l;
        }
      }
      while (!ready_right && i_l < i_r) {
        ready_right = tmp_verts[i_r].vert[channel] < f_sep;
        if (!ready_right) {
          --i_r;
        }
      }
      assert ((i_l < i_r) || !(ready_left && ready_right));

      if (ready_left && ready_right) {
        const tmp_vert s_tmp = tmp_verts[i_l];
        assert (i_l < i_r);
        tmp_verts[i_l] = tmp_verts[i_r];
        tmp_verts[i_r] = s_tmp;
        ++i_l;
        --i_r;
      }
    }

    assert (i_l == (i_r + 1) || (i_l == i_r));
    if (i_l == i_r) {
      if (tmp_verts[i_r].vert[channel] < f_sep) {
        ++i_l;
      } else {
        --i_r;
      }
    }

    if (i_left_in < i_r) {
      merge_verts_fast (tri_list, tmp_verts, positions, normals, texcoords,
                        tri_indices, i_left_in, i_r);
    }
    if (i_l < i_right_in) {
      merge_verts_fast (tri_list, tmp_verts, positions, normals, texcoords,
                        tri_indices, i_l, i_right_in);
    }
  }
}

static void
generate_shared_vertices_index_list (std::vector<int> &tri_list,
                                     const std::vector<vec3> &positions,
                                     const std::vector<vec3> &normals,
                                     const std::vector<vec2f> &texcoords,
                                     const std::vector<uint32_t> &tri_indices,
                                     int nr_triangles)
{
  std::vector<int> pi_hash_table;
  std::vector<int> pi_hash_count (GI_CELLS, 0);
  std::vector<int> pi_hash_offsets (GI_CELLS, 0);
  std::vector<int> pi_hash_count2 (GI_CELLS, 0);

  vec3 v_min = get_position (positions, tri_indices, tri_list, tri_list[0]);
  vec3 v_max = v_min;
  for (int i = 1; i < (nr_triangles * 3); i++) {
    const vec3 v_p
        = get_position (positions, tri_indices, tri_list, tri_list[i]);
    if (v_min.x > v_p.x) {
      v_min.x = v_p.x;
    } else if (v_max.x < v_p.x) {
      v_max.x = v_p.x;
    }
    if (v_min.y > v_p.y) {
      v_min.y = v_p.y;
    } else if (v_max.y < v_p.y) {
      v_max.y = v_p.y;
    }
    if (v_min.z > v_p.z) {
      v_min.z = v_p.z;
    } else if (v_max.z < v_p.z) {
      v_max.z = v_p.z;
    }
  }

  const vec3 v_dim = vsub (v_max, v_min);
  int channel = 0;
  float f_min = v_min.x;
  float f_max = v_max.x;
  if (v_dim.y > v_dim.x && v_dim.y > v_dim.z) {
    channel = 1;
    f_min = v_min.y;
    f_max = v_max.y;
  } else if (v_dim.z > v_dim.x) {
    channel = 2;
    f_min = v_min.z;
    f_max = v_max.z;
  }

  for (int i = 0; i < (nr_triangles * 3); i++) {
    const vec3 v_p
        = get_position (positions, tri_indices, tri_list, tri_list[i]);
    const float f_val = channel == 0 ? v_p.x : (channel == 1 ? v_p.y : v_p.z);
    ++pi_hash_count[find_grid_cell (f_min, f_max, f_val)];
  }

  pi_hash_offsets[0] = 0;
  for (int k = 1; k < GI_CELLS; k++) {
    pi_hash_offsets[k] = pi_hash_offsets[k - 1] + pi_hash_count[k - 1];
  }

  pi_hash_table.resize (nr_triangles * 3);
  for (int i = 0; i < (nr_triangles * 3); i++) {
    const vec3 v_p
        = get_position (positions, tri_indices, tri_list, tri_list[i]);
    const float f_val = channel == 0 ? v_p.x : (channel == 1 ? v_p.y : v_p.z);
    const int cell = find_grid_cell (f_min, f_max, f_val);
    pi_hash_table[pi_hash_offsets[cell] + pi_hash_count2[cell]] = i;
    ++pi_hash_count2[cell];
  }

  int max_count = 0;
  for (int k = 0; k < GI_CELLS; k++) {
    if (max_count < pi_hash_count[k]) {
      max_count = pi_hash_count[k];
    }
  }
  std::vector<tmp_vert> tmp_verts (max_count);

  for (int k = 0; k < GI_CELLS; k++) {
    const int *p_table = &pi_hash_table[pi_hash_offsets[k]];
    const int entries = pi_hash_count[k];
    if (entries < 2) {
      continue;
    }
    for (int e = 0; e < entries; e++) {
      const int i = p_table[e];
      const vec3 v_p
          = get_position (positions, tri_indices, tri_list, tri_list[i]);
      tmp_verts[e].vert[0] = v_p.x;
      tmp_verts[e].vert[1] = v_p.y;
      tmp_verts[e].vert[2] = v_p.z;
      tmp_verts[e].index = i;
    }
    merge_verts_fast (tri_list, tmp_verts, positions, normals, texcoords,
                      tri_indices, 0, entries - 1);
  }
}

// ---------------------------------------------------------------------------
// CalcTexArea
// ---------------------------------------------------------------------------
static inline float
calc_tex_area (const std::vector<vec2f> &texcoords,
               const std::vector<uint32_t> &tri_indices,
               const std::vector<int> &tris, const int indices[3])
{
  const vec3 t1 = get_texcoord (texcoords, tri_indices, tris, indices[0]);
  const vec3 t2 = get_texcoord (texcoords, tri_indices, tris, indices[1]);
  const vec3 t3 = get_texcoord (texcoords, tri_indices, tris, indices[2]);

  const float t21x = t2.x - t1.x;
  const float t21y = t2.y - t1.y;
  const float t31x = t3.x - t1.x;
  const float t31y = t3.y - t1.y;

  const float f_signed_area_st_x2 = t21x * t31y - t21y * t31x;

  return f_signed_area_st_x2 < 0 ? (-f_signed_area_st_x2) : f_signed_area_st_x2;
}

// ---------------------------------------------------------------------------
// BuildNeighborsFast / BuildNeighborsSlow
// ---------------------------------------------------------------------------
static void
build_neighbors_fast (tri_info *tri_infos, edge *edges,
                      const std::vector<int> &tris, int nr_triangles)
{
  int i_edge_count = 0;
  for (int f = 0; f < nr_triangles; f++) {
    for (int i = 0; i < 3; i++) {
      const int index = tris[f * 3 + i];
      edges[i_edge_count].array[0] = index;
      edges[i_edge_count].array[1] = tris[f * 3 + ((i + 1) % 3)];
      edges[i_edge_count].array[2] = f;
      ++i_edge_count;
    }
  }

  std::sort (edges, edges + i_edge_count, [] (const edge &a, const edge &b) {
    if (a.array[0] != b.array[0]) {
      return a.array[0] < b.array[0];
    }
    return a.array[1] < b.array[1];
  });

  int i = 0;
  while (i < i_edge_count) {
    int j = i + 1;
    while (j < i_edge_count && edges[j].array[0] == edges[i].array[0]
           && edges[j].array[1] == edges[i].array[1]) {
      const int f1 = edges[i].array[2];
      const int f2 = edges[j].array[2];
      const int idx0 = edges[i].array[2];
      const int idx1 = edges[j].array[2];
      tri_infos[f1].face_neighbors[idx0] = f2;
      tri_infos[f2].face_neighbors[idx1] = f1;
      ++j;
    }
    ++i;
  }
}

// ---------------------------------------------------------------------------
// InitTriInfo
// ---------------------------------------------------------------------------
static void
init_tri_info (tri_info *tri_infos, const std::vector<int> &tris,
               const std::vector<vec3> &positions,
               const std::vector<vec3> & /*normals */,
               const std::vector<vec2f> &texcoords,
               const std::vector<uint32_t> &tri_indices, int nr_triangles)
{
  for (int f = 0; f < nr_triangles; f++) {
    for (int i = 0; i < 3; i++) {
      tri_infos[f].face_neighbors[i] = -1;
      tri_infos[f].assigned_group[i] = nullptr;
      tri_infos[f].v_os = {};
      tri_infos[f].v_ot = {};
      tri_infos[f].mag_s = 0;
      tri_infos[f].mag_t = 0;
      tri_infos[f].flags |= GROUP_WITH_ANY;
    }
  }

  for (int f = 0; f < nr_triangles; f++) {
    const vec3 v1
        = get_position (positions, tri_indices, tris, tris[f * 3 + 0]);
    const vec3 v2
        = get_position (positions, tri_indices, tris, tris[f * 3 + 1]);
    const vec3 v3
        = get_position (positions, tri_indices, tris, tris[f * 3 + 2]);
    const vec3 t1
        = get_texcoord (texcoords, tri_indices, tris, tris[f * 3 + 0]);
    const vec3 t2
        = get_texcoord (texcoords, tri_indices, tris, tris[f * 3 + 1]);
    const vec3 t3
        = get_texcoord (texcoords, tri_indices, tris, tris[f * 3 + 2]);

    const float t21x = t2.x - t1.x;
    const float t21y = t2.y - t1.y;
    const float t31x = t3.x - t1.x;
    const float t31y = t3.y - t1.y;
    const vec3 d1 = vsub (v2, v1);
    const vec3 d2 = vsub (v3, v1);

    const float f_signed_area_st_x2 = t21x * t31y - t21y * t31x;
    vec3 v_os = vsub (vscale (t31y, d1), vscale (t21y, d2));
    vec3 v_ot = vadd (vscale (-t31x, d1), vscale (t21x, d2));

    tri_infos[f].flags |= (f_signed_area_st_x2 > 0 ? ORIENT_PRESERVING : 0);

    if (not_zero (f_signed_area_st_x2)) {
      const float f_abs_area = std::fabs (f_signed_area_st_x2);
      const float f_len_os = length (v_os);
      const float f_len_ot = length (v_ot);
      const float f_s
          = (tri_infos[f].flags & ORIENT_PRESERVING) == 0 ? (-1.0F) : 1.0F;
      if (not_zero (f_len_os)) {
        tri_infos[f].v_os = vscale (f_s / f_len_os, v_os);
      }
      if (not_zero (f_len_ot)) {
        tri_infos[f].v_ot = vscale (f_s / f_len_ot, v_ot);
      }

      tri_infos[f].mag_s = f_len_os / f_abs_area;
      tri_infos[f].mag_t = f_len_ot / f_abs_area;

      if (not_zero (tri_infos[f].mag_s) && not_zero (tri_infos[f].mag_t)) {
        tri_infos[f].flags &= (~GROUP_WITH_ANY);
      }
    }
  }

  // Force otherwise healthy quads to a fixed orientation
  int t = 0;
  while (t < (nr_triangles - 1)) {
    const int i_fo_a = tri_infos[t].org_face_number;
    const int i_fo_b = tri_infos[t + 1].org_face_number;
    if (i_fo_a == i_fo_b) {
      const bool is_deg_a = (tri_infos[t].flags & MARK_DEGENERATE) != 0;
      const bool is_deg_b = (tri_infos[t + 1].flags & MARK_DEGENERATE) != 0;

      if (!is_deg_a && !is_deg_b) {
        const bool orient_a = (tri_infos[t].flags & ORIENT_PRESERVING) != 0;
        const bool orient_b = (tri_infos[t + 1].flags & ORIENT_PRESERVING) != 0;
        if (orient_a != orient_b) {
          bool choose_first = false;
          if ((tri_infos[t + 1].flags & GROUP_WITH_ANY) != 0) {
            choose_first = true;
          } else {
            const int idx_a[3]
                = { tris[t * 3 + 0], tris[t * 3 + 1], tris[t * 3 + 2] };
            const int idx_b[3] = { tris[(t + 1) * 3 + 0], tris[(t + 1) * 3 + 1],
                                   tris[(t + 1) * 3 + 2] };
            const float area_a
                = calc_tex_area (texcoords, tri_indices, tris, idx_a);
            const float area_b
                = calc_tex_area (texcoords, tri_indices, tris, idx_b);
            if (area_a >= area_b) {
              choose_first = true;
            }
          }

          const int t0 = choose_first ? t : (t + 1);
          const int t1 = choose_first ? (t + 1) : t;
          tri_infos[t1].flags &= (~ORIENT_PRESERVING);
          tri_infos[t1].flags |= (tri_infos[t0].flags & ORIENT_PRESERVING);
        }
      }
      t += 2;
    } else {
      ++t;
    }
  }

  // Match up edge pairs
  {
    std::vector<edge> edges (nr_triangles * 3);
    build_neighbors_fast (tri_infos, edges.data (), tris, nr_triangles);
  }
}

// ---------------------------------------------------------------------------
// AddTriToGroup / AssignRecur / Build4RuleGroups
// ---------------------------------------------------------------------------
static void
add_tri_to_group (group *grp, int tri_index)
{
  grp->face_indices[grp->nr_faces] = tri_index;
  ++grp->nr_faces;
}

static bool assign_recur (const std::vector<int> &tris, tri_info *tri_infos,
                          int tri_index, group *grp);

static bool
assign_recur (const std::vector<int> &tris, tri_info *tri_infos, int tri_index,
              group *grp)
{
  tri_info *my_info = &tri_infos[tri_index];

  const int i_vert_rep = grp->vertex_representative;
  const int *p_verts = &tris[3 * tri_index];
  int i = -1;
  if (p_verts[0] == i_vert_rep) {
    i = 0;
  } else if (p_verts[1] == i_vert_rep) {
    i = 1;
  } else if (p_verts[2] == i_vert_rep) {
    i = 2;
  }
  assert (i >= 0 && i < 3);

  if (my_info->assigned_group[i] == grp) {
    return true;
  }
  if (my_info->assigned_group[i] != nullptr) {
    return false;
  }
  if ((my_info->flags & GROUP_WITH_ANY) != 0) {
    if (my_info->assigned_group[0] == nullptr
        && my_info->assigned_group[1] == nullptr
        && my_info->assigned_group[2] == nullptr) {
      my_info->flags &= (~ORIENT_PRESERVING);
      my_info->flags |= (grp->orient_preserving ? ORIENT_PRESERVING : 0);
    }
  }
  {
    const bool orient = (my_info->flags & ORIENT_PRESERVING) != 0;
    if (orient != grp->orient_preserving) {
      return false;
    }
  }

  add_tri_to_group (grp, tri_index);
  my_info->assigned_group[i] = grp;

  {
    const int neigh_l = my_info->face_neighbors[i];
    const int neigh_r = my_info->face_neighbors[i > 0 ? (i - 1) : 2];
    if (neigh_l >= 0) {
      assign_recur (tris, tri_infos, neigh_l, grp);
    }
    if (neigh_r >= 0) {
      assign_recur (tris, tri_infos, neigh_r, grp);
    }
  }

  return true;
}

static int
build_4_rule_groups (tri_info *tri_infos, group *groups,
                     int *group_triangles_buffer, const std::vector<int> &tris,
                     int nr_triangles)
{
  const int nr_max_groups = nr_triangles * 3;
  int nr_active_groups = 0;
  int offset = 0;

  for (int f = 0; f < nr_triangles; f++) {
    for (int i = 0; i < 3; i++) {
      if ((tri_infos[f].flags & GROUP_WITH_ANY) == 0
          && tri_infos[f].assigned_group[i] == nullptr) {
        const int vert_index = tris[f * 3 + i];
        assert (nr_active_groups < nr_max_groups);
        tri_infos[f].assigned_group[i] = &groups[nr_active_groups];
        tri_infos[f].assigned_group[i]->vertex_representative = vert_index;
        tri_infos[f].assigned_group[i]->orient_preserving
            = (tri_infos[f].flags & ORIENT_PRESERVING) != 0;
        tri_infos[f].assigned_group[i]->nr_faces = 0;
        tri_infos[f].assigned_group[i]->face_indices
            = &group_triangles_buffer[offset];
        ++nr_active_groups;

        add_tri_to_group (tri_infos[f].assigned_group[i], f);
        const int neigh_l = tri_infos[f].face_neighbors[i];
        const int neigh_r = tri_infos[f].face_neighbors[i > 0 ? (i - 1) : 2];
        if (neigh_l >= 0) {
          assign_recur (tris, tri_infos, neigh_l,
                        tri_infos[f].assigned_group[i]);
        }
        if (neigh_r >= 0) {
          assign_recur (tris, tri_infos, neigh_r,
                        tri_infos[f].assigned_group[i]);
        }

        offset += tri_infos[f].assigned_group[i]->nr_faces;
        assert (offset <= nr_max_groups);
      }
    }
  }

  return nr_active_groups;
}

// ---------------------------------------------------------------------------
// QuickSort
// ---------------------------------------------------------------------------
static void
quick_sort (int *buffer, int left, int right)
{
  if (left >= right) {
    return;
  }
  int mid = (left + right) >> 1;
  if (buffer[left] > buffer[mid]) {
    std::swap (buffer[left], buffer[mid]);
  }
  if (buffer[left] > buffer[right]) {
    std::swap (buffer[left], buffer[right]);
  }
  if (buffer[mid] > buffer[right]) {
    std::swap (buffer[mid], buffer[right]);
  }
  std::swap (buffer[mid], buffer[left]);

  const int pivot = buffer[left];
  int last = left;
  for (int i = left + 1; i <= right; i++) {
    if (buffer[i] < pivot) {
      ++last;
      std::swap (buffer[last], buffer[i]);
    }
  }
  std::swap (buffer[left], buffer[last]);
  quick_sort (buffer, left, last - 1);
  quick_sort (buffer, last + 1, right);
}

// ---------------------------------------------------------------------------
// CompareSubGroups / EvalTspace
// ---------------------------------------------------------------------------
static bool
compare_sub_groups (const sub_group *pg1, const sub_group *pg2)
{
  if (pg1->nr_faces != pg2->nr_faces) {
    return false;
  }
  for (int i = 0; i < pg1->nr_faces; i++) {
    if (pg1->members[i] != pg2->members[i]) {
      return false;
    }
  }
  return true;
}

static tspace
eval_tspace (int *face_indices, int i_faces, const std::vector<int> &tris,
             const tri_info *tri_infos, const std::vector<vec3> &positions,
             const std::vector<vec3> &normals,
             const std::vector<vec2f> & /*texcoords */,
             const std::vector<uint32_t> &tri_indices,
             int i_vertex_representative)
{
  tspace res{};
  float f_angle_sum = 0;

  for (int face = 0; face < i_faces; face++) {
    const int f = face_indices[face];

    if ((tri_infos[f].flags & GROUP_WITH_ANY) == 0) {
      int i = -1;
      if (tris[3 * f + 0] == i_vertex_representative) {
        i = 0;
      } else if (tris[3 * f + 1] == i_vertex_representative) {
        i = 1;
      } else if (tris[3 * f + 2] == i_vertex_representative) {
        i = 2;
      }
      assert (i >= 0 && i < 3);

      const int index = tris[3 * f + i];
      const vec3 n = get_normal (normals, tri_indices, tris, index);
      vec3 v_os
          = vsub (tri_infos[f].v_os, vscale (vdot (n, tri_infos[f].v_os), n));
      vec3 v_ot
          = vsub (tri_infos[f].v_ot, vscale (vdot (n, tri_infos[f].v_ot), n));
      if (vnot_zero (v_os)) {
        v_os = vnormalize (v_os);
      }
      if (vnot_zero (v_ot)) {
        v_ot = vnormalize (v_ot);
      }

      const int i2 = tris[3 * f + (i < 2 ? (i + 1) : 0)];
      const int i1 = tris[3 * f + i];
      const int i0 = tris[3 * f + (i > 0 ? (i - 1) : 2)];

      const vec3 p0 = get_position (positions, tri_indices, tris, i0);
      const vec3 p1 = get_position (positions, tri_indices, tris, i1);
      const vec3 p2 = get_position (positions, tri_indices, tris, i2);
      const vec3 v1 = vsub (p1, p0);
      const vec3 v2 = vsub (p2, p0);

      float f_cos = vdot (v1, v2) / (length (v1) * length (v2));
      f_cos = f_cos < -1.0F ? -1.0F : (f_cos > 1.0F ? 1.0F : f_cos);
      const float f_angle = std::acos (f_cos);
      f_angle_sum += f_angle;

      res.v_os = vadd (res.v_os, vscale (f_angle, v_os));
      res.v_ot = vadd (res.v_ot, vscale (f_angle, v_ot));

      res.mag_s += f_angle * tri_infos[f].mag_s;
      res.mag_t += f_angle * tri_infos[f].mag_t;
    }
  }

  if (vnot_zero (res.v_os)) {
    res.v_os = vnormalize (res.v_os);
  }
  if (vnot_zero (res.v_ot)) {
    res.v_ot = vnormalize (res.v_ot);
  }
  if (f_angle_sum > 0) {
    res.mag_s /= f_angle_sum;
    res.mag_t /= f_angle_sum;
  } else {
    res.mag_s = 1.0F;
    res.mag_t = 1.0F;
  }

  return res;
}

// ---------------------------------------------------------------------------
// GenerateTSpaces
// ---------------------------------------------------------------------------
static bool
generate_tspaces (tspace *tspaces, const tri_info *tri_infos,
                  const group *groups, int nr_active_groups,
                  const std::vector<int> &tris, float f_thres_cos,
                  const std::vector<vec3> &positions,
                  const std::vector<vec3> &normals,
                  const std::vector<vec2f> &texcoords,
                  const std::vector<uint32_t> &tri_indices)
{
  int max_nr_faces = 0;
  for (int g = 0; g < nr_active_groups; g++) {
    if (max_nr_faces < groups[g].nr_faces) {
      max_nr_faces = groups[g].nr_faces;
    }
  }

  if (max_nr_faces == 0) {
    return true;
  }

  std::vector<tspace> sub_group_tspaces (max_nr_faces);
  std::vector<sub_group> uni_sub_groups (max_nr_faces);
  std::vector<int> tmp_members (max_nr_faces);

  for (int g = 0; g < nr_active_groups; g++) {
    const group *grp = &groups[g];
    int unique_sub_groups = 0;

    for (int i = 0; i < grp->nr_faces; i++) {
      const int f = grp->face_indices[i];
      int index = -1;
      if (tri_infos[f].assigned_group[0] == grp) {
        index = 0;
      } else if (tri_infos[f].assigned_group[1] == grp) {
        index = 1;
      } else if (tri_infos[f].assigned_group[2] == grp) {
        index = 2;
      }
      assert (index >= 0 && index < 3);

      const int i_vert_index = tris[f * 3 + index];
      assert (i_vert_index == grp->vertex_representative);

      const vec3 n = get_normal (normals, tri_indices, tris, i_vert_index);

      vec3 v_os
          = vsub (tri_infos[f].v_os, vscale (vdot (n, tri_infos[f].v_os), n));
      vec3 v_ot
          = vsub (tri_infos[f].v_ot, vscale (vdot (n, tri_infos[f].v_ot), n));
      if (vnot_zero (v_os)) {
        v_os = vnormalize (v_os);
      }
      if (vnot_zero (v_ot)) {
        v_ot = vnormalize (v_ot);
      }

      const int i_of_1 = tri_infos[f].org_face_number;

      int i_members = 0;
      for (int j = 0; j < grp->nr_faces; j++) {
        const int t = grp->face_indices[j];
        const int i_of_2 = tri_infos[t].org_face_number;

        vec3 v_os2
            = vsub (tri_infos[t].v_os, vscale (vdot (n, tri_infos[t].v_os), n));
        vec3 v_ot2
            = vsub (tri_infos[t].v_ot, vscale (vdot (n, tri_infos[t].v_ot), n));
        if (vnot_zero (v_os2)) {
          v_os2 = vnormalize (v_os2);
        }
        if (vnot_zero (v_ot2)) {
          v_ot2 = vnormalize (v_ot2);
        }

        const bool b_any
            = ((tri_infos[f].flags | tri_infos[t].flags) & GROUP_WITH_ANY) != 0;
        const bool b_same_org_face = i_of_1 == i_of_2;

        const float f_cos_s = vdot (v_os, v_os2);
        const float f_cos_t = vdot (v_ot, v_ot2);

        assert (f != t || b_same_org_face);
        if (b_any || b_same_org_face
            || (f_cos_s > f_thres_cos && f_cos_t > f_thres_cos)) {
          tmp_members[i_members++] = t;
        }
      }

      sub_group tmp_group{};
      tmp_group.nr_faces = i_members;
      tmp_group.members = tmp_members.data ();
      if (i_members > 1) {
        quick_sort (tmp_members.data (), 0, i_members - 1);
      }

      bool found = false;
      int l = 0;
      while (l < unique_sub_groups && !found) {
        found = compare_sub_groups (&tmp_group, &uni_sub_groups[l]);
        if (!found) {
          ++l;
        }
      }

      assert (found || l == unique_sub_groups);

      if (!found) {
        int *p_indices = new (std::nothrow) int[i_members];
        if (p_indices == nullptr) {
          for (int s = 0; s < unique_sub_groups; s++) {
            delete[] uni_sub_groups[s].members;
          }
          return false;
        }
        uni_sub_groups[unique_sub_groups].nr_faces = i_members;
        uni_sub_groups[unique_sub_groups].members = p_indices;
        std::memcpy (p_indices, tmp_group.members, i_members * sizeof (int));
        sub_group_tspaces[unique_sub_groups] = eval_tspace (
            tmp_group.members, i_members, tris, tri_infos, positions, normals,
            texcoords, tri_indices, grp->vertex_representative);
        ++unique_sub_groups;
      }

      {
        const int i_offs = tri_infos[f].tspaces_offs;
        const int i_vert = tri_infos[f].vert_num[index];
        tspace *p_ts_out = &tspaces[i_offs + i_vert];
        assert (p_ts_out->counter < 2);
        assert (((tri_infos[f].flags & ORIENT_PRESERVING) != 0)
                == grp->orient_preserving);
        if (p_ts_out->counter == 1) {
          *p_ts_out = avg_tspace (*p_ts_out, sub_group_tspaces[l]);
          p_ts_out->counter = 2;
          p_ts_out->orient = grp->orient_preserving;
        } else {
          assert (p_ts_out->counter == 0);
          *p_ts_out = sub_group_tspaces[l];
          p_ts_out->counter = 1;
          p_ts_out->orient = grp->orient_preserving;
        }
      }
    }

    for (int s = 0; s < unique_sub_groups; s++) {
      delete[] uni_sub_groups[s].members;
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Main algorithm
// ---------------------------------------------------------------------------
static bool
gen_tang_space_impl (const std::vector<vec3> &positions,
                     const std::vector<vec3> &normals,
                     const std::vector<vec2f> &texcoords,
                     const std::vector<uint32_t> &triangle_indices,
                     std::vector<vec4f> &out_tangents, float angular_threshold)
{
  const int num_verts = static_cast<int> (positions.size ());
  const int num_triangles = static_cast<int> (triangle_indices.size () / 3);

  if (num_triangles == 0 || num_verts == 0) {
    return false;
  }

  out_tangents.resize (num_verts, vec4f (1.0F, 0.0F, 0.0F, 1.0F));

  // Step 1: Build encoded index list: tris[face*3+vert] = make_index(face,
  // vert)
  std::vector<int> tris (num_triangles * 3);
  for (int f = 0; f < num_triangles; f++) {
    for (int v = 0; v < 3; v++) {
      tris[f * 3 + v] = make_index (f, v);
    }
  }

  // Step 2: Weld identical vertices
  generate_shared_vertices_index_list (tris, positions, normals, texcoords,
                                       triangle_indices, num_triangles);

  // Step 3: Set per-triangle metadata (face number, tspace offset, vert
  // numbers)
  std::vector<tri_info> tri_infos (num_triangles);
  {
    int tspaces_offs = 0;
    for (int f = 0; f < num_triangles; f++) {
      tri_infos[f].org_face_number = f;
      tri_infos[f].tspaces_offs = tspaces_offs;
      tri_infos[f].vert_num[0] = 0;
      tri_infos[f].vert_num[1] = 1;
      tri_infos[f].vert_num[2] = 2;
      tri_infos[f].vert_num[3] = 0;
      tri_infos[f].flags = 0;
      tspaces_offs += 3;
    }
  }

  // Step 3b: Compute derivatives, neighbors
  init_tri_info (tri_infos.data (), tris, positions, normals, texcoords,
                 triangle_indices, num_triangles);

  // Step 4: Build groups
  const int nr_max_groups = num_triangles * 3;
  std::vector<int> group_triangles_buffer (nr_max_groups);
  std::vector<group> groups (nr_max_groups);

  const int nr_active_groups = build_4_rule_groups (
      tri_infos.data (), groups.data (), group_triangles_buffer.data (), tris,
      num_triangles);

  // Step 5: Generate tangent spaces
  std::vector<tspace> tspaces (num_triangles * 3);

  const float f_thres_cos
      = std::cos (angular_threshold * 3.14159265358979323846F / 180.0F);

  const bool ok = generate_tspaces (
      tspaces.data (), tri_infos.data (), groups.data (), nr_active_groups,
      tris, f_thres_cos, positions, normals, texcoords, triangle_indices);

  if (!ok) {
    return false;
  }

  // Step 6: Write output tangents
  // For each triangle face, copy the per-vertex tangent to the output.
  // Since vertices may be shared (after welding), the last face that writes
  // wins. For properly indexed meshes this is fine; for meshes with
  // duplicates, the tangent will be the same anyway.
  for (int f = 0; f < num_triangles; f++) {
    for (int v = 0; v < 3; v++) {
      const int global_vert = static_cast<int> (triangle_indices[f * 3 + v]);
      const int i_offs = tri_infos[f].tspaces_offs;
      const int i_vert = tri_infos[f].vert_num[v];
      const tspace &ts = tspaces[i_offs + i_vert];
      const float sign = ts.orient ? 1.0F : -1.0F;
      out_tangents[global_vert] = vec4f (ts.v_os.x, ts.v_os.y, ts.v_os.z, sign);
    }
  }

  return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool
mikktspace_generator::gen_tang_space_default (
    const std::vector<vec3f> &positions, const std::vector<vec3f> &normals,
    const std::vector<vec2f> &texcoords,
    const std::vector<uint32_t> &triangle_indices,
    std::vector<vec4f> &out_tangents) const noexcept
{
  return gen_tang_space (positions, normals, texcoords, triangle_indices,
                         out_tangents, 180.0F);
}

bool
mikktspace_generator::gen_tang_space (
    const std::vector<vec3f> &positions, const std::vector<vec3f> &normals,
    const std::vector<vec2f> &texcoords,
    const std::vector<uint32_t> &triangle_indices,
    std::vector<vec4f> &out_tangents, float angular_threshold) const noexcept
{
  std::vector<vec3> pos (positions.size ());
  std::vector<vec3> nrm (normals.size ());
  for (size_t i = 0; i < positions.size (); ++i) {
    pos[i] = { positions[i].x (), positions[i].y (), positions[i].z () };
    nrm[i] = { normals[i].x (), normals[i].y (), normals[i].z () };
  }

  return gen_tang_space_impl (pos, nrm, texcoords, triangle_indices,
                              out_tangents, angular_threshold);
}

} // namespace wsl::math
