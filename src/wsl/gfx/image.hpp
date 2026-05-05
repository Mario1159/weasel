#pragma once

#include <SDL3/SDL_gpu.h>

namespace wsl
{

namespace gfx
{

struct image
{
  SDL_GPUTexture *texture = nullptr;
  SDL_GPUDevice *device = nullptr;
  SDL_GPUSampler *sampler = nullptr;

  image () = default;

  image (const image &) = delete;
  image &operator= (const image &) = delete;

  image (image &&other) noexcept
  {
    texture = other.texture;
    device = other.device;
    sampler = other.sampler;

    other.texture = nullptr;
    other.device = nullptr;
    other.sampler = nullptr;
  }

  image &
  operator= (image &&other) noexcept
  {
    if (this != &other) {
      release ();

      texture = other.texture;
      device = other.device;
      sampler = other.sampler;

      other.texture = nullptr;
      other.device = nullptr;
      other.sampler = nullptr;
    }
    return *this;
  }

  ~image () { release (); }

private:
  void
  release ()
  {
    if ((texture != nullptr) && (device != nullptr)) {
      SDL_ReleaseGPUTexture (device, texture);
    }
    if ((sampler != nullptr) && (device != nullptr)) {
      SDL_ReleaseGPUSampler (device, sampler);
    }
    texture = nullptr;
    device = nullptr;
    sampler = nullptr;
  }
};

} // namespace gfx

} // namespace wsl
