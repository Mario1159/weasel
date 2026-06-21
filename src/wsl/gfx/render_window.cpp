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

#include <tracy/Tracy.hpp>
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
                              bool headless, bool try_disable_vsync_on_startup)
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

  // Default behavior: try to disable vsync so the triple-buffered
  // pipeline can actually overlap frames. The change is wrapped in
  // `set_vsync`, which spdlog-logs and bails out (keeping VSYNC) if
  // the backend refuses. The known AMDVK / Mesa driver-side bug —
  // where `SDL_WindowSupportsGPUPresentMode` and
  // `SDL_SetGPUSwapchainParameters` both return success for
  // IMMEDIATE/MAILBOX, but the next compute dispatch segfaults —
  // is documented on `set_vsync`. AMDVK users should pass
  // `try_disable_vsync_on_startup = false` to skip this attempt, or
  // call `set_vsync(true)` immediately after construction.
  if (try_disable_vsync_on_startup) {
    if (!set_vsync (false)) {
      wsl::log::gfx ()->error (
          "render_window: failed to disable vsync on startup, "
          "swapchain remains vsync-paced (VSYNC).");
    }
  }

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

bool
render_window::set_vsync (bool enabled)
{
  if (handler == nullptr || ctx->gpu_device == nullptr) {
    wsl::log::gfx ()->error ("render_window::set_vsync: no window or device");
    return false;
  }

  // Pick the requested present mode based on the desired vsync state
  // and what the backend actually supports. We re-query support on
  // every call because the user may have moved the window to a
  // different monitor with different capabilities between toggles.
  SDL_GPUPresentMode requested;
  if (enabled) {
    // vsync ON: prefer MAILBOX (lower-latency vsync, drops pending
    // images instead of queueing them) when available, fall back to
    // plain VSYNC otherwise.
    if (SDL_WindowSupportsGPUPresentMode (ctx->gpu_device, handler,
                                          SDL_GPU_PRESENTMODE_MAILBOX)) {
      requested = SDL_GPU_PRESENTMODE_MAILBOX;
    } else {
      requested = SDL_GPU_PRESENTMODE_VSYNC;
    }
  } else {
    // vsync OFF: IMMEDIATE (no vblank sync). Bail with a spdlog error
    // if the backend doesn't advertise it — we'd rather keep the
    // previous mode than call `SDL_SetGPUSwapchainParameters` with a
    // mode that's known to fail.
    if (!SDL_WindowSupportsGPUPresentMode (ctx->gpu_device, handler,
                                           SDL_GPU_PRESENTMODE_IMMEDIATE)) {
      wsl::log::gfx ()->error (
          "render_window::set_vsync: IMMEDIATE present mode not "
          "supported by the backend, vsync stays enabled");
      return false;
    }
    requested = SDL_GPU_PRESENTMODE_IMMEDIATE;
  }

  if (!SDL_SetGPUSwapchainParameters (ctx->gpu_device, handler,
                                      m_swapchain_composition, requested)) {
    wsl::log::gfx ()->error (
        "render_window::set_vsync: SDL_SetGPUSwapchainParameters "
        "rejected the request ({})",
        SDL_GetError ());
    return false;
  }

  m_vsync = enabled;
  char const *mode_name
      = (requested == SDL_GPU_PRESENTMODE_VSYNC)       ? "VSYNC"
        : (requested == SDL_GPU_PRESENTMODE_MAILBOX)   ? "MAILBOX"
        : (requested == SDL_GPU_PRESENTMODE_IMMEDIATE) ? "IMMEDIATE"
                                                       : "UNKNOWN";
  wsl::log::gfx ()->info ("render_window::set_vsync: vsync {} (mode={})",
                          enabled ? "ON" : "OFF", mode_name);
  return true;
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
  if (depth_texture == nullptr) {
    wsl::log::gfx ()->error ("Failed to create depth texture: {}",
                             SDL_GetError ());
  }
}

