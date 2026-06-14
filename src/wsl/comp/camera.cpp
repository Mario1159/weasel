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
  rsc::scene *scene = runtime_ctx->scene_manager.get_active ();

  changed |= ImGui::DragFloat ("FOV", &fov, 0.5F, 1.0F, 179.0F, "%.1f");
  changed |= ImGui::DragFloat ("Near", &near, 0.01F, 0.01F, far * 0.5F, "%.3f");
  changed
      |= ImGui::DragFloat ("Far", &far, 0.1F, near * 2.0F, 10000.0F, "%.1f");
  changed |= ImGui::Checkbox ("Only For Editor", &only_for_editor);

  ImGui::Separator ();

  if (scene == nullptr) {
    ImGui::TextDisabled ("No active scene.");
    return changed;
  }

  entt::registry &registry = scene->get_registry ();
  auto &ctx_reg = registry.ctx ();
  entt::entity current_main_cam = entt::null;
  bool has_rendering = false;

  if (ctx_reg.contains<comp::singl::rendering_manager> ()) {
    auto &rendering = ctx_reg.get<comp::singl::rendering_manager> ();
    current_main_cam = rendering.main_camera;
    has_rendering = true;
  }

  const char *preview = "None";
  if (current_main_cam != entt::null) {
    preview = scene->get_entity_name (current_main_cam).c_str ();
  }

  if (ImGui::BeginCombo ("Main Camera", preview)) {
    if (ImGui::Selectable ("None", current_main_cam == entt::null)) {
      if (has_rendering) {
        auto &rendering = ctx_reg.get<comp::singl::rendering_manager> ();
        rendering.main_camera = entt::null;
      }
      scene->camera = entt::null;
      changed = true;
    }

    auto cam_view = registry.view<comp::camera> ();
    for (entt::entity const e : cam_view) {
      comp::camera const &cam = cam_view.get<comp::camera> (e);
      if (cam.only_for_editor) {
        continue;
      }

      const std::string &name = scene->get_entity_name (e);
      bool const selected = (e == current_main_cam);

      if (ImGui::Selectable (name.c_str (), selected)) {
        if (has_rendering) {
          auto &rendering = ctx_reg.get<comp::singl::rendering_manager> ();
          rendering.main_camera = e;
        }
        scene->camera = e;
        changed = true;
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }

    auto cam2d_view = registry.view<comp::camera_2d> ();
    for (entt::entity const e : cam2d_view) {
      comp::camera_2d const &cam2d = cam2d_view.get<comp::camera_2d> (e);
      if (cam2d.only_for_editor) {
        continue;
      }

      const std::string &name = scene->get_entity_name (e);
      bool const selected = (e == current_main_cam);

      if (ImGui::Selectable (name.c_str (), selected)) {
        if (has_rendering) {
          auto &rendering = ctx_reg.get<comp::singl::rendering_manager> ();
          rendering.main_camera = e;
        }
        scene->camera = e;
        changed = true;
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }

    ImGui::EndCombo ();
  }

  return changed;
}

} // namespace comp

} // namespace wsl
