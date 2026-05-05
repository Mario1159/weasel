#include "image_loader.hpp"
#include "gfx/image.hpp"
#include "stb_image.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>
#include <spdlog/spdlog.h>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <algorithm>

namespace wsl
{

namespace rsc
{

std::shared_ptr<gfx::image>
image_loader::operator() (const std::string & /*unused*/) const
{
  return {};
}

std::shared_ptr<gfx::image>
image_loader::operator() (gfx::image &&ready_image) const
{
  return std::make_shared<gfx::image> (std::move (ready_image));
}

std::shared_ptr<raw::image_cpu>
image_loader::load_cpu (const std::string &path)
{
  std::filesystem::path const p (path);
  std::string ext = p.extension ().string ();
  for (auto &c : ext) {
    c = (char)std::tolower (c);
  }

  if (ext == ".hdr") {
    int w;
    int h;
    int channels;
    float *data = stbi_loadf (path.c_str (), &w, &h, &channels, 4);
    if (data == nullptr) {
      SDL_Log ("stbi_loadf failed for %s: %s", path.c_str (),
               stbi_failure_reason ());
      return {};
    }

    SDL_Surface *surf = SDL_CreateSurfaceFrom (
        w, h, SDL_PIXELFORMAT_RGBA128_FLOAT, data, w * 16);
    if (surf == nullptr) {
      stbi_image_free (data);
      return {};
    }

    // Duplicate to own the memory
    SDL_Surface *owned = SDL_DuplicateSurface (surf);
    SDL_DestroySurface (surf);
    stbi_image_free (data);

    auto cpu = std::make_shared<raw::image_cpu> ();
    cpu->surface = owned;
    return cpu;
  }

  // Special-case SVG: rasterize at a higher resolution to get smooth AA when
  // the texture is sampled smaller in the UI. Use SDL_image 3's
  // IMG_LoadSizedSVG_IO to rasterize at exact pixel dimensions; do not use
  // any fallback. If loading fails, return empty.
  if (ext == ".svg") {
    constexpr int SVG_TARGET_LOGICAL_PX = 32; // logical icon size
    constexpr int SVG_RASTER_SCALE = 4;       // supersample factor
    const int desired_px = SVG_TARGET_LOGICAL_PX * SVG_RASTER_SCALE;

    // Read SVG file into memory
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      SDL_Log("Failed to open SVG %s", path.c_str());
      return {};
    }

    std::string svg_src((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    SDL_IOStream *io = SDL_IOFromConstMem(svg_src.data(), svg_src.size());
    if (io == nullptr) {
      SDL_Log("SDL_IOFromConstMem failed for %s: %s", path.c_str(), SDL_GetError());
      return {};
    }

    // IMG_LoadSizedSVG_IO rasterizes at the requested pixel size.
    SDL_Surface *svg_surf = IMG_LoadSizedSVG_IO(io, desired_px, desired_px);
    // IMG_LoadSizedSVG_IO does NOT close the SDL_IOStream; close it explicitly.
    SDL_CloseIO(io);

    if (svg_surf == nullptr) {
      SDL_Log("IMG_LoadSizedSVG_IO failed for %s: %s", path.c_str(), SDL_GetError());
      return {};
    }

    auto cpu = std::make_shared<raw::image_cpu>();
    cpu->surface = svg_surf;
    return cpu;
  }

  SDL_Surface *surf = IMG_Load (path.c_str ());
  if (surf == nullptr) {
    SDL_Log ("Failed to load image %s: %s", path.c_str (), SDL_GetError ());
    return {};
  }

  auto cpu = std::make_shared<raw::image_cpu> ();
  cpu->surface = surf;
  return cpu;
}

gfx::image
image_loader::upload_gpu (SDL_GPUDevice *device, raw::image_cpu &cpu)
{
  SDL_GPUTextureFormat gpu_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  SDL_PixelFormat convert_format = SDL_PIXELFORMAT_RGBA32;
  Uint32 bytes_per_pixel = 4;

  if (cpu.surface->format == SDL_PIXELFORMAT_RGBA128_FLOAT) {
    gpu_format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    convert_format = SDL_PIXELFORMAT_RGBA128_FLOAT;
    bytes_per_pixel = 16;
  }

  SDL_Surface *rgba = SDL_ConvertSurface (cpu.surface, convert_format);
  if (rgba == nullptr) {
    SDL_Log ("Surface conversion failed: %s", SDL_GetError ());
    return {};
  }

  // Convert to premultiplied alpha in-place for nicer compositing when GPU uses premultiplied blending.
  // Use SDL helpers to be correct with pixel formats.
  if (convert_format == SDL_PIXELFORMAT_RGBA32) {
    SDL_LockSurface(rgba);
    const int w = rgba->w;
    const int h = rgba->h;
    const int pitch = rgba->pitch;
    const SDL_PixelFormatDetails *fmt_details = SDL_GetPixelFormatDetails(rgba->format);
    const SDL_Palette *fmt_palette = nullptr; // non-indexed formats don't need a palette
    for (int y = 0; y < h; ++y) {
      uint8_t *row = static_cast<uint8_t *>(rgba->pixels) + y * pitch;
      for (int x = 0; x < w; ++x) {
        uint32_t pixel = *(uint32_t *)(row + x * 4);
        uint8_t r, g, b, a;
        SDL_GetRGBA(pixel, fmt_details, fmt_palette, &r, &g, &b, &a);
        if (a == 255) continue;
        uint8_t pr = static_cast<uint8_t>((int(r) * int(a) + 127) / 255);
        uint8_t pg = static_cast<uint8_t>((int(g) * int(a) + 127) / 255);
        uint8_t pb = static_cast<uint8_t>((int(b) * int(a) + 127) / 255);
        uint32_t mapped = SDL_MapRGBA(fmt_details, fmt_palette, pr, pg, pb, a);
        *(uint32_t *)(row + x * 4) = mapped;
      }
    }
    SDL_UnlockSurface(rgba);
  }

  const Uint32 width = rgba->w;
  const Uint32 height = rgba->h;

  auto mip_count_2d = [] (uint32_t w, uint32_t h) {
    uint32_t levels = 1;
    while (w > 1 || h > 1) {
      w = (w > 1) ? (w >> 1) : 1;
      h = (h > 1) ? (h >> 1) : 1;
      ++levels;
    }
    return levels;
  };

  // Create a default sampler with linear filtering + linear mipmaps to ensure
  // smooth downsampling when rendering small icons. It is safe to create now
  // and attach it to returned images; if creation fails we continue without it.
  SDL_GPUSampler *default_sampler = nullptr;
  {
    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    default_sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (default_sampler == nullptr) {
      SDL_Log("Failed to create default sampler: %s", SDL_GetError());
      // continue without sampler
    }
  }

  // If the surface is HDR (16 bytes per pixel / float), avoid CPU mip generation
  // because SDL_BlitSurfaceScaled doesn't reliably support float surfaces on
  // all backends. Upload only level 0 in that case.
  if (bytes_per_pixel == 16) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = gpu_format;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture (device, &info);
    if (texture == nullptr) {
      SDL_DestroySurface (rgba);
      return {};
    }

    const Uint32 upload_size = width * height * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = upload_size;

    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer (device, &tb_info);
    if (tb == nullptr) {
      SDL_DestroySurface (rgba);
      SDL_ReleaseGPUTexture (device, texture);
      return {};
    }

    void *mapped = SDL_MapGPUTransferBuffer (device, tb, true);
    std::memcpy (mapped, rgba->pixels, upload_size);
    SDL_UnmapGPUTransferBuffer (device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);

    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = tb;
    src.offset = 0;
    src.pixels_per_row = width;
    src.rows_per_layer = height;

    SDL_GPUTextureRegion dst{};
    dst.texture = texture;
    dst.mip_level = 0;
    dst.layer = 0;
    dst.x = 0;
    dst.y = 0;
    dst.z = 0;
    dst.w = width;
    dst.h = height;
    dst.d = 1;

    SDL_UploadToGPUTexture (copy, &src, &dst, false);

    SDL_EndGPUCopyPass (copy);
    SDL_SubmitGPUCommandBuffer (cmd);

    SDL_ReleaseGPUTransferBuffer (device, tb);
    SDL_DestroySurface (rgba);

    gfx::image result{};
    result.texture = texture;
    result.device = device;
    // Attach default sampler if created.
    if (default_sampler != nullptr) {
      result.sampler = default_sampler;
    }
    return result;
  }

  const uint32_t mip_levels = mip_count_2d (width, height);

  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = gpu_format;
  info.width = width;
  info.height = height;
  info.layer_count_or_depth = 1;
  info.num_levels = mip_levels;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture (device, &info);
  if (texture == nullptr) {
    SDL_DestroySurface (rgba);
    return {};
  }


  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);

