#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace wsl
{

constexpr int max_point_lights = 16;
constexpr int max_dir_lights = 4;
constexpr int max_spot_lights = 8;

struct alignas (16) gpu_point_light
{
  glm::vec4 pos_radius;      // xyz = pos, w = radius
  glm::vec4 color_intensity; // rgb = color, w = intensity
  glm::ivec4 shadow_info;    // x = shadow index, y = cast_shadows, z/w unused
};

struct alignas (16) gpu_dir_light
{
  glm::vec4 dir_pad;         // xyz = dir, w unused
  glm::vec4 color_intensity; // rgb = color, w = intensity
};

struct alignas (16) gpu_spot_light
{
  glm::vec4 pos_pad;         // xyz = pos
  glm::vec4 dir_pad;         // xyz = dir
  glm::vec4 color_intensity; // rgb color, w intensity
  glm::vec4 angles_pad;      // x = inner_cos, y = outer_cos
};

constexpr int max_shadowed_dir_lights = 1;
constexpr int max_shadowed_spot_lights = 4;
constexpr int max_shadowed_point_lights = 2;

struct alignas (16) gpu_shadowed_dir
{
  glm::mat4 light_view_proj;
  glm::vec4 params; // x=bias, y=strength, z=map_size, w=enabled
};

struct alignas (16) gpu_shadowed_spot
{
  glm::mat4 light_view_proj;
  glm::vec4 params; // x=bias, y=strength, z=map_size, w=enabled
};

struct alignas (16) gpu_shadowed_point
{
  glm::vec4 pos_far; // xyz=light pos, w=far plane
  glm::vec4 params;  // x=bias, y=strength, z=enabled, w=unused
};

struct alignas (16) lighting_ubo
{
  // x = num_dir, y = num_spot. Point lights are no longer in this UBO;
  // they live in the clustered-light SSBO and are shaded via the cluster
  // grid in `cube.frag.slang`.
  glm::ivec4 counts;

  gpu_dir_light dirs[max_dir_lights];
  gpu_spot_light spots[max_spot_lights];

  glm::vec4 camera_pos;
  glm::vec4 ambient;

  gpu_shadowed_dir shadowed_dirs[max_shadowed_dir_lights];
  gpu_shadowed_spot shadowed_spots[max_shadowed_spot_lights];
  gpu_shadowed_point shadowed_points[max_shadowed_point_lights];
};

struct shadow_map_2d
{
  SDL_GPUTexture *depth = nullptr;
  glm::mat4 light_vp = glm::mat4 (1.0F);
  float bias = 0.0025F;
  float strength = 1.0F;
  float map_size = 2048.0F;
  bool enabled = false;
};

struct point_shadow_map
{
  SDL_GPUTexture *depth_cube = nullptr;
  glm::vec3 light_pos = glm::vec3 (0.0F);
  float strength = 1.0F;
  float near_plane = 0.1F;
  float far_plane = 25.0F;
  float bias = 0.02F;
  bool enabled = false;
};

} // namespace wsl
