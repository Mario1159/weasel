#include "ui_render_interface.hpp"
#include "gfx/render_context.hpp"
#include "gfx/render_window.hpp"
#include "shader.hpp"
#include "wsl/rsc/resource_manager.hpp"

#include <RmlUi/Config/Config.h>
#include <RmlUi/Core/Colour.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi/Core/Span.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Vertex.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace wsl
{

ui_render_interface::ui_render_interface (gfx::render_context *ctx,
                                          wsl::gfx::render_window *window,
                                          wsl::rsc::resource_manager *res_mgr)
    : m_ctx (ctx), m_window (window)
{
  create_pipeline (res_mgr);
}

ui_render_interface::~ui_render_interface ()
{
  for (auto &[_, g] : m_geometries) {
    if (g.vbo != nullptr) {
      SDL_ReleaseGPUBuffer (m_ctx->gpu_device, g.vbo);
}
    if (g.ibo != nullptr) {
      SDL_ReleaseGPUBuffer (m_ctx->gpu_device, g.ibo);
}
  }

  for (auto &[_, t] : m_textures) {
    if (t.texture != nullptr) {
      SDL_ReleaseGPUTexture (m_ctx->gpu_device, t.texture);
}
  }

  if (m_pipeline != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline (m_ctx->gpu_device, m_pipeline);
}
}

void
ui_render_interface::create_pipeline (wsl::rsc::resource_manager *res_mgr)
{
  auto vs_id = res_mgr->register_shader ("engine://compiled_shaders/ui.vert.slang.spv");
  auto fs_id = res_mgr->register_shader ("engine://compiled_shaders/ui.frag.slang.spv");

  SDL_GPUShader *vs = gfx::shader::load_from_manager (
      m_ctx->gpu_device, res_mgr, vs_id,
      SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);

  SDL_GPUShader *fs = gfx::shader::load_from_manager (
      m_ctx->gpu_device, res_mgr, fs_id,
      SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);

  if ((vs == nullptr) || (fs == nullptr)) {
    if (vs != nullptr) {
      SDL_ReleaseGPUShader (m_ctx->gpu_device, vs);
}
    if (fs != nullptr) {
      SDL_ReleaseGPUShader (m_ctx->gpu_device, fs);
}
    return;
  }

  SDL_GPUGraphicsPipelineCreateInfo info{};
  SDL_zero (info);

  info.vertex_shader = vs;
  info.fragment_shader = fs;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

  info.depth_stencil_state.enable_depth_test = false;
  info.depth_stencil_state.enable_depth_write = false;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  SDL_GPUVertexBufferDescription vbuf{};
  vbuf.slot = 0;
  vbuf.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vbuf.pitch = sizeof (Rml::Vertex);

  static SDL_GPUVertexAttribute attrs[3];

  attrs[0]
      = SDL_GPUVertexAttribute{ .location = 0,
                                .buffer_slot = 0,
                                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                .offset = offsetof (Rml::Vertex, position) };

  attrs[1] = SDL_GPUVertexAttribute{ .location = 1,
                                     .buffer_slot = 0,
                                     .format
                                     = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                                     .offset = offsetof (Rml::Vertex, colour) };

  attrs[2]
      = SDL_GPUVertexAttribute{ .location = 2,
                                .buffer_slot = 0,
                                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                .offset = offsetof (Rml::Vertex, tex_coord) };

  info.vertex_input_state.num_vertex_buffers = 1;
  info.vertex_input_state.vertex_buffer_descriptions = &vbuf;
  info.vertex_input_state.num_vertex_attributes = 3;
  info.vertex_input_state.vertex_attributes = attrs;

  SDL_GPUColorTargetDescription ct{};
  ct.format
      = SDL_GetGPUSwapchainTextureFormat (m_ctx->gpu_device, m_window->handler);

  info.target_info.num_color_targets = 1;
  info.target_info.color_target_descriptions = &ct;

  m_pipeline = SDL_CreateGPUGraphicsPipeline (m_ctx->gpu_device, &info);

  SDL_ReleaseGPUShader (m_ctx->gpu_device, vs);
  SDL_ReleaseGPUShader (m_ctx->gpu_device, fs);
}

Rml::CompiledGeometryHandle
ui_render_interface::CompileGeometry (Rml::Span<const Rml::Vertex> vertices,
                                      Rml::Span<const int> indices)
{
  ui_geometry geom{};

  SDL_GPUBufferCreateInfo vb{};
  vb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  vb.size = vertices.size () * sizeof (Rml::Vertex);
  geom.vbo = SDL_CreateGPUBuffer (m_ctx->gpu_device, &vb);

  SDL_GPUBufferCreateInfo ib{};
  ib.usage = SDL_GPU_BUFFERUSAGE_INDEX;
  ib.size = indices.size () * sizeof (int);
  geom.ibo = SDL_CreateGPUBuffer (m_ctx->gpu_device, &ib);

  geom.index_count = (uint32_t)indices.size ();

  SDL_GPUTransferBufferCreateInfo const transfer_vertex_buffer_create_info{
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = vb.size, .props = 0
  };

  SDL_GPUTransferBuffer *staging_v = SDL_CreateGPUTransferBuffer (
      m_ctx->gpu_device, &transfer_vertex_buffer_create_info);

  SDL_GPUTransferBufferCreateInfo const transfer_index_buffer_create_info{
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = ib.size, .props = 0
  };

  SDL_GPUTransferBuffer *staging_i = SDL_CreateGPUTransferBuffer (
      m_ctx->gpu_device, &transfer_index_buffer_create_info);

  memcpy (SDL_MapGPUTransferBuffer (m_ctx->gpu_device, staging_v, false),
          vertices.data (), vb.size);
  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, staging_v);

  memcpy (SDL_MapGPUTransferBuffer (m_ctx->gpu_device, staging_i, false),
          indices.data (), ib.size);
  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, staging_i);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTransferBufferLocation const vloc{ staging_v, 0 };
  SDL_GPUBufferRegion const vdst{ geom.vbo, 0, vb.size };

  SDL_GPUTransferBufferLocation const iloc{ staging_i, 0 };
  SDL_GPUBufferRegion const idst{ geom.ibo, 0, ib.size };

  SDL_UploadToGPUBuffer (cp, &vloc, &vdst, true);
  SDL_UploadToGPUBuffer (cp, &iloc, &idst, true);

  SDL_EndGPUCopyPass (cp);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, staging_v);
  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, staging_i);

  auto handle = m_next_geom_handle++;
  m_geometries[handle] = geom;
  return handle;
}

