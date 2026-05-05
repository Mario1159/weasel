#pragma once

#include <glm/glm.hpp>

namespace wsl {
namespace debug {

class debug_renderer_interface
{
public:
  virtual ~debug_renderer_interface () = default;

  virtual void begin_frame () = 0;
  virtual void end_frame (const glm::mat4 &view_proj) = 0;
  virtual void upload_buffers () = 0;
  virtual void set_camera_pos (const glm::vec3 &pos) = 0;
};

} // namespace debug
} // namespace wsl
