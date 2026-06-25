#include "batch_renderer_2d.hpp"
#include "render_context.hpp"
#include "render_window.hpp"
#include "shader.hpp"
#include "tracy_gpu_mem.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace wsl::gfx
{

batch_renderer_2d::batch_renderer_2d (wsl::gfx::render_window &window,
                                      render_context *ctx,
                                      wsl::rsc::resource_manager *res_mgr)
    : renderer (window, ctx, res_mgr)
{
  create_pipeline ();
  create_buffers ();

  m_vertices.reserve (max_vertices);
}

batch_renderer_2d::~batch_renderer_2d ()
{
  destroy_pipeline ();
  destroy_buffers ();
}

void
batch_renderer_2d::submit (const draw_command &cmd)
{
  m_queue.push_back (cmd);
}

void
batch_renderer_2d::set_projection (const glm::mat4 &proj)
{
  m_override_projection = proj;
}

std::vector<batch_renderer_2d::draw_command>
batch_renderer_2d::snapshot_queue () const
{
  return m_queue;
}

void
batch_renderer_2d::restore_queue (const std::vector<draw_command> &cmds)
{
  m_queue = cmds;
}

void
batch_renderer_2d::build_and_upload ()
{
  m_batches.clear ();
  if (m_queue.empty ()) {
    return;
  }

  // Sort by z-index then image
  std::sort (m_queue.begin (), m_queue.end (),
             [] (const draw_command &a, const draw_command &b) {
               if (a.z_index != b.z_index)
                 return a.z_index < b.z_index;
               return a.image.value < b.image.value;
             });

  uint32_t w, h;
  m_window->get_size (w, h);
  m_projection = m_override_projection.has_value ()
                     ? *m_override_projection
                     : glm::ortho (0.0F, (float)w, (float)h, 0.0F, -1.0F, 1.0F);
  m_override_projection.reset ();

  m_vertices.clear ();

  SDL_GPUTexture *current_texture = nullptr;
  uint32_t batch_start = 0;

  for (const auto &cmd : m_queue) {
    auto img_handle = m_res_mgr->get (cmd.image);
    SDL_GPUTexture *tex = img_handle ? img_handle->texture.get () : nullptr;

    if (tex != current_texture || m_vertices.size () + 6 > max_vertices) {
      if (current_texture != nullptr || !m_vertices.empty ()) {
        m_batches.push_back ({ current_texture, batch_start,
                               (uint32_t)m_vertices.size () - batch_start });
      }
      current_texture = tex;
      batch_start = (uint32_t)m_vertices.size ();

      if (m_vertices.size () + 6 > max_vertices) {
        break;
      }
    }

    // Quad vertices with rotation support
    float const x = cmd.position.x;
    float const y = cmd.position.y;
    float const sw = cmd.size.x;
    float const sh = cmd.size.y;

    float const half_sw = sw * 0.5F;
    float const half_sh = sh * 0.5F;

    glm::vec2 const corners[4] = { { -half_sw, -half_sh },
                                   { half_sw, -half_sh },
                                   { half_sw, half_sh },
                                   { -half_sw, half_sh } };

    glm::vec2 rotated_corners[4];
    float const cos_r = std::cos (cmd.rotation);
    float const sin_r = std::sin (cmd.rotation);

    for (int i = 0; i < 4; ++i) {
      rotated_corners[i].x
          = x + half_sw + (corners[i].x * cos_r - corners[i].y * sin_r);
      rotated_corners[i].y
          = y + half_sh + (corners[i].x * sin_r + corners[i].y * cos_r);
    }

    glm::vec2 u0 = cmd.uv_offset;
    glm::vec2 u1 = cmd.uv_offset + cmd.uv_scale;

    if (cmd.flip_h)
      std::swap (u0.x, u1.x);
    if (cmd.flip_v)
      std::swap (u0.y, u1.y);

    m_vertices.push_back ({ rotated_corners[0], { u0.x, u0.y }, cmd.color });
    m_vertices.push_back ({ rotated_corners[1], { u1.x, u0.y }, cmd.color });
    m_vertices.push_back ({ rotated_corners[2], { u1.x, u1.y }, cmd.color });

    m_vertices.push_back ({ rotated_corners[0], { u0.x, u0.y }, cmd.color });
    m_vertices.push_back ({ rotated_corners[2], { u1.x, u1.y }, cmd.color });
    m_vertices.push_back ({ rotated_corners[3], { u0.x, u1.y }, cmd.color });
  }

  if (!m_vertices.empty ()) {
    m_batches.push_back ({ current_texture, batch_start,
                           (uint32_t)m_vertices.size () - batch_start });
  }

  // Upload vertices
  if (!m_vertices.empty ()) {
    void *mapped
        = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, m_vbo_transfer, true);
    std::memcpy (mapped, m_vertices.data (),
                 m_vertices.size () * sizeof (vertex_2d));
    SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, m_vbo_transfer);

    SDL_GPUCommandBuffer *cmd_buf = m_ctx->command_buffer ();
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass (cmd_buf);
    SDL_GPUTransferBufferLocation src{ m_vbo_transfer, 0 };
    SDL_GPUBufferRegion dst{
      m_vbo, 0, (uint32_t)(m_vertices.size () * sizeof (vertex_2d))
    };
    SDL_UploadToGPUBuffer (copy, &src, &dst, false);
    SDL_EndGPUCopyPass (copy);
  }
}

