#include "render_context.hpp"
#include "spdlog/spdlog.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>


namespace wsl
{

gfx::render_context::render_context ()
{
  int driver_count = SDL_GetNumGPUDrivers ();
  spdlog::debug ("render_context: found {} GPU drivers", driver_count);

  for (int i = 0; i < driver_count; i++) {
    const char *driver_name = SDL_GetGPUDriver (i);
    gpu_device = SDL_CreateGPUDevice (SDL_GPU_SHADERFORMAT_SPIRV, false, driver_name);
    if (gpu_device != nullptr) {
      spdlog::info ("render_context: created GPU device using driver: {}", driver_name);
      break;
    }
    spdlog::warn ("render_context: failed to create GPU device with driver {}: {}", driver_name, SDL_GetError ());
  }

  if (gpu_device == nullptr) {
    // Try default with fallback formats
    gpu_device = SDL_CreateGPUDevice (SDL_GetGPUShaderFormats (nullptr), false, nullptr);
    if (gpu_device != nullptr) {
      spdlog::info ("render_context: created GPU device using default driver and fallback formats");
    }
  }
  
  if (gpu_device == nullptr) {
    SDL_Log ("render_context: CRITICAL: failed to create GPU device: %s", SDL_GetError ());
    spdlog::critical ("render_context: CRITICAL: failed to create GPU device: {}", SDL_GetError ());
  }
  spdlog::debug ("GPU DEVICE {}", (void *)gpu_device);
}

gfx::render_context::~render_context () { SDL_DestroyGPUDevice (gpu_device); }

SDL_GPUDevice *
gfx::render_context::device () const
{
  return gpu_device;
}

SDL_GPUCommandBuffer *
gfx::render_context::command_buffer () const
{
  return main_cmd;
}

SDL_GPURenderPass *
gfx::render_context::main_render_pass () const
{
  return main_pass;
}

SDL_GPURenderPass *
gfx::render_context::ui_render_pass () const
{
  return ui_pass;
}

bool
gfx::render_context::has_active_frame () const
{
  return main_cmd != nullptr;
}

bool
gfx::render_context::has_main_render_pass () const
{
  return main_pass != nullptr;
}

bool
gfx::render_context::has_ui_render_pass () const
{
  return ui_pass != nullptr;
}

void
gfx::render_context::begin_frame ()
{
  main_cmd = SDL_AcquireGPUCommandBuffer (gpu_device);
}

void
gfx::render_context::submit_frame ()
{
  if (main_cmd == nullptr) {
    return;
  }

  if (main_pass != nullptr) {
    end_main_render_pass ();
  }

  if (ui_pass != nullptr) {
    end_ui_render_pass ();
  }

  SDL_SubmitGPUCommandBuffer (main_cmd);
  main_cmd = nullptr;
}

SDL_GPURenderPass *
gfx::render_context::begin_render_pass (
    const SDL_GPUColorTargetInfo *color_targets, Uint32 num_color_targets,
    const SDL_GPUDepthStencilTargetInfo *depth_target) const
{
  return SDL_BeginGPURenderPass (main_cmd, color_targets, num_color_targets,
                                 depth_target);
}

void
gfx::render_context::begin_main_render_pass (
    const SDL_GPUColorTargetInfo *color_targets, Uint32 num_color_targets,
    const SDL_GPUDepthStencilTargetInfo *depth_target)
{
  main_pass
      = begin_render_pass (color_targets, num_color_targets, depth_target);
}

void
gfx::render_context::end_main_render_pass ()
{
  if (main_pass == nullptr) {
    return;
  }

  SDL_EndGPURenderPass (main_pass);
  main_pass = nullptr;
}

void
gfx::render_context::begin_cmd ()
{
  begin_frame ();
}

void
gfx::render_context::end_cmd ()
{
  submit_frame ();
}

void
gfx::render_context::begin_clear_render_pass (SDL_GPUTexture *swapchain,
                                              SDL_GPUTexture *depth)
{
  SDL_GPUColorTargetInfo color_info{};
  color_info.texture = swapchain;
  color_info.load_op = SDL_GPU_LOADOP_CLEAR;
  color_info.clear_color = { 0, 0, 0, 1 };
  color_info.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth_info{};
  depth_info.texture = depth;
  depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
  depth_info.clear_depth = 1.0F;
  depth_info.store_op = SDL_GPU_STOREOP_STORE;

  begin_main_render_pass (&color_info, 1, &depth_info);
}

void
gfx::render_context::begin_clear_render_pass (
    SDL_GPUTexture *color_msaa,     // MSAA target
    SDL_GPUTexture *resolve_target, // resolved 1x target
    SDL_GPUTexture *depth)          // depth (MSAA)
{
  SDL_GPUColorTargetInfo color_info{};
  color_info.texture = color_msaa; // 4x MSAA
  color_info.load_op = SDL_GPU_LOADOP_CLEAR;
  color_info.clear_color = { 0, 0, 0, 1 };

  color_info.store_op = SDL_GPU_STOREOP_RESOLVE; // resolve to single-sample
  color_info.resolve_texture = resolve_target;

  SDL_GPUDepthStencilTargetInfo depth_info{};
  depth_info.texture = depth; // MSAA depth
  depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
  depth_info.clear_depth = 1.0F;
  depth_info.store_op = SDL_GPU_STOREOP_STORE;

  begin_main_render_pass (&color_info, 1, &depth_info);
}

void
gfx::render_context::end_clear_render_pass ()
{
  end_main_render_pass ();
}

void
gfx::render_context::begin_ui_render_pass (SDL_GPUTexture *swapchain)
{
  SDL_GPUColorTargetInfo target_info = {};
  target_info.texture = swapchain;
  target_info.clear_color = { 0.0F, 0.0F, 0.0F, 0.0F };
  target_info.load_op = SDL_GPU_LOADOP_LOAD;
  target_info.store_op = SDL_GPU_STOREOP_STORE;
  target_info.mip_level = 0;
  target_info.layer_or_depth_plane = 0;
  target_info.cycle = false;
  ui_pass = begin_render_pass (&target_info, 1, nullptr);
}

void
gfx::render_context::end_ui_render_pass ()
{
  SDL_EndGPURenderPass (ui_pass);
  ui_pass = nullptr;
}

} // namespace wsl
