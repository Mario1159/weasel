#pragma once

#include "wsl/comp/camera.hpp"
#include "wsl/gfx/mesh.hpp"
#include "wsl/gfx/render_context.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <entt/entt.hpp>

namespace wsl
{

namespace rsc { class resource_manager; }

namespace gfx
{

class render_window
{
public:
  render_window (const char *name, int width, int height,
                 wsl::gfx::render_context *ctx,
                 wsl::rsc::resource_manager *res_mgr);
  ~render_window ();

  void get_size (uint32_t &width, uint32_t &height) const;
  void get_size (int &width, int &height) const;

  SDL_Window *handler = nullptr;
  wsl::gfx::texture swapchain;

  // entt::registry *registry;
  wsl::gfx::render_context *ctx;

  SDL_GPUTexture *depth_texture = nullptr;
  SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

  SDL_GPUTexture *msaa_hdr_scene = nullptr;
  SDL_GPUTexture *msaa_hdr_bloom = nullptr;

  SDL_GPUTexture *hdr_scene = nullptr; // resolved HDR scene (sampler)
  SDL_GPUTexture *hdr_bloom_src
      = nullptr; // resolved HDR bloom source (sampler)

  // half-res bloom ping-pong
  SDL_GPUTexture *bloom_a = nullptr;
  SDL_GPUTexture *bloom_b = nullptr;

  // final LDR (tonemapped + bloom) output that can be sampled by ImGui /
  // GameView
  wsl::gfx::texture present_tex;
  SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

  SDL_GPUGraphicsPipeline *pipe_downsample = nullptr;
  SDL_GPUGraphicsPipeline *pipe_blur = nullptr;
  SDL_GPUGraphicsPipeline *pipe_composite = nullptr;

  SDL_GPUSampler *linear_sampler = nullptr;

  int current_sample_count = SDL_GPU_SAMPLECOUNT_4;
  bool present_to_swapchain = true;
  SDL_FColor scene_clear_color{ 0.1F, 0.1F, 0.1F, 1.0F };
  float exposure = 1.0F;
  float bloom_intensity = 1.0F;

  void create_depth_texture ();
  void begin_3d_pass () const;
  void end_3d_pass ();
  void begin_ui_pass () const;
  void end_ui_pass () const;
  void new_swapchain ();
  void on_resize ();
  void postprocess_hdr_bloom ();

  [[nodiscard]] wsl::rsc::resource_manager *resource_manager () const { return m_res_mgr; }

private:
  SDL_GPUSampler *ensure_linear_sampler ();
  void destroy_texture (SDL_GPUTexture *&texture) const;
  SDL_GPUGraphicsPipeline *create_fullscreen_pipe (
      const char *frag_shader_path, SDL_GPUTextureFormat out_format,
      int num_uniform_buffers, int num_samplers);
  SDL_GPUGraphicsPipeline *create_composite_pipe ();
  SDL_GPUGraphicsPipeline *create_downsample_pipe ();
  SDL_GPUGraphicsPipeline *create_blur_pipe ();

private:
  wsl::rsc::resource_manager *m_res_mgr = nullptr;
};

} // namespace gfx

} // namespace wsl
