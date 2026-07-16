#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace wsl
{

struct pick_ray
{
  glm::vec3 origin;
  glm::vec3 dir;
};

/*!
 * \brief Build a pick ray from mouse coordinates through a 3D camera.
 *
 * Unprojects the mouse position using the camera entity's view and
 * projection matrices (derived from its world_transform and camera
 * component).
 *
 * \param registry   ECS registry owning the camera entity.
 * \param camera     Entity with comp::camera + comp::world_transform.
 * \param mouse_x    Mouse X in screen/viewport pixels.
 * \param mouse_y    Mouse Y in screen/viewport pixels.
 * \param vp_x       Viewport top-left X in screen pixels.
 * \param vp_y       Viewport top-left Y in screen pixels.
 * \param vp_w       Viewport width in pixels.
 * \param vp_h       Viewport height in pixels.
 */
pick_ray make_pick_ray (entt::registry &registry, entt::entity camera,
                        float mouse_x, float mouse_y, float vp_x, float vp_y,
                        float vp_w, float vp_h);

/*!
 * \brief Intersect a pick ray with an infinite plane.
 *
 * The plane is defined by a point on the plane and its outward normal.
 *
 * \param ray         The pick ray.
 * \param plane_point  A point on the plane.
 * \param plane_normal The plane's outward normal (must be unit length).
 * \param t_hit        Output — parametric distance along the ray.
 * \param hit_point    Output — world-space intersection point.
 *
 * \return true if the ray intersects the plane (t_hit >= 0).
 */
bool ray_plane_intersect (const pick_ray &ray, const glm::vec3 &plane_point,
                          const glm::vec3 &plane_normal, float &t_hit,
                          glm::vec3 &hit_point);

} // namespace wsl
