#include "render_context.hpp"
#ifdef WEASEL_ENABLE_RENDERDOC
#include "renderdoc.hpp"
#endif
#include "wsl/log/log.hpp"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <tracy/Tracy.hpp>

namespace wsl
{

namespace
{

// Try to create an SDL_GPUDevice for the named driver. For the Vulkan
// driver we go through SDL_CreateGPUDeviceWithProperties so that we can
// opt into VK_EXT_debug_utils (which gates the SDL_PushGPUDebugGroup /
// SDL_SetGPUTextureName calls used to annotate RenderDoc captures). For
// every other backend the simple SDL_CreateGPUDevice path is enough.
SDL_GPUDevice *
try_create_device (SDL_GPUShaderFormat shader_format, const char *driver_name)
{
  if (driver_name != nullptr && SDL_strcmp (driver_name, "vulkan") == 0) {
    SDL_PropertiesID props = SDL_CreateProperties ();
    if (props == 0) {
      return nullptr;
    }
    SDL_SetBooleanProperty (props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
                            true);
    SDL_SetStringProperty (props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
                           driver_name);

    // Request VK_EXT_debug_utils so SDL3's debug-group / debug-name
    // calls (which call vkCmdBeginDebugUtilsLabelEXT /
    // vkSetDebugUtilsObjectNameEXT under the hood) are accepted by
    // the Vulkan loader.
    SDL_GPUVulkanOptions vk_opts{};
    static const char *kInstanceExts[] = { "VK_EXT_debug_utils" };
    vk_opts.instance_extension_count = 1;
    vk_opts.instance_extension_names = kInstanceExts;
    SDL_SetPointerProperty (
        props, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &vk_opts);

    SDL_GPUDevice *dev = SDL_CreateGPUDeviceWithProperties (props);
    SDL_DestroyProperties (props);
    return dev;
  }
  return SDL_CreateGPUDevice (shader_format, false, driver_name);
}

} // namespace

gfx::render_context::render_context (bool headless)
{
  if (headless) {
    wsl::log::gfx ()->debug ("Headless mode, skipping GPU device creation");
    return;
  }

#if defined(__APPLE__)
  SDL_GPUShaderFormat shader_format = SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
  SDL_GPUShaderFormat shader_format = SDL_GPU_SHADERFORMAT_DXIL;
#else
  SDL_GPUShaderFormat shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
#endif

  int driver_count = SDL_GetNumGPUDrivers ();
  wsl::log::gfx ()->debug ("Found {} GPU drivers", driver_count);

  for (int i = 0; i < driver_count; i++) {
    const char *driver_name = SDL_GetGPUDriver (i);
    gpu_device = try_create_device (shader_format, driver_name);
    if (gpu_device != nullptr) {
      wsl::log::gfx ()->info ("Created GPU device using driver: {}",
                              driver_name);
      break;
    }
    wsl::log::gfx ()->warn ("Failed to create GPU device with driver {}: {}",
                            driver_name, SDL_GetError ());
  }

  if (gpu_device == nullptr) {
    gpu_device = try_create_device (shader_format, nullptr);
    if (gpu_device != nullptr) {
      wsl::log::gfx ()->info ("Created GPU device using default driver");
    }
  }

  if (gpu_device == nullptr) {
    wsl::log::gfx ()->critical ("Failed to create GPU device: {}",
                                SDL_GetError ());
    return;
  }

  // Log usable GPU properties
  char const *driver_cfg = "<<unknown>>";
  switch (shader_format) {
  case SDL_GPU_SHADERFORMAT_SPIRV:
    driver_cfg = "SPIR-V";
    break;
  case SDL_GPU_SHADERFORMAT_MSL:
    driver_cfg = "MSL";
    break;
  case SDL_GPU_SHADERFORMAT_DXIL:
    driver_cfg = "DXIL";
    break;
  default:
    break;
  }
  wsl::log::gfx ()->debug ("Shader target: {}, format: {}",
                           SDL_GetGPUDeviceDriver (gpu_device), driver_cfg);
}

gfx::render_context::~render_context ()
{
  if (gpu_device != nullptr) {
    SDL_WaitForGPUIdle (gpu_device);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
      if (m_slots[i].fence != nullptr) {
        SDL_ReleaseGPUFence (gpu_device, m_slots[i].fence);
        m_slots[i].fence = nullptr;
      }
    }
    SDL_DestroyGPUDevice (gpu_device);
    gpu_device = nullptr;
    main_cmd = nullptr;
    main_pass = nullptr;
    ui_pass = nullptr;
    SDL_PumpEvents ();
  }
}

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
  ZoneScoped;
  // Reset pass handles BEFORE acquiring a new command buffer.
  // This ensures that if a previous frame's end_main_render_pass was skipped or
  // if begin_main_render_pass is not called this frame, we never hold a stale
  // pass handle across frame boundaries.
  main_pass = nullptr;
  ui_pass = nullptr;

  // Triple buffering: pick the slot for this frame and wait on the fence
  // from the previous time this slot was used. This is what replaces the
  // old vsync stall: instead of blocking the CPU on swapchain present
  // completion every frame, we only block on the GPU's completion of
  // work that consumed this slot's resources. The slot's resources are
  // free to be overwritten as soon as the fence signals, and the CPU
  // can run 1..kMaxFramesInFlight frames ahead of the GPU.
  m_current_slot = static_cast<uint32_t> (m_frame_index % kMaxFramesInFlight);
  FrameSlot &slot = m_slots[m_current_slot];

  if (slot.fence != nullptr) {
    ZoneScopedN ("render_context::wait_for_slot_fence");
    SDL_WaitForGPUFences (gpu_device, false, &slot.fence, 1);
    SDL_ReleaseGPUFence (gpu_device, slot.fence);
    slot.fence = nullptr;
  }

  main_cmd = SDL_AcquireGPUCommandBuffer (gpu_device);
  ++m_frame_index;

  // Stamp the in-progress command buffer with the frame index + wall-clock
  // timestamp. Visible in RenderDoc's Annotation Viewer as children of
  // the `frame.*` namespace. No-op when RenderDoc is not loaded.
