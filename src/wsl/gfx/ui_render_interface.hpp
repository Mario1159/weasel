#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <SDL3/SDL.h>
#include <unordered_map>

#include "render_context.hpp"
#include "wsl/gfx/render_window.hpp"


namespace rsc { class resource_manager; }

namespace wsl
{

class ui_render_interface : public Rml::RenderInterface
{
public:
  ui_render_interface (gfx::render_context *ctx, wsl::gfx::render_window *window,
                       wsl::rsc::resource_manager *res_mgr);
  ~ui_render_interface () override;

  Rml::CompiledGeometryHandle
  CompileGeometry (Rml::Span<const Rml::Vertex> vertices,
                   Rml::Span<const int> indices) override;

  void RenderGeometry (Rml::CompiledGeometryHandle geometry,
                       Rml::Vector2f translation,
                       Rml::TextureHandle texture) override;

  void ReleaseGeometry (Rml::CompiledGeometryHandle geometry) override;

  Rml::TextureHandle LoadTexture (Rml::Vector2i &texture_dimensions,
                                  const Rml::String &source) override;

  Rml::TextureHandle GenerateTexture (Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i size) override;

  void ReleaseTexture (Rml::TextureHandle texture) override;

  void EnableScissorRegion (bool enable) override;
  void SetScissorRegion (Rml::Rectanglei region) override;

private:
  struct ui_geometry
  {
    SDL_GPUBuffer *vbo = nullptr;
    SDL_GPUBuffer *ibo = nullptr;
    uint32_t index_count = 0;
  };

  struct ui_texture
  {
    SDL_GPUTexture *texture = nullptr;
    int width = 0;
    int height = 0;
  };

  gfx::render_context *m_ctx = nullptr;
  wsl::gfx::render_window *m_window = nullptr;

  SDL_GPUGraphicsPipeline *m_pipeline = nullptr;

  bool m_scissor_enabled = false;
  Rml::Rectanglei m_scissor;

  std::unordered_map<Rml::CompiledGeometryHandle, ui_geometry> m_geometries;
  std::unordered_map<Rml::TextureHandle, ui_texture> m_textures;

  Rml::CompiledGeometryHandle m_next_geom_handle = 1;
  Rml::TextureHandle m_next_tex_handle = 1;

  void create_pipeline (wsl::rsc::resource_manager *res_mgr);
};

} // namespace wsl
