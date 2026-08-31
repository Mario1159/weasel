#pragma once

#include <SDL3/SDL_gpu.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

namespace wsl::comp::singl { class runtime_context; }
namespace wsl::rsc { class resource_manager; }

namespace wsl::gfx {

struct imgui_fonts
{
  ImFont *regular = nullptr;
  ImFont *light = nullptr;
  ImFont *medium = nullptr;
  ImFont *semibold = nullptr;
  ImFont *bold = nullptr;
  ImFont *title = nullptr;
  ImFont *mono = nullptr;
};

struct editor_theme
{
  ImVec4 primary;
  ImVec4 secondary;
  ImVec4 background1;
  ImVec4 background2;
  ImVec4 foreground;
};

class imgui_renderer_interface
{
public:
  virtual ~imgui_renderer_interface () = default;

  virtual void begin_frame () = 0;
  virtual void end_frame () = 0;
  virtual void prepare (ImDrawData *draw_data) = 0;
  virtual void render (ImDrawData *draw_data) = 0;
  virtual void render_requested_previews () = 0;
  virtual void on_resize (uint32_t w, uint32_t h) = 0;

  // Preview system
  virtual void request_model_preview (wsl::comp::singl::runtime_context *runtime_ctx,
                                      wsl::rsc::resource_manager *resource_manager,
                                      entt::id_type model_eid, uint32_t w,
                                      uint32_t h) = 0;
  virtual void preview_set_camera_from_gizmo (const glm::vec3 &pos,
                                              const glm::quat &rot) = 0;
  virtual void preview_get_camera (glm::vec3 &out_pos,
                                   glm::quat &out_rot) const = 0;
  virtual void preview_reset_camera_to_default () = 0;
  virtual SDL_GPUTexture *get_model_preview_texture () const = 0;

  // Editor styling
  virtual void apply_editor_style (const editor_theme &t) = 0;
  virtual const editor_theme &get_theme () const = 0;

  virtual imgui_fonts &get_fonts () = 0;
  virtual const imgui_fonts &get_fonts () const = 0;
};

} // namespace wsl::gfx
