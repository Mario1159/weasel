#pragma once

#include "vector.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace wsl::math
{

/**
 * Pure C++ MikkTSpace tangent space generator.
 *
 *  Generates tangent and bitangent vectors for normal mapping using
 *  the MikkTSpace algorithm. Uses vertex data directly instead of callbacks.
 */
class mikktspace_generator
{
public:
  mikktspace_generator () noexcept = default;

  struct vertex
  {
    vec3f position;
    vec3f normal;
    vec2f texcoord;
  };

  struct tangent_data
  {
    vec4f tangent;
  };

  /**
   * Generate tangent space using the default angular threshold (180°).
   *
   * :param positions: Vertex positions (3 floats per vertex).
   * :param normals: Vertex normals (3 floats per vertex).
   * :param texcoords: Vertex texture coordinates (2 floats per vertex).
   * :param triangle_indices: Triangle index buffer (3 indices per triangle).
   * :param out_tangents: Per-vertex tangent output (same size as positions).
   * :return: true on success, false on failure.
   */
  bool gen_tang_space_default (const std::vector<vec3f> &positions,
                               const std::vector<vec3f> &normals,
                               const std::vector<vec2f> &texcoords,
                               const std::vector<uint32_t> &triangle_indices,
                               std::vector<vec4f> &out_tangents) const noexcept;

  /**
   * Generate tangent space with a custom angular threshold.
   *
   * :param positions: Vertex positions (3 floats per vertex).
   * :param normals: Vertex normals (3 floats per vertex).
   * :param texcoords: Vertex texture coordinates (2 floats per vertex).
   * :param triangle_indices: Triangle index buffer (3 indices per triangle).
   * :param out_tangents: Per-vertex tangent output (same size as positions).
   * :param angular_threshold: Threshold in degrees used when splitting groups.
   * :return: true on success, false on failure.
   */
  bool gen_tang_space (const std::vector<vec3f> &positions,
                       const std::vector<vec3f> &normals,
                       const std::vector<vec2f> &texcoords,
                       const std::vector<uint32_t> &triangle_indices,
                       std::vector<vec4f> &out_tangents,
                       float angular_threshold) const noexcept;

  /**
   * Generate tangent space from a vertex array with accessors.
   *
   * Generic version that works with any vertex type via accessor lambdas.
   *
   * :param vertices: Array of vertices.
   * :param triangle_indices: Triangle index buffer (3 indices per triangle).
   * :param out_tangents: Per-vertex tangent output (same size as vertices).
   * :param get_position: Lambda returning vec3f position from a vertex.
   * :param get_normal: Lambda returning vec3f normal from a vertex.
   * :param get_texcoord: Lambda returning vec2f texcoord from a vertex.
   * :param angular_threshold: Threshold in degrees used when splitting groups.
   * :return: true on success, false on failure.
   */
  template <typename Vertex, typename PosFn, typename NormFn, typename UVFn>
  bool
  gen_tang_space (const std::vector<Vertex> &vertices,
                  const std::vector<uint32_t> &triangle_indices,
                  std::vector<vec4f> &out_tangents, PosFn get_position,
                  NormFn get_normal, UVFn get_texcoord,
                  float angular_threshold) const noexcept
  {
    std::vector<vec3f> positions (vertices.size ());
    std::vector<vec3f> normals (vertices.size ());
    std::vector<vec2f> texcoords (vertices.size ());

    for (size_t i = 0; i < vertices.size (); ++i) {
      positions[i] = get_position (vertices[i]);
      normals[i] = get_normal (vertices[i]);
      texcoords[i] = get_texcoord (vertices[i]);
    }

    return gen_tang_space (positions, normals, texcoords, triangle_indices,
                           out_tangents, angular_threshold);
  }

  /** Generate tangent space with default threshold from a vertex array. */
  template <typename Vertex, typename PosFn, typename NormFn, typename UVFn>
  bool
  gen_tang_space_default (const std::vector<Vertex> &vertices,
                          const std::vector<uint32_t> &triangle_indices,
                          std::vector<vec4f> &out_tangents, PosFn get_position,
                          NormFn get_normal, UVFn get_texcoord) const noexcept
  {
    return gen_tang_space (vertices, triangle_indices, out_tangents,
                           get_position, get_normal, get_texcoord, 180.0F);
  }
};

} // namespace wsl::math