void
batch_renderer_2d::draw ()
{
  if (m_batches.empty ()) {
    m_queue.clear ();
    return;
  }

  SDL_GPURenderPass *pass = m_ctx->main_render_pass ();
  if (pass == nullptr) {
    return;
  }

  SDL_BindGPUGraphicsPipeline (pass, m_pipeline);
  SDL_GPUBufferBinding vbo_binding{ m_vbo, 0 };
  SDL_BindGPUVertexBuffers (pass, 0, &vbo_binding, 1);

  // Push projection matrix
  SDL_PushGPUVertexUniformData (m_ctx->command_buffer (), 0, &m_projection,
                                sizeof (m_projection));

  for (const auto &b : m_batches) {
    SDL_GPUTexture *t = b.texture ? b.texture : m_window->hdr_scene;
    if (t) {
      SDL_GPUTextureSamplerBinding tex_binding{ t, m_sampler };
      SDL_BindGPUFragmentSamplers (pass, 0, &tex_binding, 1);
    }
    SDL_DrawGPUPrimitives (pass, b.vertex_count, 1, b.first_vertex, 0);
  }

  m_queue.clear ();
  m_batches.clear ();
  m_vertices.clear ();
}

void
batch_renderer_2d::flush ()
{
  build_and_upload ();
  draw ();
}

void
batch_renderer_2d::create_pipeline ()
{
  auto vert_id = m_res_mgr->register_shader (
      "engine://compiled_shaders/sprite_2d.vert.slang.spv");
  auto frag_id = m_res_mgr->register_shader (
      "engine://compiled_shaders/sprite_2d.frag.slang.spv");

  SDL_GPUShader *vert = shader::load_from_manager (
      m_ctx->gpu_device, m_res_mgr, vert_id, SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
  SDL_GPUShader *frag
      = shader::load_from_manager (m_ctx->gpu_device, m_res_mgr, frag_id,
                                   SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);

  if (!vert || !frag)
    return;

  SDL_GPUGraphicsPipelineCreateInfo pipe{};
  SDL_zero (pipe);
  pipe.vertex_shader = vert;
  pipe.fragment_shader = frag;
  pipe.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

  SDL_GPUColorTargetDescription ctd[1]{};
  ctd[0].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  ctd[0].blend_state.enable_blend = true;
  ctd[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  ctd[0].blend_state.dst_color_blendfactor
      = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  ctd[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  ctd[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  ctd[0].blend_state.dst_alpha_blendfactor
      = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  ctd[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  ctd[0].blend_state.color_write_mask
      = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G
        | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

  pipe.target_info.num_color_targets = 1;
  pipe.target_info.color_target_descriptions = ctd;

  SDL_GPUVertexBufferDescription vbd[1]{};
  vbd[0].slot = 0;
  vbd[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vbd[0].pitch = sizeof (vertex_2d);

  pipe.vertex_input_state.num_vertex_buffers = 1;
  pipe.vertex_input_state.vertex_buffer_descriptions = vbd;

  SDL_GPUVertexAttribute va[3]{};
  va[0].buffer_slot = 0;
  va[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  va[0].location = 0;
  va[0].offset = offsetof (vertex_2d, pos);

  va[1].buffer_slot = 0;
  va[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  va[1].location = 1;
  va[1].offset = offsetof (vertex_2d, uv);

  va[2].buffer_slot = 0;
  va[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  va[2].location = 2;
  va[2].offset = offsetof (vertex_2d, color);

  pipe.vertex_input_state.num_vertex_attributes = 3;
  pipe.vertex_input_state.vertex_attributes = va;

  m_pipeline = SDL_CreateGPUGraphicsPipeline (m_ctx->gpu_device, &pipe);
  wsl::gfx::tracy_alloc_pipeline (m_pipeline);

  SDL_ReleaseGPUShader (m_ctx->gpu_device, vert);
  SDL_ReleaseGPUShader (m_ctx->gpu_device, frag);

  SDL_GPUSamplerCreateInfo sinfo{};
  sinfo.min_filter = SDL_GPU_FILTER_LINEAR;
  sinfo.mag_filter = SDL_GPU_FILTER_LINEAR;
  sinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sinfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sinfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  m_sampler = SDL_CreateGPUSampler (m_ctx->gpu_device, &sinfo);
  wsl::gfx::tracy_alloc_sampler (m_sampler);
}

void
batch_renderer_2d::destroy_pipeline ()
{
  if (m_pipeline) {
    wsl::gfx::tracy_free_pipeline (m_pipeline);
    SDL_ReleaseGPUGraphicsPipeline (m_ctx->gpu_device, m_pipeline);
  }
  if (m_sampler) {
    wsl::gfx::tracy_free_sampler (m_sampler);
    SDL_ReleaseGPUSampler (m_ctx->gpu_device, m_sampler);
  }
}

void
batch_renderer_2d::create_buffers ()
{
  SDL_GPUBufferCreateInfo binfo{};
  binfo.size = max_vertices * sizeof (vertex_2d);
  binfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  m_vbo = SDL_CreateGPUBuffer (m_ctx->gpu_device, &binfo);
  wsl::gfx::tracy_alloc_buffer (m_vbo, binfo.size);

  SDL_GPUTransferBufferCreateInfo tinfo{};
  tinfo.size = max_vertices * sizeof (vertex_2d);
  tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  m_vbo_transfer = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tinfo);
  wsl::gfx::tracy_alloc_transfer (m_vbo_transfer, tinfo.size);
}

void
batch_renderer_2d::destroy_buffers ()
{
  if (m_vbo) {
    wsl::gfx::tracy_free_buffer (m_vbo);
    SDL_ReleaseGPUBuffer (m_ctx->gpu_device, m_vbo);
  }
  if (m_vbo_transfer) {
    wsl::gfx::tracy_free_transfer (m_vbo_transfer);
    SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, m_vbo_transfer);
  }
}

} // namespace wsl::gfx
