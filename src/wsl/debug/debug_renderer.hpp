#pragma once

#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif

namespace wsl
{
namespace gfx
{
class scene_renderer;
} // namespace gfx

namespace debug
{

/**
 * Collects world-space line geometry during the draw-data build pass and
 * hands it to the scene renderer's debug line pass during the record pass.
 *
 * Implementations accumulate between `begin_frame` and `end_frame`, so a new
 * frame can drop the previous batch simply by starting again. `end_frame`
 * runs while the 3D pass is open, which lets the batch share the depth buffer
 * with the scene it visualizes.
 */
class debug_renderer_interface
{
public:
  virtual ~debug_renderer_interface () = default;

  /** Drops the lines collected last frame and starts a new batch. */
  virtual void begin_frame () = 0;

  /** Appends a world-space line segment. */
  virtual void draw_segment (const glm::vec3 &begin, const glm::vec3 &end,
                             const glm::vec4 &color) = 0;

  /**
   * Submits everything collected since `begin_frame`.
   *
   * :param renderer: Scene renderer whose debug line pass draws the batch.
   * :param view_proj: View-projection matrix of the current viewport.
   */
  virtual void end_frame (gfx::scene_renderer &renderer,
                          const glm::mat4 &view_proj) = 0;
};

} // namespace debug
} // namespace wsl
