#pragma once

#ifndef IN_MODULE_INTERFACE
#include <SDL3/SDL_gpu.h>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstdint>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

#include "material.hpp"
#include "material_asset.hpp"

namespace wsl
{

namespace gfx
{

/** GPU texture wrapper used by renderer-side resources. */
struct texture
{
  SDL_GPUTexture *texture_data = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
};

/** Single vertex layout used by mesh primitives. */
struct vertex
{
  glm::vec3 pos{ 0.0F };
  glm::vec3 normal{ 0.0F };
  glm::vec2 uv{ 0.0F };
  /** xyz = tangent, w = sign used to reconstruct the bitangent. */
  glm::vec4 tangent{ 0.0F, 0.0F, 0.0F, 0.0F };
  /** Indices into the owning node's skin joint list. */
  glm::uvec4 joints{ 0U };
  /** Linear blend weights corresponding to joints. */
  glm::vec4 weights{ 0.0F };
};

/** Indexed primitive with a single material assignment. */
struct primitive
{
  uint32_t first_index = 0;
  std::vector<vertex> vertices;
  std::vector<uint32_t> indices;
  material mat;

  /** When true, the renderer uses custom_mat instead of the legacy mat. */
  bool use_custom_material = false;
  material_instance custom_mat;
};

/** Mesh containing one or more drawable primitives. */
struct mesh
{
  std::vector<primitive> primitives;
};

} // namespace gfx

} // namespace wsl
