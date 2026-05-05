#pragma once
#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <cstdint>


namespace wsl
{

namespace gfx
{

struct cubemap
{
  SDL_GPUTexture *texture = nullptr;
  SDL_GPUSampler *sampler = nullptr;

  SDL_GPUTexture *ibl_irradiance = nullptr;
  SDL_GPUTexture *ibl_prefilter = nullptr;
  SDL_GPUTexture *ibl_brdf_lut = nullptr;

  SDL_GPUSampler *ibl_sampler = nullptr;

  float prefilter_max_mip = 0.0F;
  uint32_t prefilter_mip_count = 1;

  SDL_GPUDevice *device = nullptr;
  SDL_GPUTexture *equirect_to_bake = nullptr;

  cubemap () = default;
  ~cubemap ();

  // Non-copyable (raw GPU handles)
  cubemap (const cubemap &) = delete;
  cubemap &operator= (const cubemap &) = delete;

  // Move-safe: steal handles
  cubemap (cubemap &&other) noexcept { *this = std::move (other); }
  cubemap &
  operator= (cubemap &&other) noexcept
  {
    if (this == &other) { { { { { { { { { {
      return *this;
}
}
}
}
}
}
}
}
}
}

    // release current
    this->~cubemap ();

    // steal
    texture = other.texture;
    other.texture = nullptr;
    sampler = other.sampler;
    other.sampler = nullptr;

    ibl_irradiance = other.ibl_irradiance;
    other.ibl_irradiance = nullptr;
    ibl_prefilter = other.ibl_prefilter;
    other.ibl_prefilter = nullptr;
    ibl_brdf_lut = other.ibl_brdf_lut;
    other.ibl_brdf_lut = nullptr;

    ibl_sampler = other.ibl_sampler;
    other.ibl_sampler = nullptr;

    prefilter_max_mip = other.prefilter_max_mip;
    prefilter_mip_count = other.prefilter_mip_count;

    equirect_to_bake = other.equirect_to_bake;
    other.equirect_to_bake = nullptr;

    device = other.device;
    other.device = nullptr;

    return *this;
  }
};

} // namespace gfx

} // namespace wsl
