#include "cubemap_loader.hpp"
#include "../gfx/tracy_gpu_mem.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <archive.h>
#include <archive_entry.h>

#include "gfx/cubemap.hpp"
#include "stb_image.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wsl
{

namespace rsc
{

uint32_t
cubemap_loader::mip_count_2d (uint32_t w, uint32_t h)
{
  uint32_t levels = 1;
  while (w > 1 || h > 1) {
    w = (w > 1) ? (w >> 1) : 1;
    h = (h > 1) ? (h >> 1) : 1;
    ++levels;
  }
  return levels;
}

// Prefer HDR formats if your backend supports them.
// If your SDL GPU build doesn’t have RGBA16F, switch to RGBA8.
static SDL_GPUTextureFormat ibl_cube_format
    = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
static SDL_GPUTextureFormat ibl_lut_format
    = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // safe/simple (store RG in
                                                // .rg)

int
cubemap_loader::face_index_from_name (const std::string &name)
{
  if (name == "px.png") {
    return 0;
  }
  if (name == "nx.png") {
    return 1;
  }
  if (name == "py.png") {
    return 2;
  }
  if (name == "ny.png") {
    return 3;
  }
  if (name == "pz.png") {
    return 4;
  }
  if (name == "nz.png") {
    return 5;
  }
  return -1;
}

bool
cubemap_loader::load_rgba_image_from_memory (const uint8_t *data, size_t size,
                                             int &w, int &h,
                                             std::vector<uint8_t> &pixels)
{

  SDL_IOStream *io = SDL_IOFromConstMem (data, size);
  if (io == nullptr) {
    wsl::log::rsc ()->error ("Cubemap: SDL_IOFromConstMem failed");
    return false;
  }

  // IMPORTANT: force PNG decoder
  SDL_Surface *surf = IMG_LoadTyped_IO (io, true, "PNG");
  if (surf == nullptr) {
    wsl::log::rsc ()->error ("Cubemap: IMG_LoadTyped_IO failed: {}",
                             SDL_GetError ());
    return false;
  }

  SDL_Surface *rgba = SDL_ConvertSurface (surf, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface (surf);

  if (rgba == nullptr) {
    wsl::log::rsc ()->error ("Cubemap: failed to convert surface to RGBA");
    return false;
  }

  w = rgba->w;
  h = rgba->h;
  pixels.resize (size_t (w) * size_t (h) * 4);

  uint8_t *dst = pixels.data ();
  uint8_t const *src = static_cast<uint8_t *> (rgba->pixels);

  for (int y = 0; y < h; ++y) {
    std::memcpy (dst + (size_t (y) * w * 4), src + (size_t (y) * rgba->pitch),
                 size_t (w) * 4);
  }

  SDL_DestroySurface (rgba);
  return true;
}

bool
cubemap_loader::upload_cubemap (
    SDL_GPUTexture *tex, int w, int h,
    const std::array<std::vector<uint8_t>, 6> &faces) const
{

  const size_t face_size = size_t (w) * size_t (h) * 4;
  const size_t total_size = face_size * 6;

  SDL_GPUTransferBufferCreateInfo tbi{};
  tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tbi.size = total_size;

  SDL_GPUTransferBuffer *upload
      = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tbi);

  uint8_t *mapped
      = (uint8_t *)SDL_MapGPUTransferBuffer (m_ctx->gpu_device, upload, false);

  for (int face = 0; face < 6; ++face) {
    std::memcpy (mapped + (face * face_size), faces[face].data (), face_size);
  }

  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, upload);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);

  for (int face = 0; face < 6; ++face) {
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = upload;
    src.offset = face_size * face;
    src.pixels_per_row = (Uint32)w;
    src.rows_per_layer = (Uint32)h;

    SDL_GPUTextureRegion dst{};
    dst.texture = tex;
    dst.mip_level = 0;
    dst.layer = face;
    dst.x = 0;
    dst.y = 0;
    dst.z = 0;
    dst.w = (Uint32)w;
    dst.h = (Uint32)h;
    dst.d = 1;

    SDL_UploadToGPUTexture (cp, &src, &dst, false);
  }

  SDL_EndGPUCopyPass (cp);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, upload);
  return true;
}

