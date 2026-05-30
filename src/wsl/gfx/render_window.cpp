// window.cpp - HDR + Bloom + Tonemap (SDL_gpu3)
// NOTE: Requires these shaders compiled:
//   compiled_shaders/fullscreen.vert.slang.spv
//   compiled_shaders/bloom_downsample.frag.slang.spv
//   compiled_shaders/bloom_blur.frag.slang.spv
//   compiled_shaders/composite_tonemap.frag.slang.spv
//
// And your 3D pipelines (PBR/skybox/unlit) must render into 2 HDR targets
// (SV_Target0 scene, SV_Target1 bloom).

#include "render_window.hpp"
#include "wsl/log/log.hpp"

#include "wsl/gfx/shader.hpp" // Shader loader, also used in scene_renderer.cpp.
#include "wsl/rsc/resource_manager.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_pixels.h>

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <cstdint>
#include <sys/types.h>

namespace wsl
{

namespace gfx
{

SDL_GPUSampler *
render_window::ensure_linear_sampler ()
{
  if (linear_sampler != nullptr) {
    return linear_sampler;
  }
  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = SDL_GPU_FILTER_LINEAR;
  si.mag_filter = SDL_GPU_FILTER_LINEAR;
  si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  linear_sampler = SDL_CreateGPUSampler (ctx->gpu_device, &si);
  return linear_sampler;
}

void
render_window::destroy_texture (SDL_GPUTexture *&texture) const
{
  if (texture != nullptr) {
    SDL_ReleaseGPUTexture (ctx->gpu_device, texture);
    texture = nullptr;
  }
}

SDL_GPUGraphicsPipeline *
render_window::create_fullscreen_pipe (const char *frag_shader_path,
                                       SDL_GPUTextureFormat out_format,
                                       int num_uniform_buffers,
                                       int num_samplers)
{
  // Fullscreen triangle VS, no vertex buffers.
  auto vert_id = m_res_mgr->register_shader (
      "engine://compiled_shaders/fullscreen.vert.slang.spv");
  SDL_GPUShader *vert = wsl::gfx::shader::load_from_manager (
      ctx->gpu_device, m_res_mgr, vert_id, SDL_GPU_SHADERSTAGE_VERTEX,
      /*num_uniform_buffers=*/0,
      /*num_samplers=*/0);

  auto frag_id = m_res_mgr->register_shader (frag_shader_path);
  SDL_GPUShader *frag = wsl::gfx::shader::load_from_manager (
      ctx->gpu_device, m_res_mgr, frag_id, SDL_GPU_SHADERSTAGE_FRAGMENT,
      /*num_uniform_buffers=*/num_uniform_buffers,
      /*num_samplers=*/num_samplers);

  if ((vert == nullptr) || (frag == nullptr)) {
    if (vert != nullptr) {
      SDL_ReleaseGPUShader (ctx->gpu_device, vert);
    }
    if (frag != nullptr) {
      SDL_ReleaseGPUShader (ctx->gpu_device, frag);
    }
    return nullptr;
  }

  SDL_GPUGraphicsPipelineCreateInfo pipe{};
  SDL_zero (pipe);

  pipe.vertex_shader = vert;
  pipe.fragment_shader = frag;
  pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

  pipe.depth_stencil_state.enable_depth_test = false;
  pipe.depth_stencil_state.enable_depth_write = false;
  pipe.depth_stencil_state.enable_stencil_test = false;

  pipe.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipe.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

  pipe.vertex_input_state.num_vertex_buffers = 0;
  pipe.vertex_input_state.vertex_buffer_descriptions = nullptr;
  pipe.vertex_input_state.num_vertex_attributes = 0;
  pipe.vertex_input_state.vertex_attributes = nullptr;

  SDL_GPUColorTargetDescription ctd{};
  SDL_zero (ctd);
  ctd.format = out_format;

  pipe.target_info.num_color_targets = 1;
  pipe.target_info.color_target_descriptions = &ctd;

  // no depth attachment in these passes
  pipe.target_info.has_depth_stencil_target = false;

  pipe.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  pipe.multisample_state.sample_mask = 0;
  pipe.multisample_state.enable_mask = false;

  SDL_GPUGraphicsPipeline *out
      = SDL_CreateGPUGraphicsPipeline (ctx->gpu_device, &pipe);

  SDL_ReleaseGPUShader (ctx->gpu_device, vert);
  SDL_ReleaseGPUShader (ctx->gpu_device, frag);

  return out;
}

SDL_GPUGraphicsPipeline *
render_window::create_composite_pipe ()
{
  // composite outputs to swapchain format (LDR)
  SDL_GPUTextureFormat const sc_fmt
      = SDL_GetGPUSwapchainTextureFormat (ctx->gpu_device, handler);

  return create_fullscreen_pipe (
      "engine://compiled_shaders/composite_tonemap.frag.slang.spv", sc_fmt,
      /*num_uniform_buffers=*/1, /*Composite cbuffer*/
      /*num_samplers=*/2 /*scene + bloom*/);
}

SDL_GPUGraphicsPipeline *
render_window::create_downsample_pipe ()
{
  return create_fullscreen_pipe (
      "engine://compiled_shaders/bloom_downsample.frag.slang.spv",
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
      /*num_uniform_buffers=*/1, /*Downsample cbuffer*/
      /*num_samplers=*/1);
}

SDL_GPUGraphicsPipeline *
render_window::create_blur_pipe ()
{
  return create_fullscreen_pipe (
      "engine://compiled_shaders/bloom_blur.frag.slang.spv",
      SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
      /*num_uniform_buffers=*/1, /*Blur cbuffer*/
      /*num_samplers=*/1);
}

render_window::render_window (const char *name, int width, int height,
                              wsl::gfx::render_context *ctx,
                              wsl::rsc::resource_manager *res_mgr,
                              bool headless)
    : ctx (ctx), m_res_mgr (res_mgr)
{
  if (headless) {
    wsl::log::gfx ()->debug ("Headless mode, skipping window creation");
    return;
  }

  handler = SDL_CreateWindow (name, width, height, SDL_WINDOW_RESIZABLE);
  SDL_ShowWindow (handler);
  SDL_ClaimWindowForGPUDevice (ctx->gpu_device, handler);
  swapchain_format
      = SDL_GetGPUSwapchainTextureFormat (ctx->gpu_device, handler);

  wsl::log::gfx ()->debug ("Window: {} ({}x{}), swapchain format={:#x}", name,
                           width, height,
                           static_cast<unsigned> (swapchain_format));

  // Allocate initial resources based on current size.
  on_resize ();

  // Create post-process pipelines (once).
  pipe_downsample = create_downsample_pipe ();
  pipe_blur = create_blur_pipe ();
  pipe_composite = create_composite_pipe ();
}

render_window::~render_window ()
{
  if (ctx->gpu_device == nullptr) {
    if (handler != nullptr)
      SDL_DestroyWindow (handler);
    return;
  }

  SDL_WaitForGPUIdle (ctx->gpu_device);

  if (pipe_downsample != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline (ctx->gpu_device, pipe_downsample);
  }
  if (pipe_blur != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline (ctx->gpu_device, pipe_blur);
  }
  if (pipe_composite != nullptr) {
    SDL_ReleaseGPUGraphicsPipeline (ctx->gpu_device, pipe_composite);
  }

  if (linear_sampler != nullptr) {
    SDL_ReleaseGPUSampler (ctx->gpu_device, linear_sampler);
  }

  destroy_texture (bloom_a);
  destroy_texture (bloom_b);
  destroy_texture (hdr_scene);
  destroy_texture (hdr_bloom_src);
  destroy_texture (msaa_hdr_scene);
  destroy_texture (msaa_hdr_bloom);

  destroy_texture (depth_texture);
  destroy_texture (present_tex.texture_data);

  SDL_WaitForGPUIdle (ctx->gpu_device);

  SDL_ReleaseWindowFromGPUDevice (ctx->gpu_device, handler);
  SDL_DestroyWindow (handler);
  SDL_PumpEvents ();
}

void
render_window::get_size (uint32_t &width, uint32_t &height) const
{
  if (handler == nullptr) {
    width = 0;
    height = 0;
    return;
  }

  int w;
  int h;
  SDL_GetWindowSize (handler, &w, &h);
  width = (uint32_t)w;
  height = (uint32_t)h;
}

void
render_window::get_size (int &width, int &height) const
{
  if (handler == nullptr) {
    width = 0;
    height = 0;
    return;
  }
  SDL_GetWindowSize (handler, &width, &height);
}

void
render_window::create_depth_texture ()
{
  if (ctx->gpu_device == nullptr || handler == nullptr)
    return;

  destroy_texture (depth_texture);

  int w;
  int h;
  SDL_GetWindowSizeInPixels (handler, &w, &h);

  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = depth_format;
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  info.width = (uint32_t)w;
  info.height = (uint32_t)h;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.sample_count = SDL_GPU_SAMPLECOUNT_4; // MSAA matches MRT pass

  depth_texture = SDL_CreateGPUTexture (ctx->gpu_device, &info);
}

void
render_window::begin_3d_pass () const
{
  SDL_GPUColorTargetInfo ct[2]{};

  // Scene HDR
  ct[0].texture = msaa_hdr_scene;
  ct[0].load_op = SDL_GPU_LOADOP_CLEAR;
  ct[0].store_op = SDL_GPU_STOREOP_RESOLVE;
  ct[0].clear_color = scene_clear_color;
  ct[0].resolve_texture = hdr_scene;

  // Bloom source HDR
  ct[1].texture = msaa_hdr_bloom;
  ct[1].load_op = SDL_GPU_LOADOP_CLEAR;
  ct[1].store_op = SDL_GPU_STOREOP_RESOLVE;
  ct[1].clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
  ct[1].resolve_texture = hdr_bloom_src;

  SDL_GPUDepthStencilTargetInfo ds{};
  SDL_zero (ds);
  ds.texture = depth_texture;
  ds.clear_depth = 1.0F;
  ds.load_op = SDL_GPU_LOADOP_CLEAR;
  ds.store_op = SDL_GPU_STOREOP_STORE;

  ctx->begin_main_render_pass (ct, 2, &ds);
}

void
render_window::end_3d_pass ()
{
  if (ctx->has_main_render_pass ()) {
    ctx->end_main_render_pass ();
  }

  // Always build present_tex (Game View samples this).
  // Only also write to swapchain if present_to_swapchain is true.
  postprocess_hdr_bloom ();
}

void
render_window::begin_ui_pass () const
{
  ctx->begin_ui_render_pass (swapchain.texture_data);
}

void
render_window::end_ui_pass () const
{
  ctx->end_ui_render_pass ();
}

void
render_window::new_swapchain ()
{
  SDL_WaitAndAcquireGPUSwapchainTexture (ctx->main_cmd, handler,
                                         &swapchain.texture_data,
                                         &swapchain.width, &swapchain.height);
}

void
render_window::on_resize ()
{
  int w;
  int h;
  SDL_GetWindowSizeInPixels (handler, &w, &h);

  if (w <= 0 || h <= 0) {
    return;
  }

  wsl::log::gfx ()->debug ("Resize: {}x{}", w, h);

  SDL_WaitForGPUIdle (ctx->gpu_device);

  // Depth
  create_depth_texture ();

  // Destroy HDR/bloom targets
  destroy_texture (msaa_hdr_scene);
  destroy_texture (msaa_hdr_bloom);
  destroy_texture (hdr_scene);
  destroy_texture (hdr_bloom_src);
  destroy_texture (bloom_a);
  destroy_texture (bloom_b);
  destroy_texture (present_tex.texture_data);

  // MSAA HDR targets (two MRT textures)
  SDL_GPUTextureCreateInfo msaa{};
  SDL_zero (msaa);
  msaa.type = SDL_GPU_TEXTURETYPE_2D;
  msaa.width = (uint32_t)w;
  msaa.height = (uint32_t)h;
  msaa.layer_count_or_depth = 1;
  msaa.num_levels = 1;
  msaa.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  msaa.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  msaa.sample_count = SDL_GPU_SAMPLECOUNT_4;

  msaa_hdr_scene = SDL_CreateGPUTexture (ctx->gpu_device, &msaa);
  msaa_hdr_bloom = SDL_CreateGPUTexture (ctx->gpu_device, &msaa);

  // Resolved HDR targets must be sampler + color target
  SDL_GPUTextureCreateInfo res = msaa;
  res.sample_count = SDL_GPU_SAMPLECOUNT_1;
  res.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  hdr_scene = SDL_CreateGPUTexture (ctx->gpu_device, &res);
  hdr_bloom_src = SDL_CreateGPUTexture (ctx->gpu_device, &res);

  // Half-res bloom ping-pong
  SDL_GPUTextureCreateInfo half = res;
  half.width = (uint32_t)std::max (1, w / 2);
  half.height = (uint32_t)std::max (1, h / 2);

  bloom_a = SDL_CreateGPUTexture (ctx->gpu_device, &half);
  bloom_b = SDL_CreateGPUTexture (ctx->gpu_device, &half);

  // Create LDR output texture (same format as swapchain) so UI can sample it
  SDL_GPUTextureCreateInfo out{};
  SDL_zero (out);
  out.type = SDL_GPU_TEXTURETYPE_2D;
  out.width = (uint32_t)w;
  out.height = (uint32_t)h;
  out.layer_count_or_depth = 1;
  out.num_levels = 1;
  out.sample_count = SDL_GPU_SAMPLECOUNT_1;
  out.format = swapchain_format;
  out.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  present_tex.texture_data = SDL_CreateGPUTexture (ctx->gpu_device, &out);
  present_tex.width = (uint32_t)w;
  present_tex.height = (uint32_t)h;
}

void
render_window::postprocess_hdr_bloom ()
{
  if ((hdr_bloom_src == nullptr) || (hdr_scene == nullptr)
      || (bloom_a == nullptr) || (bloom_b == nullptr)
      || (present_tex.texture_data == nullptr)) {
    return;
  }

  ensure_linear_sampler ();

  // Pipelines must exist (created in ctor). If shader compilation failed,
  // skip post to avoid crashing.
  if ((pipe_downsample == nullptr) || (pipe_blur == nullptr)
      || (pipe_composite == nullptr)) {
    return;
  }

  int ww;
  int hh;
  SDL_GetWindowSizeInPixels (handler, &ww, &hh);

  // ---------- (1) Downsample bloom_src -> bloom_a ----------
  {
    SDL_GPUColorTargetInfo ct{};
    SDL_zero (ct);
    ct.texture = bloom_a;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = { 0, 0, 0, 1 };

    SDL_GPURenderPass *pass
        = SDL_BeginGPURenderPass (ctx->main_cmd, &ct, 1, nullptr);
    if (pass != nullptr) {
      SDL_BindGPUGraphicsPipeline (pass, pipe_downsample);

      struct alignas (16) down
      {
        float texel[2];
        float pad[2];
      } p{};
      p.texel[0] = 1.0F / float (ww);
      p.texel[1] = 1.0F / float (hh);

      SDL_PushGPUFragmentUniformData (ctx->main_cmd, 0, &p, sizeof (p));

      SDL_GPUTextureSamplerBinding b{};
      SDL_zero (b);
      b.texture = hdr_bloom_src;
      b.sampler = linear_sampler;
      SDL_BindGPUFragmentSamplers (pass, 0, &b, 1);

      SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);
      SDL_EndGPURenderPass (pass);
    }
  }

