/*! \file mikktspace_internal.cpp
 *  \brief Minimal implementations for core helpers used by the C++ port.
 */

#include "mikktspace_internal.hpp"
#include <cmath>

namespace wsl {
namespace math {
namespace mikktspace {

int generate_initial_vertices_index_list (std::vector<int> &tri_list_out,
                                          std::vector<tspace> &out_tspaces,
                                          const SMikkTSpaceContext *ctx)
{
  // For the incremental port we keep the original face-splitting strategy:
  // triangles are emitted as-is and quads are split along the shortest
  // diagonal in texture space. This mirrors the behaviour of the original
  // implementation and prepares for a one-to-one algorithm port.

  const int num_faces = ctx->m_pInterface->m_getNumFaces(ctx);
  tri_list_out.clear();
  out_tspaces.clear();

  int tspace_offset = 0;
  for (int f = 0; f < num_faces; ++f)
  {
    const int verts = ctx->m_pInterface->m_getNumVerticesOfFace(ctx, f);
    if (verts != 3 && verts != 4) continue;

    out_tspaces.resize(tspace_offset + verts);

    if (verts == 3)
    {
      tri_list_out.push_back(make_index(f, 0));
      tri_list_out.push_back(make_index(f, 1));
      tri_list_out.push_back(make_index(f, 2));
      tspace_offset += 3;
    }
    else
    {
      // quad - decide diagonal using texture coordinates (then position tie-break)
      float tex0[2], tex1[2], tex2[2], tex3[2];
      ctx->m_pInterface->m_getTexCoord(ctx, tex0, f, 0);
      ctx->m_pInterface->m_getTexCoord(ctx, tex1, f, 1);
      ctx->m_pInterface->m_getTexCoord(ctx, tex2, f, 2);
      ctx->m_pInterface->m_getTexCoord(ctx, tex3, f, 3);

      auto dist_sq_02 = (tex2[0]-tex0[0])*(tex2[0]-tex0[0]) + (tex2[1]-tex0[1])*(tex2[1]-tex0[1]);
      auto dist_sq_13 = (tex3[0]-tex1[0])*(tex3[0]-tex1[0]) + (tex3[1]-tex1[1])*(tex3[1]-tex1[1]);

      bool diag_02 = false;
      if (dist_sq_02 < dist_sq_13) diag_02 = true;
      else if (dist_sq_13 < dist_sq_02) diag_02 = false;
      else
      {
        float pos0[3], pos1[3], pos2[3], pos3[3];
        ctx->m_pInterface->m_getPosition(ctx, pos0, f, 0);
        ctx->m_pInterface->m_getPosition(ctx, pos1, f, 1);
        ctx->m_pInterface->m_getPosition(ctx, pos2, f, 2);
        ctx->m_pInterface->m_getPosition(ctx, pos3, f, 3);
        float pd02 = (pos2[0]-pos0[0])*(pos2[0]-pos0[0]) + (pos2[1]-pos0[1])*(pos2[1]-pos0[1]) + (pos2[2]-pos0[2])*(pos2[2]-pos0[2]);
        float pd13 = (pos3[0]-pos1[0])*(pos3[0]-pos1[0]) + (pos3[1]-pos1[1])*(pos3[1]-pos1[1]) + (pos3[2]-pos1[2])*(pos3[2]-pos1[2]);
        diag_02 = pd02 <= pd13;
      }

      if (diag_02)
      {
        tri_list_out.push_back(make_index(f, 0));
        tri_list_out.push_back(make_index(f, 1));
        tri_list_out.push_back(make_index(f, 2));

        tri_list_out.push_back(make_index(f, 0));
        tri_list_out.push_back(make_index(f, 2));
        tri_list_out.push_back(make_index(f, 3));
      }
      else
      {
        tri_list_out.push_back(make_index(f, 0));
        tri_list_out.push_back(make_index(f, 1));
        tri_list_out.push_back(make_index(f, 3));

        tri_list_out.push_back(make_index(f, 1));
        tri_list_out.push_back(make_index(f, 2));
        tri_list_out.push_back(make_index(f, 3));
      }
      tspace_offset += 4;
    }
  }

  return tspace_offset;
}

// attribute helpers
vec3f get_position(const SMikkTSpaceContext *ctx, int index)
{
  int f, v; index_to_data(f, v, index);
  float pos[3] = {0,0,0};
  ctx->m_pInterface->m_getPosition(ctx, pos, f, v);
  return vec3f{ pos[0], pos[1], pos[2] };
}

vec3f get_normal(const SMikkTSpaceContext *ctx, int index)
{
  int f, v; index_to_data(f, v, index);
  float n[3] = {0,0,0};
  ctx->m_pInterface->m_getNormal(ctx, n, f, v);
  return vec3f{ n[0], n[1], n[2] };
}

vec3f get_texcoord(const SMikkTSpaceContext *ctx, int index)
{
  int f, v; index_to_data(f, v, index);
  float t[2] = {0,0};
  ctx->m_pInterface->m_getTexCoord(ctx, t, f, v);
  return vec3f{ t[0], t[1], 1.0f };
}

void generate_shared_vertices_index_list(std::vector<int> &tri_list_in_and_out, const SMikkTSpaceContext *ctx)
{
  const int n = static_cast<int>(tri_list_in_and_out.size());
  for (int i = 0; i < n; ++i)
  {
    int idx_i = tri_list_in_and_out[i];
    vec3f p = get_position(ctx, idx_i);
    vec3f nrm = get_normal(ctx, idx_i);
    vec3f tc = get_texcoord(ctx, idx_i);

    // search previous entries
    for (int j = 0; j < i; ++j)
    {
      int idx_j = tri_list_in_and_out[j];
      vec3f pj = get_position(ctx, idx_j);
      vec3f nj = get_normal(ctx, idx_j);
      vec3f tj = get_texcoord(ctx, idx_j);
      if (veq(p, pj) && veq(nrm, nj) && veq(tc, tj))
      {
        tri_list_in_and_out[i] = tri_list_in_and_out[j];
        break;
      }
    }
  }
}

} // namespace mikktspace
} // namespace math
} // namespace wsl
