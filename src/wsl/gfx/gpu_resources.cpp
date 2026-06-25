#include "gpu_resources.hpp"

#include "tracy_gpu_mem.hpp"

#include <tracy/Tracy.hpp>

namespace wsl::gfx
{

// All four operations per type follow the same pattern. The
// constructor calls SDL_Create*, then tracy_alloc_* if the handle
// is non-null. The destructor and reset() call tracy_free_* first
// (idempotent for null), then SDL_Release* only if non-null. Move
// ops just transfer the (device, handle) pair and zero the source
// so it won't release on its own.

// --------------------------------------------------------------------
// gpu_texture
// --------------------------------------------------------------------

gpu_texture::gpu_texture (SDL_GPUDevice *device,
                          const SDL_GPUTextureCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUTexture (device, &info);
  tracy_alloc_texture (m_handle, info);
}

gpu_texture::~gpu_texture () { reset (); }

gpu_texture
gpu_texture::adopt (SDL_GPUDevice *device, SDL_GPUTexture *handle) noexcept
{
  gpu_texture t;
  t.m_device = device;
  t.m_handle = handle;
  return t;
}

gpu_texture::gpu_texture (gpu_texture &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_texture &
gpu_texture::operator= (gpu_texture &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_texture::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_texture (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUTexture (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_buffer
// --------------------------------------------------------------------

gpu_buffer::gpu_buffer (SDL_GPUDevice *device,
                        const SDL_GPUBufferCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUBuffer (device, &info);
  tracy_alloc_buffer (m_handle, info.size);
}

gpu_buffer::~gpu_buffer () { reset (); }

gpu_buffer
gpu_buffer::adopt (SDL_GPUDevice *device, SDL_GPUBuffer *handle) noexcept
{
  gpu_buffer b;
  b.m_device = device;
  b.m_handle = handle;
  return b;
}

gpu_buffer::gpu_buffer (gpu_buffer &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_buffer &
gpu_buffer::operator= (gpu_buffer &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_buffer::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_buffer (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUBuffer (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_transfer_buffer
// --------------------------------------------------------------------

gpu_transfer_buffer::gpu_transfer_buffer (
    SDL_GPUDevice *device, const SDL_GPUTransferBufferCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUTransferBuffer (device, &info);
  tracy_alloc_transfer (m_handle, info.size);
}

gpu_transfer_buffer::~gpu_transfer_buffer () { reset (); }

gpu_transfer_buffer
gpu_transfer_buffer::adopt (SDL_GPUDevice *device,
                            SDL_GPUTransferBuffer *handle) noexcept
{
  gpu_transfer_buffer b;
  b.m_device = device;
  b.m_handle = handle;
  return b;
}

gpu_transfer_buffer::gpu_transfer_buffer (gpu_transfer_buffer &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_transfer_buffer &
gpu_transfer_buffer::operator= (gpu_transfer_buffer &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_transfer_buffer::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_transfer (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUTransferBuffer (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_sampler
// --------------------------------------------------------------------

gpu_sampler::gpu_sampler (SDL_GPUDevice *device,
                          const SDL_GPUSamplerCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUSampler (device, &info);
  tracy_alloc_sampler (m_handle);
}

gpu_sampler::~gpu_sampler () { reset (); }

gpu_sampler
gpu_sampler::adopt (SDL_GPUDevice *device, SDL_GPUSampler *handle) noexcept
{
  gpu_sampler s;
  s.m_device = device;
  s.m_handle = handle;
  return s;
}

gpu_sampler::gpu_sampler (gpu_sampler &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_sampler &
gpu_sampler::operator= (gpu_sampler &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_sampler::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_sampler (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUSampler (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_graphics_pipeline
// --------------------------------------------------------------------

gpu_graphics_pipeline::gpu_graphics_pipeline (
    SDL_GPUDevice *device, const SDL_GPUGraphicsPipelineCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUGraphicsPipeline (device, &info);
  tracy_alloc_pipeline (m_handle);
}

gpu_graphics_pipeline::~gpu_graphics_pipeline () { reset (); }

gpu_graphics_pipeline
gpu_graphics_pipeline::adopt (SDL_GPUDevice *device,
                              SDL_GPUGraphicsPipeline *handle) noexcept
{
  gpu_graphics_pipeline p;
  p.m_device = device;
  p.m_handle = handle;
  return p;
}

gpu_graphics_pipeline::gpu_graphics_pipeline (
    gpu_graphics_pipeline &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_graphics_pipeline &
gpu_graphics_pipeline::operator= (gpu_graphics_pipeline &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_graphics_pipeline::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_pipeline (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_compute_pipeline
// --------------------------------------------------------------------

gpu_compute_pipeline::gpu_compute_pipeline (
    SDL_GPUDevice *device, const SDL_GPUComputePipelineCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUComputePipeline (device, &info);
  tracy_alloc_compute_pipeline (m_handle);
}

gpu_compute_pipeline::~gpu_compute_pipeline () { reset (); }

gpu_compute_pipeline
gpu_compute_pipeline::adopt (SDL_GPUDevice *device,
                             SDL_GPUComputePipeline *handle) noexcept
{
  gpu_compute_pipeline p;
  p.m_device = device;
  p.m_handle = handle;
  return p;
}

gpu_compute_pipeline::gpu_compute_pipeline (
    gpu_compute_pipeline &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_compute_pipeline &
gpu_compute_pipeline::operator= (gpu_compute_pipeline &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_compute_pipeline::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_compute_pipeline (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUComputePipeline (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

// --------------------------------------------------------------------
// gpu_shader
// --------------------------------------------------------------------

gpu_shader::gpu_shader (SDL_GPUDevice *device,
                        const SDL_GPUShaderCreateInfo &info)
    : m_device (device)
{
  m_handle = SDL_CreateGPUShader (device, &info);
  tracy_alloc_shader (m_handle);
}

gpu_shader::~gpu_shader () { reset (); }

gpu_shader
gpu_shader::adopt (SDL_GPUDevice *device, SDL_GPUShader *handle) noexcept
{
  gpu_shader s;
  s.m_device = device;
  s.m_handle = handle;
  return s;
}

gpu_shader::gpu_shader (gpu_shader &&other) noexcept
    : m_device (other.m_device), m_handle (other.m_handle)
{
  other.m_device = nullptr;
  other.m_handle = nullptr;
}

gpu_shader &
gpu_shader::operator= (gpu_shader &&other) noexcept
{
  if (this != &other) {
    reset ();
    m_device = other.m_device;
    m_handle = other.m_handle;
    other.m_device = nullptr;
    other.m_handle = nullptr;
  }
  return *this;
}

void
gpu_shader::reset () noexcept
{
  if (m_handle != nullptr) {
    tracy_free_shader (m_handle);
    if (m_device != nullptr) {
      SDL_ReleaseGPUShader (m_device, m_handle);
    }
    m_handle = nullptr;
  }
}

} // namespace wsl::gfx