  // bloom texel size (half res)
  uint32_t const bw = std::max (1U, (uint32_t)ww / 2);
  uint32_t const bh = std::max (1U, (uint32_t)hh / 2);
  float const bloom_texel[2] = { 1.0F / float (bw), 1.0F / float (bh) };

  // ---------- (2) Blur H: bloom_a -> bloom_b ----------
  {
    SDL_GPUColorTargetInfo ct{};
    SDL_zero (ct);
    ct.texture = bloom_b;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = { 0, 0, 0, 1 };

    SDL_GPURenderPass *pass
        = SDL_BeginGPURenderPass (ctx->main_cmd, &ct, 1, nullptr);
    if (pass != nullptr) {
      SDL_BindGPUGraphicsPipeline (pass, pipe_blur);

      struct alignas (16) blur
      {
        float texel[2];
        float dir[2];
      } p{};
      p.texel[0] = bloom_texel[0];
      p.texel[1] = bloom_texel[1];
      p.dir[0] = 1.0F;
      p.dir[1] = 0.0F;

      SDL_PushGPUFragmentUniformData (ctx->main_cmd, 0, &p, sizeof (p));

      SDL_GPUTextureSamplerBinding b{};
      SDL_zero (b);
      b.texture = bloom_a;
      b.sampler = linear_sampler;
      SDL_BindGPUFragmentSamplers (pass, 0, &b, 1);

      SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);
      SDL_EndGPURenderPass (pass);
    }
  }

