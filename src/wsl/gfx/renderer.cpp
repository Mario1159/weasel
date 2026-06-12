#include "renderer.hpp"
#include "render_context.hpp"
#include <cstring>

namespace wsl::gfx
{

renderer::renderer (wsl::gfx::render_window &window, render_context *ctx,
                    wsl::rsc::resource_manager *res_mgr)
    : m_window (&window), m_ctx (ctx), m_res_mgr (res_mgr)
{
}

auto
renderer::create_1x1_texture (uint8_t red, uint8_t green, uint8_t blue,
                              uint8_t alpha) const -> SDL_GPUTexture *
{
  SDL_GPUTextureCreateInfo tex{};
  tex.type = SDL_GPU_TEXTURETYPE_2D;
  tex.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex.width = 1;
  tex.height = 1;
  tex.layer_count_or_depth = 1;
  tex.num_levels = 1;
  tex.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &tex);
  if (texture == nullptr) {
    return nullptr;
  }

  SDL_GPUTransferBufferCreateInfo tinfo{};
  tinfo.size = 4;
  tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

  SDL_GPUTransferBuffer *upload
      = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tinfo);
  if (upload == nullptr) {
    return texture;
  }

  uint8_t pixels[4] = { red, green, blue, alpha };

  void *mapped = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, upload, false);
  std::memcpy (mapped, pixels, 4);
  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, upload);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = upload;
  src.offset = 0;
  src.pixels_per_row = 1;
  src.rows_per_layer = 1;

  SDL_GPUTextureRegion dst{};
  dst.texture = texture;
  dst.mip_level = 0;
  dst.layer = 0;
  dst.x = 0;
  dst.y = 0;
  dst.z = 0;
  dst.w = 1;
  dst.h = 1;
  dst.d = 1;

  SDL_UploadToGPUTexture (copy, &src, &dst, false);

  SDL_EndGPUCopyPass (copy);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, upload);
  return texture;
}

auto
renderer::create_1x1_cubemap (uint8_t red, uint8_t green, uint8_t blue,
                              uint8_t alpha) const -> SDL_GPUTexture *
{
  SDL_GPUTextureCreateInfo tex{};
  tex.type = SDL_GPU_TEXTURETYPE_CUBE;
  tex.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex.width = 1;
  tex.height = 1;
  tex.layer_count_or_depth = 6;
  tex.num_levels = 1;
  tex.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &tex);
  if (texture == nullptr) {
    return nullptr;
  }

  SDL_GPUTransferBufferCreateInfo tinfo{};
  tinfo.size = 4 * 6;
  tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

  SDL_GPUTransferBuffer *upload
      = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tinfo);
  if (upload == nullptr) {
    return texture;
  }

  uint8_t px[4 * 6];
  for (int i = 0; i < 6; ++i) {
    px[(i * 4) + 0] = red;
    px[(i * 4) + 1] = green;
    px[(i * 4) + 2] = blue;
    px[(i * 4) + 3] = alpha;
  }

  void *mapped = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, upload, false);
  if (mapped != nullptr) {
    std::memcpy (mapped, px, static_cast<size_t> (4 * 6));
    SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, upload);
  }

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd);

  for (int i = 0; i < 6; ++i) {
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = upload;
    src.offset = i * 4;
    src.pixels_per_row = 1;
    src.rows_per_layer = 1;

    SDL_GPUTextureRegion dst{};
    dst.texture = texture;
    dst.mip_level = 0;
    dst.layer = i;
    dst.x = 0;
    dst.y = 0;
    dst.z = 0;
    dst.w = 1;
    dst.h = 1;
    dst.d = 1;

    SDL_UploadToGPUTexture (copy, &src, &dst, false);
  }

  SDL_EndGPUCopyPass (copy);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, upload);
  return texture;
}

} // namespace wsl::gfx
