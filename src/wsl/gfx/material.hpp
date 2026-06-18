#pragma once

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace wsl
{

namespace gfx
{

/*!
 * \brief CPU-side material data used by mesh primitives.
 */
struct material
{
  glm::vec4 base_color_factor{ 1.0F };
  float metallic_factor = 0.0F;
  float roughness_factor = 1.0F;
  float pad0[2]{};

  glm::vec3 emissive_factor{ 0.0F };
  float pad1 = 0.0F;

  //! Base color texture sampled by the material.
  SDL_GPUTexture *base_color_tex = nullptr;
  //! Metallic-roughness texture sampled by the material.
  SDL_GPUTexture *metallic_roughness_tex = nullptr;
  //! Normal map texture sampled by the material.
  SDL_GPUTexture *normal_tex = nullptr;
  //! Occlusion texture sampled by the material.
  SDL_GPUTexture *occlusion_tex = nullptr;
  //! Emissive texture sampled by the material.
  SDL_GPUTexture *emissive_tex = nullptr;

  //! Sampler shared by the material textures.
  SDL_GPUSampler *sampler = nullptr;

  bool double_sided = false;

  //! GPU device used to release resources.
  SDL_GPUDevice *device = nullptr;

  material () = default;
  ~material () { release (); }

  material (const material &) = delete;
  material &operator= (const material &) = delete;

  material (material &&other) noexcept { *this = std::move (other); }

  material &
  operator= (material &&other) noexcept
  {
    if (this != &other) {
      release ();

      base_color_factor = other.base_color_factor;
      metallic_factor = other.metallic_factor;
      roughness_factor = other.roughness_factor;
      emissive_factor = other.emissive_factor;
      double_sided = other.double_sided;

      base_color_tex = other.base_color_tex;
      metallic_roughness_tex = other.metallic_roughness_tex;
      normal_tex = other.normal_tex;
      occlusion_tex = other.occlusion_tex;
      emissive_tex = other.emissive_tex;
      sampler = other.sampler;
      device = other.device;

      other.base_color_tex = nullptr;
      other.metallic_roughness_tex = nullptr;
      other.normal_tex = nullptr;
      other.occlusion_tex = nullptr;
      other.emissive_tex = nullptr;
      other.sampler = nullptr;
      other.device = nullptr;
    }
    return *this;
  }

private:
  void
  release ()
  {
    if (device == nullptr) {
      return;
    }

    if (base_color_tex != nullptr) {
      SDL_ReleaseGPUTexture (device, base_color_tex);
    }
    if (metallic_roughness_tex != nullptr) {
      SDL_ReleaseGPUTexture (device, metallic_roughness_tex);
    }
    if (normal_tex != nullptr) {
      SDL_ReleaseGPUTexture (device, normal_tex);
    }
    if (occlusion_tex != nullptr) {
      SDL_ReleaseGPUTexture (device, occlusion_tex);
    }
    if (emissive_tex != nullptr) {
      SDL_ReleaseGPUTexture (device, emissive_tex);
    }
    if (sampler != nullptr) {
      SDL_ReleaseGPUSampler (device, sampler);
    }

    base_color_tex = nullptr;
    metallic_roughness_tex = nullptr;
    normal_tex = nullptr;
    occlusion_tex = nullptr;
    emissive_tex = nullptr;
    sampler = nullptr;
    device = nullptr;
  }
};

/*!
 * \brief GPU material parameters uploaded to shader buffers.
 */
struct gpu_material
{
  glm::vec4 base_color;
  float metallic;
  float roughness;
  float pad0[2]{};
  glm::vec3 emissive;
  float mip_lod_bias = 0.0F;
};

} // namespace gfx

} // namespace wsl
