#pragma once

#include "../../gfx/scene_renderer.hpp"
#include "../../rsc/resource_manager.hpp"
#include "../component_meta.hpp"
#include "../transform.hpp"

#include <cereal/cereal.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace wsl
{

namespace comp::singl
{

struct rendering_manager : singleton_component
{
  std::unique_ptr<gfx::scene_renderer> renderer;
  rsc::cubemap_id skybox{};

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
            "Shadow Strength", "Default shadow darkness multiplier.", "" });

    entt::meta_factory<rsc::cubemap_id> ()
        .type (entt::type_hash<rsc::cubemap_id>::value ())
        .custom<comp::meta_info> (
            meta_info{ "Cubemap", "Cubemap resource identifier.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("skybox", skybox.value),
             cereal::make_nvp ("clear_color", clear_color),
             cereal::make_nvp ("clear_alpha", clear_alpha),
             cereal::make_nvp ("ambient_color", ambient_color),
             cereal::make_nvp ("ambient_intensity", ambient_intensity),
             cereal::make_nvp ("sun_altitude", sun_altitude),
             cereal::make_nvp ("sun_azimuth", sun_azimuth),
             cereal::make_nvp ("exposure", exposure),
             cereal::make_nvp ("bloom_threshold", bloom_threshold),
             cereal::make_nvp ("bloom_knee", bloom_knee),
             cereal::make_nvp ("bloom_intensity", bloom_intensity),
             cereal::make_nvp ("ibl_intensity", ibl_intensity),
             cereal::make_nvp ("ssao_enabled", ssao_enabled),
             cereal::make_nvp ("ssao_radius", ssao_radius),
             cereal::make_nvp ("ssao_bias", ssao_bias),
             cereal::make_nvp ("ssao_power", ssao_power),
             cereal::make_nvp ("ssao_intensity", ssao_intensity),
             cereal::make_nvp ("outline_color", outline_color),
             cereal::make_nvp ("outline_alpha", outline_alpha),
             cereal::make_nvp ("outline_width", outline_width),
             cereal::make_nvp ("shadow_bias", shadow_bias),
             cereal::make_nvp ("shadow_strength", shadow_strength));
  }
};

} // namespace comp::singl

} // namespace wsl
