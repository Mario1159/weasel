#include "subviewport_target.hpp"
#include "render_window.hpp"
#include "render_context.hpp"
#include "wsl/log/log.hpp"
#include "tracy_gpu_mem.hpp"

#include <SDL3/SDL_gpu.h>

namespace wsl::gfx
{

subviewport_target
subviewport_target::create (render_window *window, render_context *ctx,
                            uint32_t w, uint32_t h)
{
  subviewport_target target{};
  target.width = w;
  target.height = h;

  if (w == 0 || h == 0 || window == nullptr || ctx == nullptr) {
    return target;
  }

  SDL_GPUDevice *device = ctx->gpu_device;

  auto create_tex
      = [&] (const SDL_GPUTextureCreateInfo &ci) -> SDL_GPUTexture * {
    SDL_GPUTexture *tex = SDL_CreateGPUTexture (device, &ci);
    if (tex == nullptr) {
      wsl::log::gfx ()->error (
          "Failed to create subviewport texture ({}x{}): {}", ci.width,
          ci.height, SDL_GetError ());
      return nullptr;
    }
    wsl::gfx::tracy_alloc_texture (tex, ci);
    return tex;
  };

  // MSAA HDR scene target (matches window msaa_hdr_scene)
  SDL_GPUTextureCreateInfo msaa{};
  SDL_zero (msaa);
  msaa.type = SDL_GPU_TEXTURETYPE_2D;
  msaa.width = w;
  msaa.height = h;
  msaa.layer_count_or_depth = 1;
  msaa.num_levels = 1;
  msaa.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  msaa.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  msaa.sample_count = SDL_GPU_SAMPLECOUNT_4;

  target.color_msaa = gpu_texture::adopt (device, create_tex (msaa));
  target.bloom_msaa = gpu_texture::adopt (device, create_tex (msaa));

  // Resolved HDR targets (sampler + color target for resolve dest)
  SDL_GPUTextureCreateInfo res = msaa;
  res.sample_count = SDL_GPU_SAMPLECOUNT_1;
  res.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  target.color_resolve = gpu_texture::adopt (device, create_tex (res));
  target.bloom_resolve = gpu_texture::adopt (device, create_tex (res));

  // Depth target (matches window depth_texture)
  SDL_GPUTextureCreateInfo ds{};
  SDL_zero (ds);
  ds.type = SDL_GPU_TEXTURETYPE_2D;
  ds.width = w;
  ds.height = h;
  ds.layer_count_or_depth = 1;
  ds.num_levels = 1;
  ds.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  ds.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  ds.sample_count = SDL_GPU_SAMPLECOUNT_4;

  target.depth = gpu_texture::adopt (device, create_tex (ds));

  return target;
}

void
subviewport_target::destroy (subviewport_target &target)
{
  target.color_msaa.reset ();
  target.color_resolve.reset ();
  target.bloom_msaa.reset ();
  target.bloom_resolve.reset ();
  target.depth.reset ();
  target.width = 0;
  target.height = 0;
}

} // namespace wsl::gfx
