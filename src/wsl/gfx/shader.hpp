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

/** Loads SDL GPU shaders from disk. */
class shader
{
public:
  /** Loads a shader with explicit resource binding counts. */
  static SDL_GPUShader *load (SDL_GPUDevice *device, const char *path,
                              SDL_GPUShaderStage stage,
                              Uint32 num_uniform_buffers, Uint32 num_samplers,
                              Uint32 num_storage_buffers = 0);

  static SDL_GPUShader *
  load_from_manager (SDL_GPUDevice *device, rsc::resource_manager *res_mgr,
                     rsc::shader_id id, SDL_GPUShaderStage stage,
                     Uint32 num_uniform_buffers, Uint32 num_samplers,
                     Uint32 num_storage_buffers = 0);

  /** Loads a shader configured for UI rendering. */
  static SDL_GPUShader *load_ui_shader (SDL_GPUDevice *device, const char *path,
                                        SDL_GPUShaderStage stage);

  /** Loads a shader configured for skybox rendering. */
  static SDL_GPUShader *load_skybox_shader (SDL_GPUDevice *device,
                                            const char *path,
                                            SDL_GPUShaderStage stage);

  /**
 * Creates a shader from an in-memory bytecode blob.
 *
 * Used for runtime-compiled shaders (e.g. from the shader graph).
 */
  static SDL_GPUShader *
  create_from_bytecode (SDL_GPUDevice *device, const uint8_t *data, size_t size,
                        SDL_GPUShaderStage stage, Uint32 num_uniform_buffers,
                        Uint32 num_samplers, Uint32 num_storage_buffers = 0);

  /**
 * Returns the native shader format for the current platform.
 *
 * Matches the shader bytecode format that the build system compiles to
 * (SPIR-V on Linux, MSL on macOS, DXIL on Windows).
 */
  static SDL_GPUShaderFormat native_format ();
};

} // namespace gfx

} // namespace wsl