std::shared_ptr<gfx::cubemap>
cubemap_loader::operator() (const std::string &path) const
{
  if (path == "builtin/skybox_procedural") {
    auto cube = std::make_shared<gfx::cubemap> ();
    cube->device = m_ctx->gpu_device;

    const uint32_t cube_size = 1024;
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_CUBE;
    ti.format = ibl_cube_format;
    ti.width = cube_size;
    ti.height = cube_size;
    ti.layer_count_or_depth = 6;
    ti.num_levels = 1;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    cube->texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &ti);

    SDL_GPUSamplerCreateInfo si{};
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    cube->sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &si);

    // Allocate IBL resources
    {
      const uint32_t irr_size = 64;
      SDL_GPUTextureCreateInfo irr = ti;
      irr.width = irr.height = irr_size;
      cube->ibl_irradiance
          = SDL_CreateGPUTexture (m_ctx->gpu_device, &irr);

      const uint32_t pre_size = 256;
      const uint32_t pre_mips = mip_count_2d (pre_size, pre_size);
      SDL_GPUTextureCreateInfo pre = ti;
      pre.width = pre.height = pre_size;
      pre.num_levels = pre_mips;
      cube->ibl_prefilter
          = SDL_CreateGPUTexture (m_ctx->gpu_device, &pre);
      cube->prefilter_mip_count = pre_mips;
      cube->prefilter_max_mip = (float)(pre_mips - 1);

      const uint32_t lut_size = 256;
      SDL_GPUTextureCreateInfo lut{};
      lut.type = SDL_GPU_TEXTURETYPE_2D;
      lut.format = ibl_lut_format;
      lut.width = lut_size;
      lut.height = lut_size;
      lut.layer_count_or_depth = 1;
      lut.num_levels = 1;
      lut.usage
          = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
      cube->ibl_brdf_lut
          = SDL_CreateGPUTexture (m_ctx->gpu_device, &lut);

      SDL_GPUSamplerCreateInfo iblsi = si;
      iblsi.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
      cube->ibl_sampler
          = SDL_CreateGPUSampler (m_ctx->gpu_device, &iblsi);
    }

    return cube;
  }

  std::filesystem::path const p (path);
  std::string ext = p.extension ().string ();
  for (auto &c : ext) {
    c = (char)std::tolower (c);
  }

  if (ext == ".tar") {
    return load_from_tar (path);
  }
  if (ext == ".png" || ext == ".hdr") {
    return load_from_equirect (path);
  }

  wsl::log::rsc ()->error ("Cubemap: unsupported extension {}", ext);
  return {};
}

