#include "ray.hpp"

#include "comp/camera.hpp"
#include "comp/world_transform.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace wsl
{

pick_ray
make_pick_ray (entt::registry &registry, entt::entity camera_entity,
               float mouse_x, float mouse_y, float vp_x, float vp_y, float vp_w,
               float vp_h)
{
  auto const &cam = registry.get<comp::camera> (camera_entity);
  auto const &wt = registry.get<comp::world_transform> (camera_entity);

  glm::mat4 const view = comp::camera::view (wt);
  glm::mat4 const proj = cam.proj ();
  glm::mat4 const inv_vp = glm::inverse (proj * view);

  // Screen-space -> NDC
  float const nx = ((mouse_x - vp_x) / vp_w) * 2.0F - 1.0F;
  float const ny = 1.0F - ((mouse_y - vp_y) / vp_h) * 2.0F;

  // Far-plane point in clip space -> world space
  glm::vec4 const far_clip (nx, ny, 1.0F, 1.0F);
  glm::vec4 far_world4 = inv_vp * far_clip;
  far_world4 /= far_world4.w;

  // Camera world position from the transform matrix (4th column)
  glm::mat4 const wt_mat = static_cast<glm::mat4> (wt.value ());
  glm::vec3 const cam_pos (wt_mat[3]);

  pick_ray ray;
  ray.origin = cam_pos;
  ray.dir = glm::normalize (glm::vec3 (far_world4) - cam_pos);
  return ray;
}

bool
ray_plane_intersect (const pick_ray &ray, const glm::vec3 &plane_point,
                     const glm::vec3 &plane_normal, float &t_hit,
                     glm::vec3 &hit_point)
{
  float const denom = glm::dot (plane_normal, ray.dir);
  if (std::abs (denom) < 1e-8F) {
    return false;
  }

  float const t = glm::dot (plane_point - ray.origin, plane_normal) / denom;
  if (t < 0.0F) {
    return false;
  }

  t_hit = t;
  hit_point = ray.origin + t * ray.dir;
  return true;
}

} // namespace wsl
