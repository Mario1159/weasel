module;
#include <SDL3/SDL_gpu.h>

module wsl.core;

namespace wsl
{

namespace gfx
{

cubemap::~cubemap ()
{
  if (device == nullptr) {
    return;
  }

  if (ibl_sampler != nullptr) {
    SDL_ReleaseGPUSampler (device, ibl_sampler);
  }
  if (ibl_brdf_lut != nullptr) {
    SDL_ReleaseGPUTexture (device, ibl_brdf_lut);
  }
  if (ibl_prefilter != nullptr) {
    SDL_ReleaseGPUTexture (device, ibl_prefilter);
  }
  if (ibl_irradiance != nullptr) {
    SDL_ReleaseGPUTexture (device, ibl_irradiance);
  }

  if (sampler != nullptr) {
    SDL_ReleaseGPUSampler (device, sampler);
  }
  if (texture != nullptr) {
    SDL_ReleaseGPUTexture (device, texture);
  }
  if (equirect_to_bake != nullptr) {
    SDL_ReleaseGPUTexture (device, equirect_to_bake);
  }

  ibl_sampler = nullptr;
  ibl_brdf_lut = nullptr;
  ibl_prefilter = nullptr;
  ibl_irradiance = nullptr;
  sampler = nullptr;
  texture = nullptr;
  equirect_to_bake = nullptr;
  device = nullptr;
}

} // namespace gfx

} // namespace wsl