std::shared_ptr<gfx::cubemap>
cubemap_loader::load_from_tar (const std::string &path) const
{

  std::array<std::vector<uint8_t>, 6> face_pixels;
  std::array<bool, 6> found{};
  int w = 0;
  int h = 0;

  archive *ar = archive_read_new ();
  archive_read_support_format_tar (ar);
  archive_read_support_filter_all (ar);

  if (archive_read_open_filename (ar, path.c_str (), 10240) != ARCHIVE_OK) {
    wsl::log::rsc ()->error ("Cubemap: failed to open archive {}", path);
    archive_read_free (ar);
    return {};
  }

  archive_entry *entry;

  while (archive_read_next_header (ar, &entry) == ARCHIVE_OK) {

    if (archive_entry_filetype (entry) != AE_IFREG) {
      archive_read_data_skip (ar);
      continue;
    }

    std::filesystem::path const p = archive_entry_pathname (entry);
    std::string name = p.filename ().string ();

    int const idx = face_index_from_name (name);
    if (idx < 0) {
      archive_read_data_skip (ar);
      continue;
    }

    size_t const size = (size_t)archive_entry_size (entry);
    std::vector<uint8_t> buffer (size);

    size_t offset = 0;
    while (offset < size) {
      la_ssize_t const r
          = archive_read_data (ar, buffer.data () + offset, size - offset);
      if (r <= 0) {
        break;
      }
      offset += (size_t)r;
    }

    int iw = 0;
    int ih = 0;
    if (!load_rgba_image_from_memory (buffer.data (), buffer.size (), iw, ih,
                                      face_pixels[idx])) {
      wsl::log::rsc ()->error ("Cubemap: failed to decode {}", name);
      archive_read_free (ar);
      return {};
    }

    if (!found[idx]) {
      found[idx] = true;
      if (w == 0 && h == 0) {
        w = iw;
        h = ih;
      } else if (iw != w || ih != h) {
        wsl::log::rsc ()->error ("Cubemap: face {} has mismatched size", name);
        archive_read_free (ar);
        return {};
      }
    }
  }

  archive_read_free (ar);

  for (int i = 0; i < 6; ++i) {
    if (!found[i]) {
      wsl::log::rsc ()->error ("Cubemap: missing face {}", i);
      return {};
    }
  }

  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_CUBE;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  ti.width = (Uint32)w;
  ti.height = (Uint32)h;
  ti.layer_count_or_depth = 6;
  ti.num_levels = 1;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  auto cube = std::make_shared<gfx::cubemap> ();
  cube->device = m_ctx->gpu_device;

  cube->texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &ti);
  if (cube->texture == nullptr) {
    wsl::log::rsc ()->error ("Cubemap: SDL_CreateGPUTexture failed: {}",
                             SDL_GetError ());
    return {};
  }

  upload_cubemap (cube->texture, w, h, face_pixels);

  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = SDL_GPU_FILTER_LINEAR;
  si.mag_filter = SDL_GPU_FILTER_LINEAR;
  si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR; // ok even if env has 1 mip
  si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  cube->sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &si);
  if (cube->sampler == nullptr) {
    wsl::log::rsc ()->error ("Cubemap: SDL_CreateGPUSampler failed: {}",
                             SDL_GetError ());
    return {};
  }

  // =========================================================
  // IBL ALLOCATION (no baking here, just create the textures)
  // =========================================================

  // (A) Irradiance cubemap (small)
  const uint32_t irr_size = 64;

  SDL_GPUTextureCreateInfo irr{};
  irr.type = SDL_GPU_TEXTURETYPE_CUBE;
  irr.format = ibl_cube_format;
  irr.width = irr_size;
  irr.height = irr_size;
  irr.layer_count_or_depth = 6;
  irr.num_levels = 1;
  irr.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  cube->ibl_irradiance = SDL_CreateGPUTexture (m_ctx->gpu_device, &irr);
  if (cube->ibl_irradiance == nullptr) {
    wsl::log::rsc ()->error ("IBL: failed to create irradiance cubemap: {}",
                             SDL_GetError ());
    return {};
  }

  // (B) Prefilter cubemap with mip chain (specular)
  const uint32_t pre_size = 256;
  const uint32_t pre_mips = mip_count_2d (pre_size, pre_size);

  SDL_GPUTextureCreateInfo pre{};
  pre.type = SDL_GPU_TEXTURETYPE_CUBE;
  pre.format = ibl_cube_format;
  pre.width = pre_size;
  pre.height = pre_size;
  pre.layer_count_or_depth = 6;
  pre.num_levels = pre_mips;
  pre.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  cube->ibl_prefilter = SDL_CreateGPUTexture (m_ctx->gpu_device, &pre);
  if (cube->ibl_prefilter == nullptr) {
    wsl::log::rsc ()->error ("IBL: failed to create prefilter cubemap: {}",
                             SDL_GetError ());
    return {};
  }

  cube->prefilter_mip_count = pre_mips;
  cube->prefilter_max_mip = float (pre_mips - 1);

  // (C) BRDF LUT 2D
  const uint32_t lut_size = 256;

  SDL_GPUTextureCreateInfo lut{};
  lut.type = SDL_GPU_TEXTURETYPE_2D;
  lut.format = ibl_lut_format;
  lut.width = lut_size;
  lut.height = lut_size;
  lut.layer_count_or_depth = 1;
  lut.num_levels = 1;
  lut.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  cube->ibl_brdf_lut = SDL_CreateGPUTexture (m_ctx->gpu_device, &lut);
  if (cube->ibl_brdf_lut == nullptr) {
    wsl::log::rsc ()->error ("IBL: failed to create BRDF LUT: {}",
                             SDL_GetError ());
    return {};
  }

  // One sampler for all IBL maps (clamp + mip)
  SDL_GPUSamplerCreateInfo iblsi{};
  iblsi.min_filter = SDL_GPU_FILTER_LINEAR;
  iblsi.mag_filter = SDL_GPU_FILTER_LINEAR;
  iblsi.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  iblsi.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  iblsi.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  iblsi.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  cube->ibl_sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &iblsi);
  if (cube->ibl_sampler == nullptr) {
    wsl::log::rsc ()->error ("IBL: failed to create IBL sampler: {}",
                             SDL_GetError ());
    return {};
  }

  wsl::log::rsc ()->debug (
      "Cubemap loaded from {} (irr {} pre {} mips {} lut {})", path, irr_size,
      pre_size, pre_mips, lut_size);

  return cube;
}