#ifdef WEASEL_ENABLE_RENDERDOC
  if (main_cmd != nullptr) {
    wsl::gfx::rdoc::annotate_command<uint64_t> (main_cmd, "frame.index",
                                                (uint64_t)m_frame_index);
    wsl::gfx::rdoc::annotate_command<uint64_t> (main_cmd, "frame.ticks_ms",
                                                (uint64_t)SDL_GetTicks ());
  }
#endif
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

  // Acquire a fence at submit so the next time this slot is recycled
  // (kMaxFramesInFlight frames from now) the CPU can wait for the GPU
  // to finish consuming the resources this command buffer used.
  FrameSlot &slot = m_slots[m_current_slot];
  slot.fence = SDL_SubmitGPUCommandBufferAndAcquireFence (main_cmd);
  if (slot.fence == nullptr) {
    wsl::log::gfx ()->warn (
        "render_context: SDL_SubmitGPUCommandBufferAndAcquireFence failed: {}",
        SDL_GetError ());
  }
  main_cmd = nullptr;
}

SDL_GPUFence *
gfx::render_context::current_fence () const
{
  return m_slots[m_current_slot].fence;
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
  if (swapchain == nullptr) {
    ui_pass = nullptr;
    return;
  }
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
  if (ui_pass == nullptr) {
    return;
  }
  SDL_EndGPURenderPass (ui_pass);
  ui_pass = nullptr;
}

void
gfx::render_context::set_viewport (const SDL_GPUViewport &vp)
{
  if (main_pass != nullptr) {
    SDL_SetGPUViewport (main_pass, &vp);
  }
}

void
gfx::render_context::set_scissor_rect (const SDL_Rect &rect)
{
  if (main_pass != nullptr) {
    SDL_SetGPUScissor (main_pass, &rect);
  }
}

void
gfx::render_context::reset_viewport (uint32_t width, uint32_t height)
{
  SDL_GPUViewport vp{};
  vp.x = 0.0F;
  vp.y = 0.0F;
  vp.w = static_cast<float> (width);
  vp.h = static_cast<float> (height);
  vp.min_depth = 0.0F;
  vp.max_depth = 1.0F;
  set_viewport (vp);
}

void
gfx::render_context::reset_scissor_rect (uint32_t width, uint32_t height)
{
  SDL_Rect rect{};
  rect.x = 0;
  rect.y = 0;
  rect.w = static_cast<int> (width);
  rect.h = static_cast<int> (height);
  set_scissor_rect (rect);
}

} // namespace wsl
