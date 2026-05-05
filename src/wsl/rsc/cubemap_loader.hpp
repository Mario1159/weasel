#pragma once

#include <array>
#include <entt/resource/loader.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "../gfx/cubemap.hpp"
#include "../gfx/render_context.hpp"


namespace wsl
{

namespace rsc
{

/*!
 * \brief Specialized loader for cubemap resources.
 *
 * Supports loading cubemaps from TAR archives (containing 6 faces) or
 * from equirectangular images.
 */
struct cubemap_loader final : entt::resource_loader<gfx::cubemap>
{
  /*!
   * \brief Constructs a cubemap loader.
   * \param ctx Pointer to the render context.
   */
  explicit cubemap_loader (gfx::render_context *ctx);

  /*!
   * \brief Loads a cubemap from the specified path.
   * \param path Path to the cubemap file (TAR or image).
   * \return Shared pointer to the loaded cubemap, or `nullptr` if loading failed.
   */
  std::shared_ptr<gfx::cubemap> operator() (const std::string &path) const;

  /*!
   * \brief Wraps an existing cubemap into a shared pointer.
   * \param ready_cubemap The cubemap object to wrap.
   * \return Shared pointer to the cubemap.
   */
  std::shared_ptr<gfx::cubemap>
  operator() (gfx::cubemap &&ready_cubemap) const
  {
    return std::make_shared<gfx::cubemap> (std::move (ready_cubemap));
  }

private:
  std::shared_ptr<gfx::cubemap> load_from_tar (const std::string &path) const;
  std::shared_ptr<gfx::cubemap> load_from_equirect (const std::string &path) const;
  static uint32_t mip_count_2d (uint32_t w, uint32_t h) ;
  static int face_index_from_name (const std::string &name) ;

  gfx::render_context *m_ctx;

  static bool load_rgba_image_from_memory (const uint8_t *data, size_t size, int &w,
                                    int &h, std::vector<uint8_t> &pixels) ;

  bool upload_cubemap (SDL_GPUTexture *tex, int w, int h,
                       const std::array<std::vector<uint8_t>, 6> &faces) const;
};

} // namespace rsc

} // namespace wsl