std::shared_ptr<gfx::cubemap>
cubemap_loader::load_from_equirect (const std::string &path) const
{
  std::filesystem::path const p (path);
  std::string ext = p.extension ().string ();
  for (auto &c : ext) {
    c = (char)std::tolower (c);
  }

  int w;
  int h;
  int channels;
  float *pixels_to_free = nullptr;
  size_t upload_size = 0;

  if (ext == ".hdr") {
    pixels_to_free = stbi_loadf (path.c_str (), &w, &h, &channels, 4);
    if (pixels_to_free == nullptr) {
      wsl::log::rsc ()->error (
          "Cubemap: failed to load HDR equirect image {}: {}", path,
          stbi_failure_reason ());
      return {};
    }
    upload_size = size_t (w) * size_t (h) * 16;
  } else {
    SDL_Surface *surf = IMG_Load (path.c_str ());
    if (surf == nullptr) {
      wsl::log::rsc ()->error ("Cubemap: failed to load equirect image {}: {}",
                               path, SDL_GetError ());
      return {};
    }

    SDL_Surface *rgba
        = SDL_ConvertSurface (surf, SDL_PIXELFORMAT_RGBA128_FLOAT);
    SDL_DestroySurface (surf);

    if (rgba == nullptr) {
      wsl::log::rsc ()->error (
          "Cubemap: failed to convert equirect image to RGBA float");
      return {};
    }

    w = rgba->w;
    h = rgba->h;
    upload_size = size_t (w) * size_t (h) * 16;
    pixels_to_free = (float *)SDL_malloc (upload_size);
    std::memcpy (pixels_to_free, rgba->pixels, upload_size);
    SDL_DestroySurface (rgba);
  }

  SDL_GPUTextureCreateInfo ti2d{};
  ti2d.type = SDL_GPU_TEXTURETYPE_2D;
  ti2d.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  ti2d.width = (Uint32)w;
  ti2d.height = (Uint32)h;
  ti2d.layer_count_or_depth = 1;
  ti2d.num_levels = 1;
  ti2d.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *equi_tex
      = SDL_CreateGPUTexture (m_ctx->gpu_device, &ti2d);

  {
    SDL_GPUTransferBufferCreateInfo tbi{};
    tbi.size = (Uint32)upload_size;
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *tb
        = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tbi);
    void *mapped = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, tb, false);
    std::memcpy (mapped, pixels_to_free, upload_size);
    SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = tb;
    SDL_GPUTextureRegion dst{};
    dst.texture = equi_tex;
    dst.w = (Uint32)w;
    dst.h = (Uint32)h;
    dst.d = 1;
    SDL_UploadToGPUTexture (cp, &src, &dst, false);
    SDL_EndGPUCopyPass (cp);
    SDL_SubmitGPUCommandBuffer (cmd);
    SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, tb);
  }

  if (ext == ".hdr") {
    stbi_image_free (pixels_to_free);
  } else {
    SDL_free (pixels_to_free);
  }

  const uint32_t cube_size = 1024;
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_CUBE;
  ti.format = ibl_cube_format;
  ti.width = cube_size;
  ti.height = cube_size;
  ti.layer_count_or_depth = 6;
  ti.num_levels = 1;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  auto cube = std::make_shared<gfx::cubemap> ();
  cube->device = m_ctx->gpu_device;
  cube->texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &ti);

  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = SDL_GPU_FILTER_LINEAR;
  si.mag_filter = SDL_GPU_FILTER_LINEAR;
  si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  cube->sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &si);

  {
    const uint32_t irr_size = 64;
    SDL_GPUTextureCreateInfo irr = ti;
    irr.format = ibl_cube_format;
    irr.width = irr.height = irr_size;
    cube->ibl_irradiance
        = SDL_CreateGPUTexture (m_ctx->gpu_device, &irr);

    const uint32_t pre_size = 256;
    const uint32_t pre_mips = mip_count_2d (pre_size, pre_size);
    SDL_GPUTextureCreateInfo pre = ti;
    pre.format = ibl_cube_format;
    pre.width = pre.height = pre_size;
    pre.num_levels = pre_mips;
    cube->ibl_prefilter
        = SDL_CreateGPUTexture (m_ctx->gpu_device, &pre);
    cube->prefilter_mip_count = pre_mips;
    cube->prefilter_max_mip = (float)(pre_mips - 1);

    const uint32_t lut_size = 256;
    SDL_GPUTextureCreateInfo lut{};
    lut.type = SDL_GPU_TEXTURETYPE_2D;
    lut.format = ibl_lut_format;
    lut.width = lut_size;
    lut.height = lut_size;
    lut.layer_count_or_depth = 1;
    lut.num_levels = 1;
    lut.usage
        = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    cube->ibl_brdf_lut = SDL_CreateGPUTexture (m_ctx->gpu_device, &lut);

    SDL_GPUSamplerCreateInfo iblsi = si;
    iblsi.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    cube->ibl_sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &iblsi);
  }

  cube->equirect_to_bake = equi_tex;

  return cube;
}

cubemap_loader::cubemap_loader (gfx::render_context *ctx) : m_ctx (ctx) {}

} // namespace rsc

} // namespace wsl
