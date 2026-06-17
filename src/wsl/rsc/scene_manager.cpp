#include "scene_manager.hpp"

#include "../comp/camera.hpp"
#include "../comp/hierarchy.hpp"
#include "../comp/model_instance_3d.hpp"
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../comp/singl/ui_manager.hpp"
#include "../comp/transform.hpp"
#include "../comp/world_transform.hpp"
#include "comp/component_meta.hpp"
#include "rsc/resource_ref.hpp"
#include "rsc/scene.hpp"
#include "wsl/events.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL_events.h>
#include <cstdio>
#include <entt/entity/fwd.hpp>
#include <entt/entt.hpp>
#include <imgui.h>
#include <string>

namespace wsl
{

namespace
{

entt::entity
resolve_active_game_camera (rsc::scene &scene)
{
  auto &registry = scene.get_registry ();

  if (scene.camera != entt::null
      && registry.all_of<comp::camera, comp::world_transform> (scene.camera)) {
    const auto &camera = registry.get<comp::camera> (scene.camera);
    if (!camera.only_for_editor) {
      return scene.camera;
    }
  }

  auto view = registry.view<comp::camera, comp::world_transform> ();
  for (const entt::entity entity : view) {
    const auto &camera = view.get<comp::camera> (entity);
    if (!camera.only_for_editor) {
      return entity;
    }
  }

  return entt::null;
}

} // namespace

void
rsc::scene_manager::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<rsc::scene_manager> ()
      .type (entt::type_hash<rsc::scene_manager>::value ())
      .custom<comp::meta_info> (
          comp::meta_info{ "Scene Manager",
                           "Controls scene creation and active scene "
                           "selection for the current runtime.",
                           "" })
      .func<&rsc::scene_manager::custom_inspect> ("custom_inspect"_hs);
}

void
rsc::scene_manager::set_active (scene *scene_ptr)
{
  if (m_active_scene == scene_ptr) {
    return;
  }

  wsl::log::sys ()->debug ("Activating scene: {}",
                           scene_ptr ? scene_ptr->get_name () : "(none)");

  scene *old_scene = m_active_scene;
  m_active_scene = scene_ptr;

  if (m_active_scene != nullptr) {
    if (auto *runtime_ctx = m_main_world.get_runtime_context ()) {
      m_active_scene->m_running = runtime_ctx->is_running;
    }
    m_active_scene->init ();

    if (auto *runtime_ctx = m_main_world.get_runtime_context ()) {
      auto &registry = m_active_scene->get_registry ();
      auto &ctx = registry.ctx ();
      const entt::entity active_camera
          = resolve_active_game_camera (*m_active_scene);
      m_active_scene->camera = active_camera;
      if (ctx.contains<comp::singl::rendering_manager> ()) {
        // Default to root viewport (entt::null) when switching scenes.
        ctx.get<comp::singl::rendering_manager> ().render_viewport = entt::null;
      }

      if (ctx.contains<comp::singl::ui_manager *> ()) {
        auto &ui = *ctx.get<comp::singl::ui_manager *> ();
        ui.needs_reload = true;
        ui.prepare_scene (registry);
      }
    }
  }

  m_main_world.get_runtime_context ()
      ->dispatcher.trigger<wsl::event::scene_changed> (
          { old_scene, m_active_scene });
}

rsc::scene &
rsc::scene_manager::create_scene (const std::string &name, bool make_active)
{
  scene &new_scene = m_main_world.create_scene (name);
  wsl::log::sys ()->debug ("Created scene: {}", name);
  if (make_active) {
    set_active (&new_scene);
  }
  return new_scene;
}