void
render_window::begin_3d_pass (bool clear_color, bool clear_depth) const
{
  ZoneScoped;
  if ((msaa_hdr_scene == nullptr) || (msaa_hdr_bloom == nullptr)
      || (hdr_scene == nullptr) || (hdr_bloom_src == nullptr)
      || (depth_texture == nullptr)) {
    wsl::log::gfx ()->warn (
        "begin_3d_pass: null render target texture(s), skipping");
    return;
  }

  SDL_GPUColorTargetInfo ct[2]{};

  // Scene HDR
  ct[0].texture = msaa_hdr_scene;
  ct[0].load_op = clear_color ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
  ct[0].store_op = SDL_GPU_STOREOP_RESOLVE;
  ct[0].clear_color = scene_clear_color;
  ct[0].resolve_texture = hdr_scene;

  // Bloom source HDR
  ct[1].texture = msaa_hdr_bloom;
  ct[1].load_op = clear_color ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
  ct[1].store_op = SDL_GPU_STOREOP_RESOLVE;
  ct[1].clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
  ct[1].resolve_texture = hdr_bloom_src;

  SDL_GPUDepthStencilTargetInfo ds{};
  SDL_zero (ds);
  ds.texture = depth_texture;
  ds.clear_depth = 1.0F;
  ds.load_op = clear_depth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
  ds.store_op = SDL_GPU_STOREOP_STORE;

  ctx->begin_main_render_pass (ct, 2, &ds);

  // If a viewport is active, set it on the render pass.
  if (has_active_viewport ()) {
    auto vp = current_viewport ();
    int w, h;
    SDL_GetWindowSizeInPixels (handler, &w, &h);
    auto pix
        = vp.to_pixels (static_cast<uint32_t> (w), static_cast<uint32_t> (h));

    SDL_GPUViewport gpu_vp{};
    gpu_vp.x = static_cast<float> (pix.x);
    gpu_vp.y = static_cast<float> (pix.y);
    gpu_vp.w = static_cast<float> (pix.width);
    gpu_vp.h = static_cast<float> (pix.height);
    gpu_vp.min_depth = vp.min_depth;
    gpu_vp.max_depth = vp.max_depth;
    ctx->set_viewport (gpu_vp);

    SDL_Rect scissor{};
    scissor.x = pix.x;
    scissor.y = pix.y;
    scissor.w = pix.width;
    scissor.h = pix.height;
    ctx->set_scissor_rect (scissor);
  }
}

void
render_window::end_3d_pass (bool run_postprocess)
{
  ZoneScoped;
  if (ctx->has_main_render_pass ()) {
    ctx->end_main_render_pass ();
  }

  if (run_postprocess) {
    // Always build present_tex (Game View samples this).
    // Only also write to swapchain if present_to_swapchain is true.
    postprocess_hdr_bloom ();
  }
}

void
render_window::begin_ui_pass () const
{
  ZoneScoped;
  ctx->begin_ui_render_pass (swapchain.texture_data);
}

void
render_window::end_ui_pass () const
{
  ZoneScoped;
  ctx->end_ui_render_pass ();
}

void
render_window::push_viewport (const gfx::viewport &vp)
{
  m_viewport_stack.push_back (vp);
}

void
render_window::pop_viewport ()
{
  if (!m_viewport_stack.empty ()) {
    m_viewport_stack.pop_back ();
  }
}

void
render_window::reset_viewports ()
{
  m_viewport_stack.clear ();
}

bool
render_window::has_active_viewport () const
{
  return !m_viewport_stack.empty ();
}

size_t
render_window::viewport_count () const
{
  return m_viewport_stack.size ();
}

gfx::viewport
render_window::current_viewport () const
{
  if (!m_viewport_stack.empty ()) {
    return m_viewport_stack.back ();
  }
  // Default full-screen viewport.
  return gfx::viewport{};
}

void
render_window::apply_viewport (const gfx::viewport &vp) const
{
  int w, h;
  SDL_GetWindowSizeInPixels (handler, &w, &h);
  auto pix
      = vp.to_pixels (static_cast<uint32_t> (w), static_cast<uint32_t> (h));

  SDL_GPUViewport gpu_vp{};
  gpu_vp.x = static_cast<float> (pix.x);
  gpu_vp.y = static_cast<float> (pix.y);
  gpu_vp.w = static_cast<float> (pix.width);
  gpu_vp.h = static_cast<float> (pix.height);
  gpu_vp.min_depth = vp.min_depth;
  gpu_vp.max_depth = vp.max_depth;
  ctx->set_viewport (gpu_vp);

  SDL_Rect scissor{};
  scissor.x = pix.x;
  scissor.y = pix.y;
  scissor.w = pix.width;
  scissor.h = pix.height;
  ctx->set_scissor_rect (scissor);
}

