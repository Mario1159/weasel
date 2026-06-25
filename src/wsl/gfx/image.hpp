#pragma once

#include "gpu_resources.hpp"

#include <SDL3/SDL_gpu.h>

#include <utility>

namespace wsl
{

namespace gfx
{

// Owning wrapper for an SDL_GPUTexture (and optional SDL_GPUSampler).
//
// Lifetime is tied to the gpu_texture / gpu_sampler members; the
// destructors call SDL_Release* and report the free to Tracy's
// wsl.gfx.textures / wsl.gfx.samplers memory pools. The struct is
// move-only (copy is deleted) and move-constructible / move-
// assignable, which is what entt::resource_cache<gfx::image> needs
// to store it in its internal storage.
//
// Inspector / UI code that needs the raw SDL_GPUTexture* for
// ImGui::Image can call .texture.get() (or .sampler.get() for the
// sampler). The wrappers implicitly convert via get() in the same
// expression context as a raw pointer.
struct image
{
  gpu_texture texture;
  gpu_sampler sampler;

  image () = default;

  image (const image &) = delete;
  image &operator= (const image &) = delete;

  image (image &&other) noexcept = default;
  image &operator= (image &&other) noexcept = default;
};

} // namespace gfx

} // namespace wsl