  // ---------- (3) Blur V: bloom_b -> bloom_a ----------
  {
    SDL_GPUColorTargetInfo ct{};
    SDL_zero (ct);
    ct.texture = bloom_a;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = { 0, 0, 0, 1 };

    SDL_GPURenderPass *pass
        = SDL_BeginGPURenderPass (ctx->main_cmd, &ct, 1, nullptr);
    if (pass != nullptr) {
      SDL_BindGPUGraphicsPipeline (pass, pipe_blur);

      struct alignas (16) blur
      {
        float texel[2];
        float dir[2];
      } p{};
      p.texel[0] = bloom_texel[0];
      p.texel[1] = bloom_texel[1];
      p.dir[0] = 0.0F;
      p.dir[1] = 1.0F;

      SDL_PushGPUFragmentUniformData (ctx->main_cmd, 0, &p, sizeof (p));

      SDL_GPUTextureSamplerBinding b{};
      SDL_zero (b);
      b.texture = bloom_b;
      b.sampler = linear_sampler;
      SDL_BindGPUFragmentSamplers (pass, 0, &b, 1);

      SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);
      SDL_EndGPURenderPass (pass);
    }
  }

  // ---------- (4) Composite + tonemap: hdr_scene + bloom_a -> swapchain
  // ----------
  if (swapchain.texture_data != nullptr) {
    SDL_GPUColorTargetInfo ct{};
    SDL_zero (ct);
    ct.texture = swapchain.texture_data;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = { 0, 0, 0, 1 };

    SDL_GPURenderPass *pass
        = SDL_BeginGPURenderPass (ctx->main_cmd, &ct, 1, nullptr);
    if (pass != nullptr) {
      SDL_BindGPUGraphicsPipeline (pass, pipe_composite);

      struct alignas (16) comp
      {
        float exposure;
        float bloom_int;
        float pad[2];
      } c{};
      c.exposure = exposure;
      c.bloom_int = bloom_intensity;

      SDL_PushGPUFragmentUniformData (ctx->main_cmd, 0, &c, sizeof (c));

      SDL_GPUTextureSamplerBinding b[2]{};
      SDL_zero (b);
      b[0].texture = hdr_scene;
      b[0].sampler = linear_sampler;
      b[1].texture = bloom_a;
      b[1].sampler = linear_sampler;

      SDL_BindGPUFragmentSamplers (pass, 0, b, 2);

      SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);
      SDL_EndGPURenderPass (pass);
    }
  }
  // ---------- (5) Composite + tonemap also into present_tex (sampleable)
  // ----------
  if (present_tex.texture_data != nullptr) {
    SDL_GPUColorTargetInfo ct{};
    SDL_zero (ct);
    ct.texture = present_tex.texture_data;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = { 0, 0, 0, 1 };

    SDL_GPURenderPass *pass
        = SDL_BeginGPURenderPass (ctx->main_cmd, &ct, 1, nullptr);
    if (pass != nullptr) {
      SDL_BindGPUGraphicsPipeline (pass, pipe_composite);

      struct alignas (16) comp
      {
        float exposure;
        float bloom_int;
        float pad[2];
      } c{};
      c.exposure = exposure;
      c.bloom_int = bloom_intensity;

      SDL_PushGPUFragmentUniformData (ctx->main_cmd, 0, &c, sizeof (c));

      SDL_GPUTextureSamplerBinding b[2]{};
      SDL_zero (b);
      b[0].texture = hdr_scene;
      b[0].sampler = linear_sampler;
      b[1].texture = bloom_a;
      b[1].sampler = linear_sampler;

      SDL_BindGPUFragmentSamplers (pass, 0, b, 2);

      SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);
      SDL_EndGPURenderPass (pass);
    }
  }
}

} // namespace gfx

} // namespace wsl
