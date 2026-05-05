#pragma once

#include "../gfx/image.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <entt/resource/loader.hpp>
#include <memory>
#include <string>


namespace wsl
{

namespace rsc
{

namespace raw
{

/*!
 * \brief CPU-side image container owned by the resource loader.
 *
 * The surface is freed in the destructor. Callers receive ownership via
 * std::shared_ptr<raw::image_cpu> and may move the surface into GPU upload logic.
 */
struct image_cpu
{
  SDL_Surface *surface = nullptr;

  ~image_cpu ()
  {
    if (surface != nullptr) {
      SDL_DestroySurface (surface);
    }
  }
};

} // namespace raw

/*!
 * \brief Image loader for disk -> GPU images.
 *
 * Behavior summary:
 * - SVG files are rasterized with SDL_image 3 via IMG_LoadSizedSVG_IO at a
 *   supersampled pixel size to improve antialiasing for small UI icons.
 * - Loaded 8-bit RGBA surfaces are converted to premultiplied alpha before
 *   upload to avoid haloing when sampling and compositing.
 * - A CPU mipmap chain is generated with SDL_ScaleSurface and each level is
 *   uploaded to an SDL_gpu texture. HDR/float surfaces are uploaded only at
 *   base level (no CPU mip generation).
 * - A default SDL_GPUSampler (linear + mipmap linear) is created and attached
 *   to returned gfx::image instances when possible.
 */
struct image_loader final : entt::resource_loader<gfx::image>
{
  std::shared_ptr<gfx::image> operator() (const std::string & /*unused*/) const;
  std::shared_ptr<gfx::image> operator() (gfx::image &&ready_image) const;

  /*! \brief Load an image from disk into an SDL_Surface. */
  static std::shared_ptr<raw::image_cpu> load_cpu (const std::string &path);

  /*! \brief Upload a CPU image to the provided SDL_GPUDevice and return a GPU image.
   *  \param device GPU device to upload into (must be valid)
   *  \param cpu  CPU image wrapper containing an SDL_Surface
   *  \return a gfx::image owning the GPU texture (and sampler when available)
   */
  static gfx::image upload_gpu (SDL_GPUDevice *device, raw::image_cpu &cpu);
};

} // namespace rsc

} // namespace wsl