void
ui_render_interface::RenderGeometry (Rml::CompiledGeometryHandle geometry,
                                     Rml::Vector2f translation,
                                     Rml::TextureHandle texture)
{
  (void)translation;
  (void)texture;

  auto it = m_geometries.find (geometry);
  if (it == m_geometries.end ()) {
    return;
}

  auto &g = it->second;

  if ((m_ctx->main_pass == nullptr) || (m_pipeline == nullptr)) {
    return;
}

  SDL_BindGPUGraphicsPipeline (m_ctx->main_pass, m_pipeline);

  SDL_GPUBufferBinding const vb{ g.vbo, 0 };
  SDL_BindGPUVertexBuffers (m_ctx->main_pass, 0, &vb, 1);

  SDL_GPUBufferBinding const ib{ g.ibo, 0 };
  SDL_BindGPUIndexBuffer (m_ctx->main_pass, &ib,
                          SDL_GPU_INDEXELEMENTSIZE_32BIT);

  /*
  if (m_scissor_enabled) {
    SDL_Rect r{m_scissor.x, m_scissor.y, m_scissor.width, m_scissor.height};
    SDL_SetGPUScissor (m_ctx->main_pass, &r);
  }
  */

  SDL_DrawGPUIndexedPrimitives (m_ctx->main_pass, g.index_count, 1, 0, 0, 0);
}

void
ui_render_interface::ReleaseGeometry (Rml::CompiledGeometryHandle geometry)
{
  auto it = m_geometries.find (geometry);
  if (it == m_geometries.end ()) {
    return;
}

  if (it->second.vbo != nullptr) {
    SDL_ReleaseGPUBuffer (m_ctx->gpu_device, it->second.vbo);
}
  if (it->second.ibo != nullptr) {
    SDL_ReleaseGPUBuffer (m_ctx->gpu_device, it->second.ibo);
}

  m_geometries.erase (it);
}

Rml::TextureHandle
ui_render_interface::GenerateTexture (Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i size)
{

  ui_texture tex{};
  tex.width = size.x;
  tex.height = size.y;

  SDL_GPUTextureCreateInfo info{};
  info.width = size.x;
  info.height = size.y;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  tex.texture = SDL_CreateGPUTexture (m_ctx->gpu_device, &info);

  SDL_GPUTransferBufferCreateInfo const transfer_buffer_create_info{
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = (uint32_t)size.x * size.y * 4,
    .props = 0
  };

  // upload
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer (
      m_ctx->gpu_device, &transfer_buffer_create_info);

  memcpy (SDL_MapGPUTransferBuffer (m_ctx->gpu_device, staging, false),
          source.data (), static_cast<size_t>(size.x * size.y * 4));

  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, staging);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTextureTransferInfo const src{
    .transfer_buffer = staging,
    .offset = 0,
    .pixels_per_row = (uint32_t)size.x,
    .rows_per_layer = (uint32_t)size.y,
  };

  // SDL_GPUTransferBufferLocation src{staging, 0};
  SDL_GPUTextureRegion const dst{
    .texture = tex.texture,
    .mip_level = 0,
    .layer = 0,
    .x = 0,
    .y = 0,
    .z = 0,
    .w = (uint32_t)size.x,
    .h = (uint32_t)size.y,
    .d = 1,
  };

  SDL_UploadToGPUTexture (cp, &src, &dst, true);

  SDL_EndGPUCopyPass (cp);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, staging);

  auto handle = m_next_tex_handle++;
  m_textures[handle] = tex;
  return handle;
}

Rml::TextureHandle
ui_render_interface::LoadTexture (Rml::Vector2i &texture_dimensions,
                                  const Rml::String &source)
{
  (void)source;

  // You can plug stb_image here later
  texture_dimensions = { 0, 0 };
  return 0;
}

void
ui_render_interface::ReleaseTexture (Rml::TextureHandle texture)
{
  auto it = m_textures.find (texture);
  if (it == m_textures.end ()) {
    return;
}

  SDL_ReleaseGPUTexture (m_ctx->gpu_device, it->second.texture);
  m_textures.erase (it);
}

void
ui_render_interface::EnableScissorRegion (bool enable)
{
  m_scissor_enabled = enable;
}

void
ui_render_interface::SetScissorRegion (Rml::Rectanglei region)
{
  m_scissor = region;
}

} // namespace wsl
