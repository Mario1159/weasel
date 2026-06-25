#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstddef>
#include <cstdint>

namespace wsl::gfx
{

// Named memory pools used by Tracy. String literals are pooled
// (the same address must be used for the matching TracyFreeN), so
// they live as `static constexpr` here and are passed by pointer.
//
// The full pool taxonomy in use by the engine:
//
//   wsl.gfx.textures   — all SDL_GPUTexture allocations
//                        (HDR scene/bloom, depth, present, cubemaps,
//                        IBL irradiance/prefilter/brdf, msaa targets)
//   wsl.gfx.buffers    — SDL_GPUBuffer (vertex, index, uniform,
//                        storage). Size is given explicitly at alloc
//                        time because SDL_GPUBuffer doesn't carry a
//                        sidecar size query.
//   wsl.gfx.transfer   — SDL_GPUTransferBuffer (CPU→GPU staging
//                        memory used for copies and for the Tracy
//                        frame-image readback path)
//   wsl.gfx.samplers   — SDL_GPUSampler (linear/nearest/IBL)
//   wsl.gfx.pipelines  — SDL_GPUGraphicsPipeline / SDL_GPUComputePipeline
//                        (the compiled PSO state, including the
//                        editor's debug-renderer pipelines)
//   wsl.gfx.shaders    — SDL_GPUShader (the compiled bytecode that
//                        gets handed to pipeline creation)
//   wsl.gfx.imgui      — ImGui draw-data staging buffers (VBO/IBO
//                        for the ImGui renderer_interface path)
//   wsl.gfx.cluster    — clustered_lighting CPU-side data
//                        (light grid + index lists, see
//                        clustered_lighting.cpp)
inline constexpr const char *kTracyPoolTextures = "wsl.gfx.textures";
inline constexpr const char *kTracyPoolBuffers = "wsl.gfx.buffers";
inline constexpr const char *kTracyPoolTransfer = "wsl.gfx.transfer";
inline constexpr const char *kTracyPoolSamplers = "wsl.gfx.samplers";
inline constexpr const char *kTracyPoolPipelines = "wsl.gfx.pipelines";
inline constexpr const char *kTracyPoolShaders = "wsl.gfx.shaders";
inline constexpr const char *kTracyPoolImGui = "wsl.gfx.imgui";
inline constexpr const char *kTracyPoolCluster = "wsl.gfx.cluster";

// Report a freshly-created GPU texture to Tracy. The pool name is
// a string literal whose address will be reused by the matching
// `tracy_free_*` call, so call this once per creation and call the
// matching free exactly once on destruction.
void tracy_alloc_texture (SDL_GPUTexture *tex,
                          const SDL_GPUTextureCreateInfo &info);
void tracy_free_texture (SDL_GPUTexture *tex);

// GPU buffers. The size is explicit because SDL_GPUBuffer doesn't
// carry a sidecar query for its footprint.
void tracy_alloc_buffer (SDL_GPUBuffer *buf, uint64_t size_bytes);
void tracy_free_buffer (SDL_GPUBuffer *buf);

// Transfer buffers (staging memory used for CPU -> GPU copies).
void tracy_alloc_transfer (SDL_GPUTransferBuffer *buf, size_t size_bytes);
void tracy_free_transfer (SDL_GPUTransferBuffer *buf);

// Samplers, pipelines, shaders. These don't have a meaningful
// "size" in the byte sense, so we report a flat 1 byte each. Tracy
// uses the count of events in a pool for the memory-usage graph;
// the byte size only matters for the total-usage label, where 1
// per object gives a readable number (you'll see "X samplers" in
// the tooltip rather than "X bytes").
void tracy_alloc_sampler (SDL_GPUSampler *samp);
void tracy_free_sampler (SDL_GPUSampler *samp);

void tracy_alloc_pipeline (SDL_GPUGraphicsPipeline *pipe);
void tracy_free_pipeline (SDL_GPUGraphicsPipeline *pipe);

void tracy_alloc_compute_pipeline (SDL_GPUComputePipeline *pipe);
void tracy_free_compute_pipeline (SDL_GPUComputePipeline *pipe);

void tracy_alloc_shader (SDL_GPUShader *shader);
void tracy_free_shader (SDL_GPUShader *shader);

// ImGui staging buffers (draw-data VBO/IBO owned by the ImGui
// renderer interface).
void tracy_alloc_imgui (void *ptr, size_t size_bytes);
void tracy_free_imgui (void *ptr);

// Clustered lighting CPU-side data (light grid + index lists).
void tracy_alloc_cluster (void *ptr, size_t size_bytes);
void tracy_free_cluster (void *ptr);

// --------------------------------------------------------------------
// Drop-in SDL_GPU wrappers.
//
// These call the SDL create / release function and forward the
// result to the matching Tracy memory pool, so every call site
// gets instrumented without any extra tracy_alloc_* line. The
// SDL signature is preserved 1:1 (pointer arg, return value) so a
// site can be refactored by just changing the function name.
//
// Pair every create_* with the matching release_* — both wrapper
// functions do the Tracy side first, so the pool stays consistent
// even if SDL ever returns a non-null handle that we then free
// again later (the TracyFreeN is idempotent for the same pointer).
//
// The RAII classes in gpu_resources.hpp (gpu_texture, gpu_buffer,
// etc.) are also available — they do the same thing but the
// tracking is automatic on construction / destruction. Use those
// for new code that wants the lifetime guaranteed by the type
// system; use the function wrappers when working with raw
// SDL_GPUTexture* members that other code consumes.
// --------------------------------------------------------------------
SDL_GPUTexture *create_gpu_texture (SDL_GPUDevice *device,
                                    const SDL_GPUTextureCreateInfo *info);
void release_gpu_texture (SDL_GPUDevice *device, SDL_GPUTexture *tex);

SDL_GPUBuffer *create_gpu_buffer (SDL_GPUDevice *device,
                                  const SDL_GPUBufferCreateInfo *info);
void release_gpu_buffer (SDL_GPUDevice *device, SDL_GPUBuffer *buf);

SDL_GPUTransferBuffer *
create_gpu_transfer_buffer (SDL_GPUDevice *device,
                            const SDL_GPUTransferBufferCreateInfo *info);
void release_gpu_transfer_buffer (SDL_GPUDevice *device,
                                  SDL_GPUTransferBuffer *buf);

SDL_GPUSampler *create_gpu_sampler (SDL_GPUDevice *device,
                                    const SDL_GPUSamplerCreateInfo *info);
void release_gpu_sampler (SDL_GPUDevice *device, SDL_GPUSampler *samp);

SDL_GPUGraphicsPipeline *
create_gpu_graphics_pipeline (SDL_GPUDevice *device,
                              const SDL_GPUGraphicsPipelineCreateInfo *info);
void release_gpu_graphics_pipeline (SDL_GPUDevice *device,
                                    SDL_GPUGraphicsPipeline *pipe);

SDL_GPUComputePipeline *
create_gpu_compute_pipeline (SDL_GPUDevice *device,
                             const SDL_GPUComputePipelineCreateInfo *info);
void release_gpu_compute_pipeline (SDL_GPUDevice *device,
                                   SDL_GPUComputePipeline *pipe);

SDL_GPUShader *create_gpu_shader (SDL_GPUDevice *device,
                                  const SDL_GPUShaderCreateInfo *info);
void release_gpu_shader (SDL_GPUDevice *device, SDL_GPUShader *shader);

} // namespace wsl::gfx
