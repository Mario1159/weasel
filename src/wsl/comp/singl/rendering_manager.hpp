#pragma once

#include "../../gfx/scene_renderer.hpp"
#include "../../gfx/batch_renderer_2d.hpp"
#include "../../gfx/model_3d.hpp"
#include "../../gfx/subviewport_target.hpp"
#include "../../gfx/viewport.hpp"
#include "../../rsc/resource_manager.hpp"
#include "../component_meta.hpp"
#include "../transform.hpp"

#include <cereal/cereal.hpp>
#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace comp::singl
{

struct rendering_manager : singleton_component
{
  std::unique_ptr<gfx::scene_renderer> renderer;
  std::unique_ptr<gfx::batch_renderer_2d> renderer_2d;
  rsc::cubemap_id skybox{};
  math::quatf skybox_rotation{};

  math::vec3f clear_color{ 0.1F, 0.1F, 0.1F };
  float clear_alpha = 1.0F;

  math::vec3f ambient_color{ 1.0F, 1.0F, 1.0F };
  float ambient_intensity = 0.01F;

  float sun_altitude = 40.0F;
  float sun_azimuth = 135.0F;

  float exposure = 1.0F;
  float bloom_threshold = 1.0F;
  float bloom_knee = 0.5F;
  float bloom_intensity = 1.0F;
  float ibl_intensity = 1.0F;

  bool ssao_enabled = true;
  float ssao_radius = 0.6F;
  float ssao_bias = 0.025F;
  float ssao_power = 1.25F;
  float ssao_intensity = 1.0F;

  math::vec3f outline_color{ 1.0F, 0.65F, 0.1F };
  float outline_alpha = 1.0F;
  float outline_width = 0.035F;

  float shadow_bias = 0.0025F;
  float shadow_strength = 1.0F;

  //! Active viewports for this scene. If empty, a fullscreen root viewport is
  //! used.
  std::vector<gfx::viewport> viewports;

  //! The active render viewport entity (null = Root Viewport).
  entt::entity render_viewport = entt::null;

  //! Design/virtual size for the root viewport, used by the editor 2D game view
  //! as a guizmo reference.
  math::vec2f root_viewport_virtual_size{ 1920.0F, 1080.0F };

  //! Offscreen GPU targets for subviewport entities.
  std::unordered_map<entt::entity, gfx::subviewport_target> subviewport_targets;

  //! Shared unit-quad model used to draw subviewport contents in 3D space.
  std::shared_ptr<gfx::model_3d> subviewport_quad_model;

  gfx::scene_renderer &
  ensure_renderer (wsl::gfx::render_window &window,
                   gfx::render_context &render_ctx,
                   wsl::rsc::resource_manager *res_mgr)
  {
    if (!renderer) {
      renderer = std::make_unique<gfx::scene_renderer> (window, &render_ctx,
                                                        res_mgr);
    }

    return *renderer;
  }

  gfx::scene_renderer *
  try_renderer ()
  {
    return const_cast<gfx::scene_renderer *> (renderer.get ());
  }

  const gfx::scene_renderer *
  try_renderer () const
  {
    return renderer.get ();
  }

  gfx::batch_renderer_2d &
  ensure_renderer_2d (wsl::gfx::render_window &window,
                      gfx::render_context &render_ctx,
                      wsl::rsc::resource_manager *res_mgr)
  {
    if (!renderer_2d) {
      renderer_2d = std::make_unique<gfx::batch_renderer_2d> (
          window, &render_ctx, res_mgr);
    }

    return *renderer_2d;
  }

  gfx::batch_renderer_2d *
  try_renderer_2d ()
  {
    return renderer_2d.get ();
  }

  const gfx::batch_renderer_2d *
  try_renderer_2d () const
  {
    return renderer_2d.get ();
  }

  glm::vec3
  get_sun_direction () const
  {
    float const alt_rad = glm::radians (sun_altitude);
    float const az_rad = glm::radians (sun_azimuth);

    // Direction vector pointing FROM the sun.
    // At alt=90, az=0 -> {0, -1, 0} (overhead)
    // At alt=0, az=0 -> {0, 0, 1} (horizon)
    return glm::vec3 (-cos (alt_rad) * sin (az_rad), -sin (alt_rad),
                      -cos (alt_rad) * cos (az_rad));
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::singl::rendering_manager> ()
        .type (entt::type_hash<comp::singl::rendering_manager>::value ())
        .custom<comp::meta_info> (meta_info{
            "Rendering Manager",
            "Scene-owned rendering controls for environment, lighting, and "
            "post-process settings.",
            "" })

        .data<&comp::singl::rendering_manager::skybox> ("skybox"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Skybox", "Environment cubemap used by the scene.", "" })

        .data<&comp::singl::rendering_manager::skybox_rotation> (
            "skybox_rotation"_hs)
        .custom<comp::meta_info> (meta_info{
            "Skybox Rotation",
            "Rotation applied to the skybox sampling direction.", "" })

        .data<&comp::singl::rendering_manager::clear_color> ("clear_color"_hs)
        .custom<comp::meta_info> (meta_info{
            "Clear Color",
            "Base color used when clearing the 3D render target.", "" })

        .data<&comp::singl::rendering_manager::clear_alpha> ("clear_alpha"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Clear Alpha",
                       "Alpha used when clearing the 3D render target.", "" })

        .data<&comp::singl::rendering_manager::ambient_color> (
            "ambient_color"_hs)
        .custom<comp::meta_info> (meta_info{
            "Ambient Color", "Global ambient light tint for the scene.", "" })

        .data<&comp::singl::rendering_manager::ambient_intensity> (
            "ambient_intensity"_hs)
        .custom<comp::meta_info> (meta_info{
            "Ambient Intensity", "Global ambient light strength.", "" })

        .data<&comp::singl::rendering_manager::sun_altitude> ("sun_altitude"_hs)
        .custom<comp::meta_info> (meta_info{
            "Sun Altitude", "Altitude of the sun in degrees (0-90).", "" })

        .data<&comp::singl::rendering_manager::sun_azimuth> ("sun_azimuth"_hs)
        .custom<comp::meta_info> (meta_info{
            "Sun Azimuth", "Azimuth of the sun in degrees (0-360).", "" })

        .data<&comp::singl::rendering_manager::exposure> ("exposure"_hs)
        .custom<comp::meta_info> (meta_info{
            "Exposure", "Tonemap exposure applied during final compositing.",
            "" })

        .data<&comp::singl::rendering_manager::bloom_threshold> (
            "bloom_threshold"_hs)
        .custom<comp::meta_info> (meta_info{
            "Bloom Threshold",
            "Brightness required before bloom starts contributing.", "" })

        .data<&comp::singl::rendering_manager::bloom_knee> ("bloom_knee"_hs)
        .custom<comp::meta_info> (meta_info{
            "Bloom Knee", "Soft transition region around the bloom threshold.",
            "" })

        .data<&comp::singl::rendering_manager::bloom_intensity> (
            "bloom_intensity"_hs)
        .custom<comp::meta_info> (meta_info{
            "Bloom Intensity", "Final bloom contribution strength.", "" })

        .data<&comp::singl::rendering_manager::ibl_intensity> (
            "ibl_intensity"_hs)
        .custom<comp::meta_info> (meta_info{
            "IBL Intensity",
            "Strength of image-based lighting from the environment.", "" })

        .data<&comp::singl::rendering_manager::ssao_enabled> ("ssao_enabled"_hs)
        .custom<comp::meta_info> (meta_info{
            "SSAO Enabled", "Enables screen-space ambient occlusion.", "" })

        .data<&comp::singl::rendering_manager::ssao_radius> ("ssao_radius"_hs)
        .custom<comp::meta_info> (meta_info{
            "SSAO Radius", "Sampling radius used when building SSAO.", "" })

        .data<&comp::singl::rendering_manager::ssao_bias> ("ssao_bias"_hs)
        .custom<comp::meta_info> (meta_info{
            "SSAO Bias", "Depth bias used to reduce SSAO self-occlusion.", "" })

        .data<&comp::singl::rendering_manager::ssao_power> ("ssao_power"_hs)
        .custom<comp::meta_info> (
            meta_info{ "SSAO Power", "Contrast exponent applied to SSAO.", "" })

        .data<&comp::singl::rendering_manager::ssao_intensity> (
            "ssao_intensity"_hs)
        .custom<comp::meta_info> (
            meta_info{ "SSAO Intensity", "Final weight of the SSAO pass.", "" })

        .data<&comp::singl::rendering_manager::outline_color> (
            "outline_color"_hs)
        .custom<comp::meta_info> (meta_info{
            "Outline Color", "Color used for selected-object outlines.", "" })

        .data<&comp::singl::rendering_manager::outline_alpha> (
            "outline_alpha"_hs)
        .custom<comp::meta_info> (meta_info{
            "Outline Alpha", "Alpha used for selected-object outlines.", "" })

        .data<&comp::singl::rendering_manager::outline_width> (
            "outline_width"_hs)
        .custom<comp::meta_info> (meta_info{
            "Outline Width", "Thickness of selected-object outlines.", "" })

        .data<&comp::singl::rendering_manager::shadow_bias> ("shadow_bias"_hs)
        .custom<comp::meta_info> (meta_info{
            "Shadow Bias", "Default depth bias used for shadow maps.", "" })

        .data<&comp::singl::rendering_manager::shadow_strength> (
            "shadow_strength"_hs)
        .custom<comp::meta_info> (meta_info{
            "Shadow Strength", "Default shadow darkness multiplier.", "" })

        .data<&comp::singl::rendering_manager::render_viewport> (
            "render_viewport"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Render Viewport",
                       "Active viewport entity used for the game view.", "" })

        .data<&comp::singl::rendering_manager::root_viewport_virtual_size> (
            "root_viewport_virtual_size"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Root Viewport Virtual Size",
                       "Design size for the root viewport used in the editor "
                       "2D game view.",
                       "" });

    entt::meta_factory<rsc::cubemap_id> ()
        .type (entt::type_hash<rsc::cubemap_id>::value ())
        .custom<comp::meta_info> (
            meta_info{ "Cubemap", "Cubemap resource identifier.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    rendering_manager def{};
    serialize_field_if_diff (archive, "skybox", skybox.value, def.skybox.value);
    serialize_field_if_diff (archive, "skybox_rotation", skybox_rotation,
                             def.skybox_rotation);
    serialize_field_if_diff (archive, "clear_color", clear_color,
                             def.clear_color);
    serialize_field_if_diff (archive, "clear_alpha", clear_alpha,
                             def.clear_alpha);
    serialize_field_if_diff (archive, "ambient_color", ambient_color,
                             def.ambient_color);
    serialize_field_if_diff (archive, "ambient_intensity", ambient_intensity,
                             def.ambient_intensity);
    serialize_field_if_diff (archive, "sun_altitude", sun_altitude,
                             def.sun_altitude);
    serialize_field_if_diff (archive, "sun_azimuth", sun_azimuth,
                             def.sun_azimuth);
    serialize_field_if_diff (archive, "exposure", exposure, def.exposure);
    serialize_field_if_diff (archive, "bloom_threshold", bloom_threshold,
                             def.bloom_threshold);
    serialize_field_if_diff (archive, "bloom_knee", bloom_knee, def.bloom_knee);
    serialize_field_if_diff (archive, "bloom_intensity", bloom_intensity,
                             def.bloom_intensity);
    serialize_field_if_diff (archive, "ibl_intensity", ibl_intensity,
                             def.ibl_intensity);
    serialize_field_if_diff (archive, "ssao_enabled", ssao_enabled,
                             def.ssao_enabled);
    serialize_field_if_diff (archive, "ssao_radius", ssao_radius,
                             def.ssao_radius);
    serialize_field_if_diff (archive, "ssao_bias", ssao_bias, def.ssao_bias);
    serialize_field_if_diff (archive, "ssao_power", ssao_power, def.ssao_power);
    serialize_field_if_diff (archive, "ssao_intensity", ssao_intensity,
                             def.ssao_intensity);
    serialize_field_if_diff (archive, "outline_color", outline_color,
                             def.outline_color);
    serialize_field_if_diff (archive, "outline_alpha", outline_alpha,
                             def.outline_alpha);
    serialize_field_if_diff (archive, "outline_width", outline_width,
                             def.outline_width);
    serialize_field_if_diff (archive, "shadow_bias", shadow_bias,
                             def.shadow_bias);
    serialize_field_if_diff (archive, "shadow_strength", shadow_strength,
                             def.shadow_strength);
    serialize_field_if_diff (archive, "viewports", viewports, def.viewports);
    serialize_field_if_diff (archive, "render_viewport", render_viewport,
                             def.render_viewport);
    serialize_field_if_diff (archive, "root_viewport_virtual_size",
                             root_viewport_virtual_size,
                             def.root_viewport_virtual_size);
  }
};

} // namespace comp::singl

} // namespace wsl
