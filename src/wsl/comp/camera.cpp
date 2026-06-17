#include "camera.hpp"

#include "camera_2d.hpp"
#include "component_meta.hpp"
#include "singl/rendering_manager.hpp"
#include "singl/runtime_context.hpp"
#include "../rsc/scene.hpp"
#include "../rsc/scene_manager.hpp"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <entt/meta/factory.hpp>
#include <imgui.h>

namespace wsl
{

namespace comp
{

void
camera::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::camera> ()
      .type (entt::type_hash<comp::camera>::value ())
      .custom<comp::meta_info> (meta_info{ "Camera",
                                           "Projection parameters only",
                                           "engine://icons/comp_camera.svg" })
      .func<&comp::camera::custom_inspect> ("custom_inspect"_hs)
      .data<&camera::fov> ("fov"_hs)
      .custom<comp::meta_info> (
          meta_info{ "FOV", "Field of view in degrees", "" })
      .data<&camera::near> ("near"_hs)
      .custom<comp::meta_info> (meta_info{ "Near", "Near clipping plane", "" })
      .data<&camera::far> ("far"_hs)
      .custom<comp::meta_info> (meta_info{ "Far", "Far clipping plane", "" });
}

bool
camera::custom_inspect (const char * /*label*/,
                        comp::singl::runtime_context *runtime_ctx)
{
  if (runtime_ctx == nullptr) {
    return false;
  }

  bool changed = false;

  changed |= ImGui::DragFloat ("FOV", &fov, 0.5F, 1.0F, 179.0F, "%.1f");
  changed |= ImGui::DragFloat ("Near", &near, 0.01F, 0.01F, far * 0.5F, "%.3f");
  changed
      |= ImGui::DragFloat ("Far", &far, 0.1F, near * 2.0F, 10000.0F, "%.1f");
  changed |= ImGui::Checkbox ("Only For Editor", &only_for_editor);

  return changed;
}

} // namespace comp

} // namespace wsl
