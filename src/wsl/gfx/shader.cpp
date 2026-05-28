#include "shader.hpp"
#include "../rsc/resource_manager.hpp"
#include "../rsc/shader_loader.hpp"
#include "rsc/resource_ids.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <cstddef>


namespace wsl
{

namespace gfx
{

SDL_GPUShader *
shader::load_from_manager (SDL_GPUDevice *device, rsc::resource_manager *res_mgr,
                           rsc::shader_id id, SDL_GPUShaderStage stage,
                           Uint32 num_uniform_buffers, Uint32 num_samplers)
{
  if (res_mgr == nullptr) {
    return nullptr;
}

  auto handle = res_mgr->load (id);
  if (!handle) {
    return nullptr;
}

  SDL_GPUShaderCreateInfo info{};
  info.code = handle->bytecode.data ();
  info.code_size = static_cast<Uint32> (handle->bytecode.size ());
  info.stage = stage;
  info.entrypoint = "main";
  info.format = native_format ();

  info.num_uniform_buffers = num_uniform_buffers;
  info.num_samplers = num_samplers;

  info.num_storage_buffers = 0;
  info.num_storage_textures = 0;
  info.props = 0;

  return SDL_CreateGPUShader (device, &info);
}

SDL_GPUShader *
shader::load (SDL_GPUDevice *device, const char *path, SDL_GPUShaderStage stage,
              Uint32 num_uniform_buffers, Uint32 num_samplers)
{
  size_t size = 0;
  void *data = SDL_LoadFile (path, &size);
  if (data == nullptr) {
    SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, "LoadShader: failed to open %s",
                  path);
    return nullptr;
  }

  SDL_GPUShaderCreateInfo info{};
  info.code = static_cast<Uint8 *> (data);
  info.code_size = static_cast<Uint32> (size);
  info.stage = stage;
  info.entrypoint = "main";
  info.format = native_format ();

  // Use caller-provided reflection counts (required for IBL + other variants).
  info.num_uniform_buffers = num_uniform_buffers;
  info.num_samplers = num_samplers;

  info.num_storage_buffers = 0;
  info.num_storage_textures = 0;
  info.props = 0;

  SDL_GPUShader *shader = SDL_CreateGPUShader (device, &info);
  SDL_free (data);
  return shader;
}

SDL_GPUShader *
shader::load_ui_shader (SDL_GPUDevice *device, const char *path,
                        SDL_GPUShaderStage stage)
{
  size_t size = 0;
  void *data = SDL_LoadFile (path, &size);
  if (data == nullptr) {
    SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, "LoadShader: failed to open %s",
                  path);
    return nullptr;
  }

  SDL_GPUShaderCreateInfo info{};
  info.code = static_cast<Uint8 *> (data);
  info.code_size = static_cast<Uint32> (size);
  info.stage = stage;
  info.entrypoint = "main";
  info.format = native_format ();

  info.num_uniform_buffers = 1;
  info.num_samplers = 0;
  info.num_storage_textures = 0;
  info.num_storage_buffers = 0;
  info.props = 0;

  SDL_GPUShader *shader = SDL_CreateGPUShader (device, &info);
  SDL_free (data);
  return shader;
}

SDL_GPUShader *
shader::load_skybox_shader (SDL_GPUDevice *device, const char *path,
                            SDL_GPUShaderStage stage)
{
  size_t size = 0;
  void *data = SDL_LoadFile (path, &size);
  if (data == nullptr) {
    SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                  "LoadSkyboxShader: failed to open %s", path);
    return nullptr;
  }

  SDL_GPUShaderCreateInfo info{};
  info.code = static_cast<Uint8 *> (data);
  info.code_size = static_cast<Uint32> (size);
  info.stage = stage;
  info.entrypoint = "main";
  info.format = native_format ();

  if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
    info.num_uniform_buffers = 1; // Matrices
    info.num_samplers = 0;
  } else {
    info.num_uniform_buffers = 0;
    info.num_samplers = 1; // cubemap sampler
  }

  info.num_storage_textures = 0;
  info.num_storage_buffers = 0;
  info.props = 0;

  SDL_GPUShader *shader = SDL_CreateGPUShader (device, &info);
  SDL_free (data);
  return shader;
}

SDL_GPUShaderFormat
shader::native_format ()
{
#if defined(__APPLE__)
  return SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
  return SDL_GPU_SHADERFORMAT_DXIL;
#else
  return SDL_GPU_SHADERFORMAT_SPIRV;
#endif
}

} // namespace gfx

} // namespace wsl
