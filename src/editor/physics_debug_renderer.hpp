#pragma once

#include "wsl/input.hpp"
#include "wsl/gfx/render_window.hpp"
#include "wsl/gfx/render_context.hpp"
#include "wsl/debug/debug_renderer.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include <memory>

namespace editor
{

struct debug_vertex
{
  glm::vec3 pos;
  glm::vec4 color;
};

class physics_debug_renderer : public JPH::DebugRenderer, public wsl::debug::debug_renderer_interface
{
public:
  physics_debug_renderer (wsl::gfx::render_window &window, wsl::gfx::render_context *ctx);
  ~physics_debug_renderer () override;

  // debug_renderer_interface
  void begin_frame () override;
  void end_frame (const glm::mat4 &vp) override;
  void upload_buffers () override;
  void set_camera_pos (const glm::vec3 &pos) override;

  // Jolt overrides
  void DrawLine (JPH::RVec3Arg a, JPH::RVec3Arg b,
                 JPH::ColorArg color) override;

  void DrawTriangle (JPH::RVec3Arg a, JPH::RVec3Arg b, JPH::RVec3Arg c,
                     JPH::ColorArg color,
                     ECastShadow e = ECastShadow::Off) override;

  Batch CreateTriangleBatch (const Triangle *tris, int count) override;
  Batch CreateTriangleBatch (const Vertex *in_vertices, int in_vertex_count,
                             const uint32_t *in_indices,
                             int in_index_count) override;

  void DrawGeometry (JPH::RMat44Arg inModelMatrix,
                     const JPH::AABox &inWorldSpaceBounds, float inLODScaleSq,
                     JPH::ColorArg color, const GeometryRef &geometry,
                     ECullMode cull_mode, ECastShadow cast_shadow,
                     EDrawMode inDrawMode) override;

  void DrawText3D (JPH::RVec3Arg pos, const JPH::string_view &text,
                   JPH::ColorArg color, float height) override;

  glm::vec3 camera_pos{ 0.0F, 0.0F, 0.0F };

private:
  wsl::gfx::render_window *m_window = nullptr;

  size_t m_vertex_buffer_size = 0;

  SDL_GPUGraphicsPipeline *m_pipeline_lines = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_tris = nullptr;

  SDL_GPUBuffer *m_vertex_buffer = nullptr;
  SDL_GPUTransferBuffer *m_upload_buffer = nullptr;

  std::vector<debug_vertex> m_line_vertices;
  std::vector<debug_vertex> m_tri_vertices;

  wsl::gfx::render_context *m_ctx = nullptr;

  SDL_GPUTexture *m_default_texture = nullptr;
  SDL_GPUSampler *m_default_sampler = nullptr;

  void create_default_texture ();
  void destroy_default_resources ();

  void flush (const glm::mat4 &vp);
};

std::unique_ptr<physics_debug_renderer> make_physics_debug_renderer (wsl::gfx::render_window &window,
                                                                    wsl::gfx::render_context *ctx);

class debug_triangle_batch final : public JPH::RefTargetVirtual
{
public:
  struct tri
  {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    glm::vec4 color;
  };

  JPH::Array<tri> tris;

  void
  AddRef () override
  {
    ++m_m_ref_count;
  }
  void
  Release () override
  {
    if (--m_m_ref_count == 0) {
      delete this;
}
  }

private:
  JPH::atomic<JPH::uint32> m_m_ref_count{ 0 };
};

} // namespace editor
