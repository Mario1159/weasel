#pragma once

#include <SDL3/SDL_gpu.h>

#include <utility>

namespace wsl::gfx
{

// --------------------------------------------------------------------
// RAII wrappers around SDL_GPU resources.
//
// Each wrapper:
//   * Calls the matching SDL_Create* in its constructor and reports
//     the allocation to Tracy's wsl.gfx.<kind> memory pool.
//   * Calls the matching SDL_Release* in its destructor and reports
//     the free to Tracy.
//   * Is move-only (RAII handle must not be silently duplicated).
//   * Provides .get() to pass the raw handle to SDL APIs that
//     require it (every SDL_GPU call still uses the raw C handle).
//   * Exposes reset() to release early / replace the handle.
//
// Why RAII over the previous function wrappers:
//   * Lifetime is provably tied to scope. No risk of a forgotten
//     SDL_Release* on an error path, and no risk of a Tracy pool
//     staying inflated after a leaked SDL resource (or vice versa).
//   * `m_hdr_scene = gpu_texture{...}` is one line that gives you
//     a member variable that's automatically released when the
//     owning object is destroyed, with Tracy tracking for free.
//   * Move semantics let the class be returned from factory
//     functions (e.g. cubemap_loader's per-face upload helper).
//
// The .get() design is intentional over an implicit conversion to
// SDL_GPUTexture* — it makes ownership intent obvious at the call
// site (`tex.get()` vs `&tex`) and prevents accidentally passing
// the wrapper by value.
// --------------------------------------------------------------------

#define WSL_GPU_RAII_CLASS(NAME, HANDLE, RELEASE_FN)                           \
  class NAME                                                                   \
  {                                                                            \
  public:                                                                      \
    NAME () noexcept = default;                                                \
    NAME (SDL_GPUDevice *device, const HANDLE##CreateInfo &info);              \
    ~NAME ();                                                                  \
                                                                               \
    NAME (const NAME &) = delete;                                              \
    NAME &operator= (const NAME &) = delete;                                   \
    NAME (NAME &&other) noexcept;                                              \
    NAME &operator= (NAME &&other) noexcept;                                   \
                                                                               \
    /* Take ownership of a pre-created raw handle. The Tracy      */           \
    /* allocation event is NOT reported here (the creator is     */            \
    /* assumed to have already reported it, or the handle is a    */           \
    /* non-owning reference like a swapchain image). Use the      */           \
    /* (device, info) constructor for handles that should be      */           \
    /* tracked in Tracy's memory pool. */                                      \
    static NAME adopt (SDL_GPUDevice *device, HANDLE *handle) noexcept;        \
                                                                               \
    HANDLE *                                                                   \
    get () const noexcept                                                      \
    {                                                                          \
      return m_handle;                                                         \
    }                                                                          \
    explicit                                                                   \
    operator bool () const noexcept                                            \
    {                                                                          \
      return m_handle != nullptr;                                              \
    }                                                                          \
                                                                               \
    void reset () noexcept;                                                    \
                                                                               \
  private:                                                                     \
    SDL_GPUDevice *m_device = nullptr;                                         \
    HANDLE *m_handle = nullptr;                                                \
  }

// SDL_GPUTexture
WSL_GPU_RAII_CLASS (gpu_texture, SDL_GPUTexture, SDL_ReleaseGPUTexture);

// SDL_GPUBuffer
WSL_GPU_RAII_CLASS (gpu_buffer, SDL_GPUBuffer, SDL_ReleaseGPUBuffer);

// SDL_GPUTransferBuffer
WSL_GPU_RAII_CLASS (gpu_transfer_buffer, SDL_GPUTransferBuffer,
                    SDL_ReleaseGPUTransferBuffer);

// SDL_GPUSampler
WSL_GPU_RAII_CLASS (gpu_sampler, SDL_GPUSampler, SDL_ReleaseGPUSampler);

// SDL_GPUGraphicsPipeline
WSL_GPU_RAII_CLASS (gpu_graphics_pipeline, SDL_GPUGraphicsPipeline,
                    SDL_ReleaseGPUGraphicsPipeline);

// SDL_GPUComputePipeline
WSL_GPU_RAII_CLASS (gpu_compute_pipeline, SDL_GPUComputePipeline,
                    SDL_ReleaseGPUComputePipeline);

// SDL_GPUShader
WSL_GPU_RAII_CLASS (gpu_shader, SDL_GPUShader, SDL_ReleaseGPUShader);

#undef WSL_GPU_RAII_CLASS

} // namespace wsl::gfx
