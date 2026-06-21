#pragma once

#include <SDL3/SDL_gpu.h>
#include "../rsc/resource_ids.hpp"

namespace wsl
{

namespace rsc
{
class resource_manager;
}

namespace gfx
{

/*!
 * \brief Loads SDL GPU shaders from disk.
 */
class shader
{
public:
  /*!
   * \brief Loads a shader with explicit resource binding counts.
   */
  static SDL_GPUShader *load (SDL_GPUDevice *device, const char *path,
                              SDL_GPUShaderStage stage,
                              Uint32 num_uniform_buffers, Uint32 num_samplers,
                              Uint32 num_storage_buffers = 0);

  static SDL_GPUShader *
  load_from_manager (SDL_GPUDevice *device, rsc::resource_manager *res_mgr,
                     rsc::shader_id id, SDL_GPUShaderStage stage,
                     Uint32 num_uniform_buffers, Uint32 num_samplers,
                     Uint32 num_storage_buffers = 0);

  /*!
   * \brief Loads a shader configured for UI rendering.
   */
  static SDL_GPUShader *load_ui_shader (SDL_GPUDevice *device, const char *path,
                                        SDL_GPUShaderStage stage);

  /*!
   * \brief Loads a shader configured for skybox rendering.
   */
  static SDL_GPUShader *load_skybox_shader (SDL_GPUDevice *device,
                                            const char *path,
                                            SDL_GPUShaderStage stage);

  /*!
   * \brief Returns the native shader format for the current platform.
   *
   * Matches the shader bytecode format that the build system compiles to
   * (SPIR-V on Linux, MSL on macOS, DXIL on Windows).
   */
  static SDL_GPUShaderFormat native_format ();
};

} // namespace gfx

} // namespace wsl
