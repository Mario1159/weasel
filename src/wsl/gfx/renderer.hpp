#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_pixels.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>


namespace wsl
{

namespace gfx
{

/*!
 * \brief Shared lighting uniform layout used by renderer shaders.
 */
struct lighting_ubo
{
  glm::vec3 light_pos;
  float pad1;
  glm::vec3 camera_pos;
  float pad2;
  glm::vec3 ambient;
  float pad3;
  glm::vec3 diffuse;
  float pad4;
  glm::vec3 specular;
  float shininess;
};

/*!
 * \brief Base renderer marker type.
 */
class renderer
{
};

} // namespace gfx

} // namespace wsl