  // Upload each mip level by scaling the source surface on the CPU.
  for (uint32_t level = 0; level < mip_levels; ++level) {
    const uint32_t lw = std::max<uint32_t> (1u, width >> level);
    const uint32_t lh = std::max<uint32_t> (1u, height >> level);

    // Create a temporary surface for this mip level. Level 0 uses the
    // already-converted 'rgba' surface; for others, create and blit-scaled.
    SDL_Surface *level_surf = nullptr;
    if (level == 0) {
      level_surf = rgba;
    } else {
      level_surf = SDL_CreateSurface ((int)lw, (int)lh, convert_format);
      if (level_surf == nullptr) {
        SDL_Log ("Failed to create mip surface: %s", SDL_GetError ());
        // abort mip generation; cleanup and return
        SDL_EndGPUCopyPass (copy);
        SDL_SubmitGPUCommandBuffer (cmd);
        SDL_ReleaseGPUTexture (device, texture);
        SDL_DestroySurface (rgba);
        return {};
      }

      // Use SDL_ScaleSurface to produce a properly scaled surface for the mip
      // level. SDL_ScaleSurface allocates a new surface we own and can upload.
      SDL_Surface *scaled = SDL_ScaleSurface(rgba, (int)lw, (int)lh, SDL_SCALEMODE_LINEAR);
      if (scaled == nullptr) {
        SDL_Log("SDL_ScaleSurface failed: %s", SDL_GetError());
        SDL_DestroySurface(level_surf);
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_DestroySurface(rgba);
        return {};
      }
      // Replace level_surf with the scaled result
      if (level_surf != nullptr && level_surf != rgba) SDL_DestroySurface(level_surf);
      level_surf = scaled;
    }

    const Uint32 level_size = lw * lh * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = level_size;

    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer (device, &tb_info);
    if (tb == nullptr) {
      SDL_Log ("Failed to create transfer buffer: %s", SDL_GetError ());
      if (level != 0) SDL_DestroySurface (level_surf);
      SDL_EndGPUCopyPass (copy);
      SDL_SubmitGPUCommandBuffer (cmd);
      SDL_ReleaseGPUTexture (device, texture);
      SDL_DestroySurface (rgba);
      return {};
    }

    void *mapped = SDL_MapGPUTransferBuffer (device, tb, true);
    // Copy row-by-row to account for pitch
    uint8_t *mapped_ptr = static_cast<uint8_t *> (mapped);
    uint8_t *src = static_cast<uint8_t *> (level_surf->pixels);
    for (uint32_t y = 0; y < lh; ++y) {
      std::memcpy (mapped_ptr + size_t (y) * lw * bytes_per_pixel,
                   src + size_t (y) * level_surf->pitch, lw * bytes_per_pixel);
    }
    SDL_UnmapGPUTransferBuffer (device, tb);

    SDL_GPUTextureTransferInfo src_info{};
    src_info.transfer_buffer = tb;
    src_info.offset = 0;
    src_info.pixels_per_row = lw;
    src_info.rows_per_layer = lh;

    SDL_GPUTextureRegion dst{};
    dst.texture = texture;
    dst.mip_level = (Uint32)level;
    dst.layer = 0;
    dst.x = 0;
    dst.y = 0;
    dst.z = 0;
    dst.w = lw;
    dst.h = lh;
    dst.d = 1;

    SDL_UploadToGPUTexture (copy, &src_info, &dst, false);

    SDL_ReleaseGPUTransferBuffer (device, tb);
    if (level != 0) SDL_DestroySurface (level_surf);
  }

  SDL_EndGPUCopyPass (copy);
  SDL_SubmitGPUCommandBuffer (cmd);

  // 'rgba' was destroyed on cleanup path previously; destroy it now.
  SDL_DestroySurface (rgba);

  gfx::image result{};
  result.texture = texture;
  result.device = device;
  return result;
}

} // namespace rsc

} // namespace wsl
