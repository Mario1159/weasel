#pragma once

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

#include "material.hpp"


namespace wsl
{

namespace gfx
{

/*!
 * \brief GPU texture wrapper used by renderer-side resources.
 */
struct texture
{
  SDL_GPUTexture *texture_data = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
};

/*!
 * \brief Single vertex layout used by mesh primitives.
 */
struct vertex
{
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;
  //! xyz = tangent, w = sign used to reconstruct the bitangent.
  glm::vec4 tangent;
};

/*!
 * \brief Indexed primitive with a single material assignment.
 */
struct primitive
{
  uint32_t first_index = 0;
  std::vector<vertex> vertices;
  std::vector<uint32_t> indices;
  material mat;
};

/*!
 * \brief Mesh containing one or more drawable primitives.
 */
struct mesh
{
  std::vector<primitive> primitives;
};

} // namespace gfx

} // namespace wsl
