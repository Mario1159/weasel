#pragma once

// Implementation-only: reflect-cpp adapters for engine component and
// singleton types that cannot be auto-reflected (non-aggregates, resource
// handles that serialize as paths, third-party value types).
//
// NEVER include from engine headers (module purview) — .cpp files only.
// Consumed by reg/*.cpp and rsc/*.cpp translation units.

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <rfl.hpp>

#include "adapters.hpp"

#include "../comp/audio.hpp"
#include "../comp/camera.hpp"
#include "../comp/character_body.hpp"
#include "../comp/model_instance_3d.hpp"
#include "../comp/prefab_instance.hpp"
#include "../comp/transform.hpp"
#include "../comp/world_transform.hpp"
#include "../comp/singl/physics_manager.hpp"
#include "../comp/singl/rendering_manager.hpp"
#include "../comp/singl/skybox_instance_3d.hpp"
#include "../rsc/resource_ids.hpp"
#include "../rsc/resource_manager.hpp"
#include "../gfx/material_asset.hpp"

namespace rfl
{

// --- SDL ------------------------------------------------------------------

template <> struct Reflector<SDL_FColor>
{
  struct ReflType
  {
    float r;
    float g;
    float b;
    float a;
  };
  static SDL_FColor
  to (const ReflType &v) noexcept
  {
    return { v.r, v.g, v.b, v.a };
  }
  static ReflType
  from (const SDL_FColor &v)
  {
    return { v.r, v.g, v.b, v.a };
  }
};

// --- Jolt -----------------------------------------------------------------

/** Jolt body ids are runtime state; round-trip the raw value harmlessly. */
template <> struct Reflector<JPH::BodyID>
{
  using ReflType = std::uint32_t;
  static JPH::BodyID
  to (const ReflType &v) noexcept
  {
    return JPH::BodyID (v);
  }
  static ReflType
  from (const JPH::BodyID &v)
  {
    return v.GetIndexAndSequenceNumber ();
  }
};

// --- world components -------------------------------------------------------

template <> struct Reflector<wsl::comp::transform>
{
  struct ReflType
  {
    wsl::math::vec3f position;
    wsl::math::quatf rotation;
    wsl::math::vec3f scale;
  };
  static wsl::comp::transform
  to (const ReflType &v) noexcept
  {
    wsl::comp::transform t;
    t.position = v.position;
    t.rotation = v.rotation;
    t.scale = v.scale;
    return t;
  }
  static ReflType
  from (const wsl::comp::transform &t)
  {
    return { t.position, t.rotation, t.scale };
  }
};

template <> struct Reflector<wsl::comp::world_transform>
{
  struct ReflType
  {
    wsl::math::mat44f matrix;
  };
  static wsl::comp::world_transform
  to (const ReflType &v)
  {
    wsl::comp::world_transform t;
    t.value () = v.matrix;
    return t;
  }
  static ReflType
  from (const wsl::comp::world_transform &t)
  {
    return { t.value () };
  }
};

template <> struct Reflector<wsl::comp::camera>
{
  struct ReflType
  {
    float fov;
    float near;
    float far;
    float aspect_ratio;
    bool only_for_editor;
  };
  static wsl::comp::camera
  to (const ReflType &v) noexcept
  {
    wsl::comp::camera c;
    c.fov () = v.fov;
    c.near () = v.near;
    c.far () = v.far;
    c.aspect_ratio () = v.aspect_ratio;
    c.only_for_editor () = v.only_for_editor;
    return c;
  }
  static ReflType
  from (const wsl::comp::camera &c)
  {
    return { c.fov (), c.near (), c.far (), c.aspect_ratio (),
             c.only_for_editor () };
  }
};

template <> struct Reflector<wsl::comp::character_body>
{
  struct ReflType
  {
    float height;
    float radius;
    wsl::math::vec3f desired_velocity;
  };
  static wsl::comp::character_body
  to (const ReflType &v)
  {
    wsl::comp::character_body c;
    c.height = v.height;
    c.radius = v.radius;
    c.desired_velocity = v.desired_velocity;
    return c;
  }
  static ReflType
  from (const wsl::comp::character_body &c)
  {
    return { c.height, c.radius, c.desired_velocity };
  }
};

template <> struct Reflector<wsl::comp::prefab_instance>
{
  struct ReflType
  {
    std::uint64_t prefab_id;
    std::uint32_t prefab_entity;
  };
  static wsl::comp::prefab_instance
  to (const ReflType &v)
  {
    wsl::comp::prefab_instance p;
    p.prefab_id.value = v.prefab_id;
    p.prefab_entity = static_cast<entt::entity> (v.prefab_entity);
    return p;
  }
  static ReflType
  from (const wsl::comp::prefab_instance &p)
  {
    return { p.prefab_id.value,
             static_cast<std::uint32_t> (entt::to_integral (p.prefab_entity)) };
  }
};

/** Audio component: the audio resource handle round-trips as a project path. */
template <> struct Reflector<wsl::comp::audio>
{
  struct ReflType
  {
    std::string audio_path = "None";
    bool loop = false;
    bool play_on_start = true;
    float volume = 1.0F;
  };
  static wsl::comp::audio
  to (const ReflType &v)
  {
    wsl::comp::audio a{};
    a.loop = v.loop;
    a.play_on_start = v.play_on_start;
    a.volume = v.volume;

    auto *mgr = wsl::rsc::resource_manager::serialization_context::get ();
    if (mgr != nullptr && !v.audio_path.empty () && v.audio_path != "None") {
      a.audio_resource = mgr->register_audio (v.audio_path);
    }
    return a;
  }
  static ReflType
  from (const wsl::comp::audio &a)
  {
    ReflType v;
    v.loop = a.loop;
    v.play_on_start = a.play_on_start;
    v.volume = a.volume;

    auto *mgr = wsl::rsc::resource_manager::serialization_context::get ();
    if (mgr != nullptr && a.audio_resource.value != entt::null) {
      v.audio_path = mgr->get_resource_path (a.audio_resource);
    }
    return v;
  }
};

/** Model instance: model/material handles round-trip as project paths. */
template <> struct Reflector<wsl::comp::model_instance_3d>
{
  struct ReflType
  {
    std::string model_path = "None";
    std::uint32_t scene_index = 0;
    std::string material_path = "None";
    float mip_lod_bias = 0.0F;
    float geometry_lod_bias = 0.0F;
    float visibility_range = 0.0F;
  };
  static wsl::comp::model_instance_3d
  to (const ReflType &v)
  {
    wsl::comp::model_instance_3d m{};
    m.scene_index = v.scene_index;
    m.mip_lod_bias = v.mip_lod_bias;
    m.geometry_lod_bias = v.geometry_lod_bias;
    m.visibility_range = v.visibility_range;

    auto *mgr = wsl::rsc::resource_manager::serialization_context::get ();
    if (mgr != nullptr) {
      if (!v.model_path.empty () && v.model_path != "None") {
        m.id = mgr->register_model (v.model_path);
      }
      if (!v.material_path.empty () && v.material_path != "None") {
        m.material_override = mgr->register_material (v.material_path);
      }
    }
    return m;
  }
  static ReflType
  from (const wsl::comp::model_instance_3d &m)
  {
    ReflType v;
    v.scene_index = m.scene_index;
    v.mip_lod_bias = m.mip_lod_bias;
    v.geometry_lod_bias = m.geometry_lod_bias;
    v.visibility_range = m.visibility_range;

    auto *mgr = wsl::rsc::resource_manager::serialization_context::get ();
    if (mgr != nullptr) {
      if (m.id.value != entt::null) {
        v.model_path = mgr->get_resource_path (m.id);
      }
      if (m.material_override.value != entt::null) {
        v.material_path = mgr->get_resource_path (m.material_override);
      }
    }
    return v;
  }
};

// --- singletons -------------------------------------------------------------

template <> struct Reflector<wsl::comp::singl::skybox_instance_3d>
{
  struct ReflType
  {
    std::uint64_t cubemap_id = 0;
  };
  static wsl::comp::singl::skybox_instance_3d
  to (const ReflType &v)
  {
    wsl::comp::singl::skybox_instance_3d s{};
    s.id.value = v.cubemap_id;
    wsl::rsc::normalize_resource_id (s.id.value);
    return s;
  }
  static ReflType
  from (const wsl::comp::singl::skybox_instance_3d &s)
  {
    return { s.id.value };
  }
};

template <> struct Reflector<wsl::comp::singl::physics_manager>
{
  struct ReflType
  {
    float gravity;
    float fixed_timestep;
    float max_frame_time;
    int max_substeps;
    bool show_debug;
  };
  static wsl::comp::singl::physics_manager
  to (const ReflType &v)
  {
    wsl::comp::singl::physics_manager p;
    p.gravity = v.gravity;
    p.fixed_timestep = v.fixed_timestep;
    p.max_frame_time = v.max_frame_time;
    p.max_substeps = v.max_substeps;
    p.show_debug = v.show_debug;
    p.post_load ();
    return p;
  }
  static ReflType
  from (const wsl::comp::singl::physics_manager &p)
  {
    return { p.gravity, p.fixed_timestep, p.max_frame_time, p.max_substeps,
             p.show_debug };
  }
};

template <> struct Reflector<wsl::comp::singl::rendering_manager>
{
  struct ReflType
  {
    std::uint64_t skybox = 0;
    wsl::math::quatf skybox_rotation;
    wsl::math::vec3f clear_color;
    float clear_alpha;
    wsl::math::vec3f ambient_color;
    float ambient_intensity;
    float sun_altitude;
    float sun_azimuth;
    float exposure;
    float bloom_threshold;
    float bloom_knee;
    float bloom_intensity;
    float ibl_intensity;
    bool ssao_enabled;
    float ssao_radius;
    float ssao_bias;
    float ssao_power;
    float ssao_intensity;
    wsl::math::vec3f outline_color;
    float outline_alpha;
    float outline_width;
    float shadow_bias;
    float shadow_strength;
    std::vector<wsl::gfx::viewport> viewports;
    std::uint32_t render_viewport;
    wsl::math::vec2f root_viewport_virtual_size;
  };
  static wsl::comp::singl::rendering_manager
  to (const ReflType &v)
  {
    wsl::comp::singl::rendering_manager r;
    r.skybox.value = v.skybox;
    wsl::rsc::normalize_resource_id (r.skybox.value);
    r.skybox_rotation = v.skybox_rotation;
    r.clear_color = v.clear_color;
    r.clear_alpha = v.clear_alpha;
    r.ambient_color = v.ambient_color;
    r.ambient_intensity = v.ambient_intensity;
    r.sun_altitude = v.sun_altitude;
    r.sun_azimuth = v.sun_azimuth;
    r.exposure = v.exposure;
    r.bloom_threshold = v.bloom_threshold;
    r.bloom_knee = v.bloom_knee;
    r.bloom_intensity = v.bloom_intensity;
    r.ibl_intensity = v.ibl_intensity;
    r.ssao_enabled = v.ssao_enabled;
    r.ssao_radius = v.ssao_radius;
    r.ssao_bias = v.ssao_bias;
    r.ssao_power = v.ssao_power;
    r.ssao_intensity = v.ssao_intensity;
    r.outline_color = v.outline_color;
    r.outline_alpha = v.outline_alpha;
    r.outline_width = v.outline_width;
    r.shadow_bias = v.shadow_bias;
    r.shadow_strength = v.shadow_strength;
    r.viewports = v.viewports;
    r.render_viewport = static_cast<entt::entity> (v.render_viewport);
    r.root_viewport_virtual_size = v.root_viewport_virtual_size;
    return r;
  }
  static ReflType
  from (const wsl::comp::singl::rendering_manager &r)
  {
    return ReflType{
      r.skybox.value,
      r.skybox_rotation,
      r.clear_color,
      r.clear_alpha,
      r.ambient_color,
      r.ambient_intensity,
      r.sun_altitude,
      r.sun_azimuth,
      r.exposure,
      r.bloom_threshold,
      r.bloom_knee,
      r.bloom_intensity,
      r.ibl_intensity,
      r.ssao_enabled,
      r.ssao_radius,
      r.ssao_bias,
      r.ssao_power,
      r.ssao_intensity,
      r.outline_color,
      r.outline_alpha,
      r.outline_width,
      r.shadow_bias,
      r.shadow_strength,
      r.viewports,
      static_cast<std::uint32_t> (entt::to_integral (r.render_viewport)),
      r.root_viewport_virtual_size,
    };
  }
};

} // namespace rfl