void
render_window::new_swapchain ()
{
  ZoneScoped;
  // Try the non-blocking acquire first. With max-3 frames in flight and
  // IMMEDIATE present mode this is essentially always successful, and
  // when it is the CPU never blocks on the swapchain.
  //
  // The catch: per the SDL docs, when too many frames are in flight
  // `SDL_AcquireGPUSwapchainTexture` returns `true` with
  // `swapchain_texture = NULL` as an "indication to wait". Recording
  // GPU work on a cmd buffer that holds a NULL swapchain acquire is
  // legal but the AMDVK driver is observably fragile about it — the
  // next `SDL_DispatchGPUCompute` (or even the copy pass) segfaults
  // because the cmd buffer's submit-side present is in an
  // inconsistent state. This was the source of the intermittent
  // crashes after the IMMEDIATE change.
  //
  // Critical: we must NOT call the blocking fallback
  // `SDL_WaitAndAcquireGPUSwapchainTexture` on the same `ctx->main_cmd`
  // that already has the NULL acquire recorded — that would leave the
  // command buffer with TWO swapchain acquires (one NULL, one valid)
  // and AMDVK uses the NULL one. The fallback MUST use a fresh command
  // buffer. As a bonus, this also gives us a clean command buffer
  // with exactly one valid swapchain acquire for the rest of the frame.
  constexpr int kSpinAttempts = 4;
  {
    ZoneScopedN ("new_swapchain::non_blocking_spin");
    for (int attempt = 0; attempt < kSpinAttempts; ++attempt) {
      bool const ok = SDL_AcquireGPUSwapchainTexture (
          ctx->main_cmd, handler, &swapchain.texture_data, &swapchain.width,
          &swapchain.height);
      if (ok && swapchain.texture_data != nullptr) {
        return;
      }
      if (ok) {
        // Got `true` but the texture is NULL — too many frames in flight.
        // The non-blocking acquire already left a NULL acquire on
        // `ctx->main_cmd`, so we cannot reuse it for the blocking
        // fallback. Submit the poisoned cmd buffer as a no-op (it has
        // no recorded draws yet — `new_swapchain` is the first call
        // before any pass) and get a fresh one for the blocking acquire.
        {
          ZoneScopedN ("new_swapchain::recover_poison");
          SDL_GPUFence *poison_fence
              = SDL_SubmitGPUCommandBufferAndAcquireFence (ctx->main_cmd);
          if (poison_fence != nullptr) {
            SDL_WaitForGPUFences (ctx->gpu_device, true, &poison_fence, 1);
            SDL_ReleaseGPUFence (ctx->gpu_device, poison_fence);
          }
          ctx->main_cmd = SDL_AcquireGPUCommandBuffer (ctx->gpu_device);
          if (ctx->main_cmd == nullptr) {
            wsl::log::gfx ()->error (
                "new_swapchain: failed to acquire fresh cmd buffer: {}",
                SDL_GetError ());
            return;
          }
        }
        break;
      }
      // `false` is a hard error. Bail out and let the frame render to
      // present_tex only.
      wsl::log::gfx ()->debug (
          "new_swapchain: non-blocking acquire returned false ({}), "
          "falling back to blocking acquire",
          SDL_GetError ());
      break;
    }
  }

  {
    ZoneScopedN ("new_swapchain::blocking_acquire");
    bool const ok = SDL_WaitAndAcquireGPUSwapchainTexture (
        ctx->main_cmd, handler, &swapchain.texture_data, &swapchain.width,
        &swapchain.height);
    if (!ok) {
      wsl::log::gfx ()->error (
          "new_swapchain: blocking acquire also failed: {}", SDL_GetError ());
      swapchain.texture_data = nullptr;
      swapchain.width = 0;
      swapchain.height = 0;
    }
  }
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

  auto create_hdr_target = [&] (const SDL_GPUTextureCreateInfo &ci) {
    SDL_GPUTexture *tex = SDL_CreateGPUTexture (ctx->gpu_device, &ci);
    if (tex == nullptr) {
      wsl::log::gfx ()->error ("Failed to create HDR render target ({}x{}): {}",
                               ci.width, ci.height, SDL_GetError ());
    }
    return tex;
  };

  msaa_hdr_scene = create_hdr_target (msaa);
  msaa_hdr_bloom = create_hdr_target (msaa);

  // Resolved HDR targets must be sampler + color target
  SDL_GPUTextureCreateInfo res = msaa;
  res.sample_count = SDL_GPU_SAMPLECOUNT_1;
  res.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  hdr_scene = create_hdr_target (res);
  hdr_bloom_src = create_hdr_target (res);

  // Half-res bloom ping-pong
  SDL_GPUTextureCreateInfo half = res;
  half.width = (uint32_t)std::max (1, w / 2);
  half.height = (uint32_t)std::max (1, h / 2);

  bloom_a = create_hdr_target (half);
  bloom_b = create_hdr_target (half);

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

  present_tex.texture_data = create_hdr_target (out);
  present_tex.width = (uint32_t)w;
  present_tex.height = (uint32_t)h;
}

void
render_window::postprocess_hdr_bloom ()
{
  ZoneScoped;
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
    ZoneScopedN ("postprocess::downsample");
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
    ZoneScopedN ("postprocess::blur_h");
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
    ZoneScopedN ("postprocess::blur_v");
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
    ZoneScopedN ("postprocess::composite_swapchain");
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
    ZoneScopedN ("postprocess::composite_present_tex");
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
