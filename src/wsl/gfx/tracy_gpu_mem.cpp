#include "tracy_gpu_mem.hpp"

#include <SDL3/SDL_gpu.h>
#include <tracy/Tracy.hpp>

namespace wsl::gfx
{

namespace
{

// Approximate bytes per pixel for the texture formats the engine
// actually creates. Falls back to 4 for unknown formats. Real byte
// widths are documented in the SDL3 GPU header.
size_t
bytes_per_pixel (SDL_GPUTextureFormat fmt)
{
  switch (fmt) {
  case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R8_SNORM:
    return 1;
  case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
  case SDL_GPU_TEXTUREFORMAT_R16_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R16_SNORM:
  case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
  case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
    return 2;
  case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
  case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R16G16_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R16G16_SNORM:
  case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
  case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
  case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
  case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
  case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
    return 4;
  case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
  case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM:
  case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
    return 8;
  default:
    return 4;
  }
}

// Compute the byte size of a texture with the given creation info.
// MSAA textures are sized for the un-resolved (sample_count) buffer
// because that's the allocation the driver makes.
uint64_t
texture_size_bytes (const SDL_GPUTextureCreateInfo &info)
{
  uint64_t w = info.width;
  uint64_t h = info.height;
  uint64_t layers
      = (info.type == SDL_GPU_TEXTURETYPE_3D) ? info.layer_count_or_depth : 1;
  uint64_t mips = info.num_levels == 0 ? 1 : info.num_levels;
  uint64_t samples = info.sample_count == 0 ? 1 : info.sample_count;
  uint64_t bpp = bytes_per_pixel (info.format);
  // Standard mip chain: each level is 1/4 the size of the previous.
  uint64_t mip_total = 0;
  uint64_t mw = w, mh = h;
  for (uint64_t i = 0; i < mips; ++i) {
    mip_total += mw * mh * bpp;
    mw = std::max (mw / 2, uint64_t (1));
    mh = std::max (mh / 2, uint64_t (1));
  }
  return mip_total * layers * samples;
}

} // namespace

void
tracy_alloc_texture (SDL_GPUTexture *tex, const SDL_GPUTextureCreateInfo &info)
{
  if (tex == nullptr) {
    return;
  }
  TracyAllocN (tex, texture_size_bytes (info), kTracyPoolTextures);
}

void
tracy_free_texture (SDL_GPUTexture *tex)
{
  if (tex == nullptr) {
    return;
  }
  TracyFreeN (tex, kTracyPoolTextures);
}

void
tracy_alloc_buffer (SDL_GPUBuffer *buf, uint64_t size_bytes)
{
  if (buf == nullptr) {
    return;
  }
  TracyAllocN (buf, size_bytes, kTracyPoolBuffers);
}

void
tracy_free_buffer (SDL_GPUBuffer *buf)
{
  if (buf == nullptr) {
    return;
  }
  TracyFreeN (buf, kTracyPoolBuffers);
}

void
tracy_alloc_transfer (SDL_GPUTransferBuffer *buf, size_t size_bytes)
{
  if (buf == nullptr) {
    return;
  }
  TracyAllocN (buf, size_bytes, kTracyPoolTransfer);
}

void
tracy_free_transfer (SDL_GPUTransferBuffer *buf)
{
  if (buf == nullptr) {
    return;
  }
  TracyFreeN (buf, kTracyPoolTransfer);
}

// Samplers / pipelines / shaders don't have a meaningful byte
// size, so we report 1 byte per object. Tracy's Memory tab uses
// the event count for the graph and the label just shows the
// total. Reporting 0 would still produce a valid event but the
// pool would show 0B which is confusing.
namespace
{
constexpr uint64_t kHandleSize = 1;
} // namespace

void
tracy_alloc_sampler (SDL_GPUSampler *samp)
{
  if (samp == nullptr) {
    return;
  }
  TracyAllocN (samp, kHandleSize, kTracyPoolSamplers);
}

void
tracy_free_sampler (SDL_GPUSampler *samp)
{
  if (samp == nullptr) {
    return;
  }
  TracyFreeN (samp, kTracyPoolSamplers);
}

void
tracy_alloc_pipeline (SDL_GPUGraphicsPipeline *pipe)
{
  if (pipe == nullptr) {
    return;
  }
  TracyAllocN (pipe, kHandleSize, kTracyPoolPipelines);
}

void
tracy_free_pipeline (SDL_GPUGraphicsPipeline *pipe)
{
  if (pipe == nullptr) {
    return;
  }
  TracyFreeN (pipe, kTracyPoolPipelines);
}

void
tracy_alloc_compute_pipeline (SDL_GPUComputePipeline *pipe)
{
  if (pipe == nullptr) {
    return;
  }
  TracyAllocN (pipe, kHandleSize, kTracyPoolPipelines);
}

void
tracy_free_compute_pipeline (SDL_GPUComputePipeline *pipe)
{
  if (pipe == nullptr) {
    return;
  }
  TracyFreeN (pipe, kTracyPoolPipelines);
}

void
tracy_alloc_shader (SDL_GPUShader *shader)
{
  if (shader == nullptr) {
    return;
  }
  TracyAllocN (shader, kHandleSize, kTracyPoolShaders);
}

void
tracy_free_shader (SDL_GPUShader *shader)
{
  if (shader == nullptr) {
    return;
  }
  TracyFreeN (shader, kTracyPoolShaders);
}

void
tracy_alloc_imgui (void *ptr, size_t size_bytes)
{
  if (ptr == nullptr) {
    return;
  }
  TracyAllocN (ptr, size_bytes, kTracyPoolImGui);
}

void
tracy_free_imgui (void *ptr)
{
  if (ptr == nullptr) {
    return;
  }
  TracyFreeN (ptr, kTracyPoolImGui);
}

void
tracy_alloc_cluster (void *ptr, size_t size_bytes)
{
  if (ptr == nullptr) {
    return;
  }
  TracyAllocN (ptr, size_bytes, kTracyPoolCluster);
}

void
tracy_free_cluster (void *ptr)
{
  if (ptr == nullptr) {
    return;
  }
  TracyFreeN (ptr, kTracyPoolCluster);
}

// --------------------------------------------------------------------
// SDL_GPU wrappers.
//
// Allocation pattern: call SDL, then tracy_alloc_* if the handle
// is non-null. The texture / buffer sizes come from the create
// info, so the Tracy pool sees a meaningful byte count. Sampler /
// pipeline / shader handles are reported as 1 byte each (see the
// tracy_alloc_* documentation above for why).
//
// Free pattern: tracy_free_* first (idempotent for null), then
// SDL_Release*. Order matters: if SDL_Release* throws or aborts
// the process, the Tracy side is still consistent.
// --------------------------------------------------------------------

// The create_* / release_* function wrappers used to live here.
// They've been replaced by the RAII classes in gpu_resources.hpp
// (gpu_texture, gpu_buffer, gpu_transfer_buffer, gpu_sampler,
// gpu_graphics_pipeline, gpu_compute_pipeline, gpu_shader). Each
// class constructor calls SDL_Create* + the matching tracy_alloc_*,
// and the destructor calls tracy_free_* + SDL_Release*. Use those
// instead of bare SDL handles.

SDL_GPUTexture *
create_gpu_texture (SDL_GPUDevice *device, const SDL_GPUTextureCreateInfo *info)
{
  SDL_GPUTexture *tex = SDL_CreateGPUTexture (device, info);
  tracy_alloc_texture (tex, *info);
  return tex;
}

void
release_gpu_texture (SDL_GPUDevice *device, SDL_GPUTexture *tex)
{
  tracy_free_texture (tex);
  if (tex != nullptr) {
    SDL_ReleaseGPUTexture (device, tex);
  }
}

SDL_GPUBuffer *
create_gpu_buffer (SDL_GPUDevice *device, const SDL_GPUBufferCreateInfo *info)
{
  SDL_GPUBuffer *buf = SDL_CreateGPUBuffer (device, info);
  tracy_alloc_buffer (buf, info->size);
  return buf;
}

void
release_gpu_buffer (SDL_GPUDevice *device, SDL_GPUBuffer *buf)
{
  tracy_free_buffer (buf);
  if (buf != nullptr) {
    SDL_ReleaseGPUBuffer (device, buf);
  }
}

SDL_GPUTransferBuffer *
create_gpu_transfer_buffer (SDL_GPUDevice *device,
                            const SDL_GPUTransferBufferCreateInfo *info)
{
  SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer (device, info);
  tracy_alloc_transfer (tb, info->size);
  return tb;
}

void
release_gpu_transfer_buffer (SDL_GPUDevice *device, SDL_GPUTransferBuffer *tb)
{
  tracy_free_transfer (tb);
  if (tb != nullptr) {
    SDL_ReleaseGPUTransferBuffer (device, tb);
  }
}

SDL_GPUSampler *
create_gpu_sampler (SDL_GPUDevice *device, const SDL_GPUSamplerCreateInfo *info)
{
  SDL_GPUSampler *samp = SDL_CreateGPUSampler (device, info);
  tracy_alloc_sampler (samp);
  return samp;
}

void
release_gpu_sampler (SDL_GPUDevice *device, SDL_GPUSampler *samp)
{
  tracy_free_sampler (samp);
  if (samp != nullptr) {
    SDL_ReleaseGPUSampler (device, samp);
  }
}

SDL_GPUGraphicsPipeline *
create_gpu_graphics_pipeline (SDL_GPUDevice *device,
                              const SDL_GPUGraphicsPipelineCreateInfo *info)
{
  SDL_GPUGraphicsPipeline *pipe = SDL_CreateGPUGraphicsPipeline (device, info);
  tracy_alloc_pipeline (pipe);
  return pipe;
}

void
release_gpu_graphics_pipeline (SDL_GPUDevice *device,
                               SDL_GPUGraphicsPipeline *pipe)
{
  tracy_free_pipeline (pipe);
  if (pipe != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline (device, pipe);
  }
}

SDL_GPUComputePipeline *
create_gpu_compute_pipeline (SDL_GPUDevice *device,
                             const SDL_GPUComputePipelineCreateInfo *info)
{
  SDL_GPUComputePipeline *pipe = SDL_CreateGPUComputePipeline (device, info);
  tracy_alloc_compute_pipeline (pipe);
  return pipe;
}

void
release_gpu_compute_pipeline (SDL_GPUDevice *device,
                              SDL_GPUComputePipeline *pipe)
{
  tracy_free_compute_pipeline (pipe);
  if (pipe != nullptr) {
    SDL_ReleaseGPUComputePipeline (device, pipe);
  }
}

SDL_GPUShader *
create_gpu_shader (SDL_GPUDevice *device, const SDL_GPUShaderCreateInfo *info)
{
  SDL_GPUShader *shader = SDL_CreateGPUShader (device, info);
  tracy_alloc_shader (shader);
  return shader;
}

void
release_gpu_shader (SDL_GPUDevice *device, SDL_GPUShader *shader)
{
  tracy_free_shader (shader);
  if (shader != nullptr) {
    SDL_ReleaseGPUShader (device, shader);
  }
}

} // namespace wsl::gfx
