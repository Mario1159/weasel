#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace wsl
{

namespace gfx
{

/*!
 * \brief Describes a single member inside a uniform buffer.
 */
struct shader_buffer_member
{
  std::string name;
  uint32_t offset = 0;
  uint32_t size = 0;
};

/*!
 * \brief Reflection metadata for a single uniform buffer binding.
 */
struct shader_uniform_buffer
{
  std::string name;
  uint32_t binding = 0;
  uint32_t set = 0;
  uint32_t size = 0;
  std::vector<shader_buffer_member> members;
};

/*!
 * \brief Reflection metadata for a single texture/sampler binding.
 */
struct shader_texture_binding
{
  std::string name;
  uint32_t binding = 0;
  uint32_t set = 0;
  bool is_cube = false;
};

/*!
 * \brief Reflection metadata for a single storage buffer binding.
 */
struct shader_storage_buffer
{
  std::string name;
  uint32_t binding = 0;
  uint32_t set = 0;
  uint32_t size = 0;
};

/*!
 * \brief Reflection metadata extracted from a compiled shader module.
 *
 * Used to auto-layout material uniform blobs and validate binding counts.
 */
struct shader_reflection
{
  std::vector<shader_uniform_buffer> uniform_buffers;
  std::vector<shader_texture_binding> textures;
  std::vector<shader_storage_buffer> storage_buffers;

  //! Number of `SamplerState` bindings the shader declares (i.e. the size of
  //! its `u_Samplers[N]` array). SDL_GPU requires the graphics-pipeline shader
  //! to declare exactly this many samplers. Derived from codegen/slang
  //! reflection; falls back to textures.size() when unset (legacy path).
  uint32_t sampler_count = 0;

  uint32_t num_uniform_buffers () const;
  uint32_t num_samplers () const;
  uint32_t num_storage_buffers () const;

  /*!\brief Find a uniform buffer by name, or nullptr. */
  const shader_uniform_buffer *
  find_uniform_buffer (const std::string &name) const;
  /*!\brief Find a texture binding by name, or nullptr. */
  const shader_texture_binding *find_texture (const std::string &name) const;
};

/*!
 * \brief A complete shader program combining vertex and fragment stages.
 *
 * May also hold compute for future expansion.
 */
struct shader_program
{
  std::vector<uint8_t> vertex_bytecode;
  std::vector<uint8_t> fragment_bytecode;

  shader_reflection vertex_reflection;
  shader_reflection fragment_reflection;

  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_SPIRV;

  /*! \brief Release any compiled GPU handles held by this program. */
  void clear () noexcept;
};

} // namespace gfx

} // namespace wsl
