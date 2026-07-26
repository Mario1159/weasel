#pragma once

#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <cstdint>

namespace wsl
{

namespace gfx
{

/**
 * Defines a screen region and optional camera for rendering a sub-view.
 *
 * Coordinates are normalized [0,1] relative to the window. The rendering
 * pipeline uses this to set the GPU viewport and scissor, and to compute
 * the correct aspect ratio for the projection matrix.
 */
struct viewport
{
  float x = 0.0F;      // Normalized left edge (0 = left)
  float y = 0.0F;      // Normalized top edge (0 = top)
  float width = 1.0F;  // Normalized width
  float height = 1.0F; // Normalized height
  float min_depth = 0.0F;
  float max_depth = 1.0F;

  entt::entity camera = entt::null; // Camera entity for this viewport; null =
                                    // use scene camera

  bool clear_color
      = false; // Whether to clear the color target before this viewport
  bool clear_depth
      = false; // Whether to clear the depth target before this viewport

  SDL_FColor clear_color_value{
    0.0F, 0.0F, 0.0F, 1.0F
  }; // Color to clear with (if clear_color is true)

  /** Returns true when the viewport covers the full screen. */
  [[nodiscard]] bool
  is_fullscreen () const
  {
    return (x <= 0.0F) && (y <= 0.0F) && (width >= 1.0F) && (height >= 1.0F);
  }

  /** Converts normalized coordinates to pixel dimensions. */
  [[nodiscard]] auto
  to_pixels (uint32_t window_width, uint32_t window_height) const
  {
    struct pixel_rect
    {
      int x, y, width, height;
    };
    return pixel_rect{
      static_cast<int> (x * static_cast<float> (window_width)),
      static_cast<int> (y * static_cast<float> (window_height)),
      static_cast<int> (width * static_cast<float> (window_width)),
      static_cast<int> (height * static_cast<float> (window_height)),
    };
  }

  /** Computes the aspect ratio (width/height) for this viewport. */
  [[nodiscard]] float
  aspect_ratio (uint32_t window_width, uint32_t window_height) const
  {
    float const w = width * static_cast<float> (window_width);
    float const h = height * static_cast<float> (window_height);
    if (h <= 0.0F) {
      return 1.0F;
    }
    return w / h;
  }

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (x, y, width, height, min_depth, max_depth, camera, clear_color,
        clear_depth, clear_color_value.r, clear_color_value.g,
        clear_color_value.b, clear_color_value.a);
  }

  [[nodiscard]] bool
  operator== (const viewport &other) const
  {
    return x == other.x && y == other.y && width == other.width
           && height == other.height && min_depth == other.min_depth
           && max_depth == other.max_depth && camera == other.camera
           && clear_color == other.clear_color
           && clear_depth == other.clear_depth
           && clear_color_value.r == other.clear_color_value.r
           && clear_color_value.g == other.clear_color_value.g
           && clear_color_value.b == other.clear_color_value.b
           && clear_color_value.a == other.clear_color_value.a;
  }

  [[nodiscard]] bool
  operator!= (const viewport &other) const
  {
    return !(*this == other);
  }
};

} // namespace gfx

} // namespace wsl