rsc::scene &
rsc::scene_manager::create_default_scene (const std::string &name,
                                          bool make_active)
{
  scene &new_scene = create_scene (name, make_active);
  auto &reg = new_scene.get_registry ();
  auto *runtime_ctx = m_main_world.get_runtime_context ();

  // Skybox
  auto &rendering = reg.ctx ().emplace<comp::singl::rendering_manager> ();
  rendering.skybox = { builtin_skybox_procedural };

  // Register built-in resources and add to scene load list
  const model_id builtin_cube_id
      = runtime_ctx->resource_manager.register_model ("builtin://cube");
  const model_id builtin_sphere_id
      = runtime_ctx->resource_manager.register_model ("builtin://sphere");
  const cubemap_id builtin_skybox_id
      = runtime_ctx->resource_manager.register_cubemap (
          "builtin/skybox_procedural");
  new_scene.add_resource (io::resource_type::model, builtin_cube_id.value);
  new_scene.add_resource (io::resource_type::model, builtin_sphere_id.value);
  new_scene.add_resource (io::resource_type::cubemap, builtin_skybox_id.value);

  // Sample cube
  auto sample_cube = reg.create ();
  new_scene.set_entity_name (sample_cube, "Sample Cube");
  reg.emplace<comp::hierarchy> (sample_cube);
  reg.emplace<comp::transform> (sample_cube, glm::vec3 (0.0F, 0.0F, 0.0F));
  reg.emplace<comp::world_transform> (sample_cube);
  auto &cube_model = reg.emplace<comp::model_instance_3d> (sample_cube);
  cube_model.id = builtin_cube_id;
  cube_model.scene_index = 0;

  // Default camera
  auto cam_entity = reg.create ();
  new_scene.set_entity_name (cam_entity, "Scene Default Camera");
  reg.emplace<comp::hierarchy> (cam_entity);
  auto &cam_transform
      = reg.emplace<comp::transform> (cam_entity, glm::vec3 (0.0F, 0.0F, 5.0F));
  auto &cam_world_transform = reg.emplace<comp::world_transform> (cam_entity);
  cam_world_transform.value = cam_transform.model ();
  reg.emplace<comp::camera> (cam_entity);
  new_scene.camera = cam_entity;

  return new_scene;
}

void
rsc::scene_manager::destroy_scene (scene *scene_ptr)
{
  if (scene_ptr == nullptr) {
    return;
  }

  if (m_active_scene == scene_ptr) {
    scene *replacement = nullptr;
    for (const auto &candidate : m_main_world.get_scenes ()) {
      if (candidate && candidate.get () != scene_ptr) {
        replacement = candidate.get ();
        break;
      }
    }

    set_active (replacement);
  }

  wsl::log::sys ()->debug ("Destroying scene: {}", scene_ptr->get_name ());

  m_main_world.destroy_scene (scene_ptr);
}

rsc::scene *
rsc::scene_manager::get_active () const
{
  return m_active_scene;
}

rsc::world &
rsc::scene_manager::get_world ()
{
  return m_main_world;
}

const rsc::world &
rsc::scene_manager::get_world () const
{
  return m_main_world;
}

void
rsc::scene_manager::update (double dt)
{
  if (m_active_scene != nullptr) {
    m_active_scene->update (dt);
  }
}

void
rsc::scene_manager::handle_events (const SDL_Event &event)
{
  if (m_active_scene != nullptr) {
    m_active_scene->handle_events (event);
  }
}

bool
rsc::scene_manager::custom_inspect (const char *label,
                                    comp::singl::runtime_context *runtime_ctx)
{
  (void)label;
  (void)runtime_ctx;

  bool changed = false;
  auto &scenes = m_main_world.get_scenes ();

  ImGui::TextDisabled ("Scene Count: %zu", scenes.size ());

  const char *preview = (m_active_scene != nullptr)
                            ? m_active_scene->get_name ().c_str ()
                            : "None";
  if (ImGui::BeginCombo ("Active Scene", preview)) {
    for (const auto &scene_ptr : scenes) {
      if (!scene_ptr) {
        continue;
      }

      const bool selected = (scene_ptr.get () == m_active_scene);
      if (ImGui::Selectable (scene_ptr->get_name ().c_str (), selected)) {
        set_active (scene_ptr.get ());
        changed = true;
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }

  static int new_scene_counter = 1;
  if (ImGui::Button ("Create Scene")) {
    char name[64];
    std::snprintf (name, sizeof (name), "Scene %d", new_scene_counter++);
    create_scene (name, true);
    changed = true;
  }

  if (m_active_scene == nullptr) {
    ImGui::BeginDisabled ();
  }

  ImGui::SameLine ();
  if (ImGui::Button ("Remove Active Scene") && (m_active_scene != nullptr)) {
    scene *to_remove = m_active_scene;
    destroy_scene (to_remove);
    changed = true;
  }

  if (m_active_scene == nullptr) {
    ImGui::EndDisabled ();
  }

  return changed;
}

} // namespace wsl
