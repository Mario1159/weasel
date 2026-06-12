#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstdint>

namespace wsl
{

namespace rsc
{
class resource_manager;
}

namespace gfx
{

class render_window;
class render_context;

/*!
 * \brief Base class for graphics renderers.
 *
 * Provides shared access to the rendering context, window, and resource
 * manager, along with common GPU utility methods.
 */
class renderer
{
public:
  renderer (wsl::gfx::render_window &window, render_context *ctx,
            wsl::rsc::resource_manager *res_mgr);
  virtual ~renderer () = default;

protected:
  /*!
   * \brief Creates a 1x1 RGBA8 texture with the specified color.
   */
  [[nodiscard]] auto create_1x1_texture (uint8_t red, uint8_t green,
                                         uint8_t blue, uint8_t alpha) const
      -> SDL_GPUTexture *;

  /*!
   * \brief Creates a 1x1 RGBA8 cubemap with the specified color on all faces.
   */
  [[nodiscard]] auto create_1x1_cubemap (uint8_t red, uint8_t green,
                                         uint8_t blue, uint8_t alpha) const
      -> SDL_GPUTexture *;

  wsl::gfx::render_window *m_window = nullptr;
  render_context *m_ctx = nullptr;
  wsl::rsc::resource_manager *m_res_mgr = nullptr;
};

} // namespace gfx

} // namespace wsl
