// character_body.hpp
#pragma once

#include <memory>

#include "../math/vector.hpp"
#include "../phys/physics_engine.hpp"
#include "component_meta.hpp"
namespace wsl::comp::singl
{
class runtime_context;
}
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif

namespace wsl
{

namespace comp
{

class character_body : public world_component
{
public:
  character_body ();
  ~character_body ();
  character_body (character_body &&) noexcept;
  character_body &operator= (character_body &&) noexcept;

  character_body (phys::engine &physics, const glm::vec3 &position,
                  float height = 1.8F, float radius = 0.4F);

  // runtime ops
  void create_body (phys::engine &physics, const glm::vec3 &position);
  void destroy_body ();

  // must be called after load
  void recreate (phys::engine &physics, const glm::vec3 &position);

  // called by inspector after any field edit
  void on_inspector_changed (comp::singl::runtime_context *runtime,
                             const glm::vec3 &scale = { 1, 1, 1 });

  bool valid () const noexcept;
  phys::body_id get_id () const noexcept;
  void *native_handle () noexcept;
  const void *native_handle () const noexcept;

  float height = 1.8F;
  float radius = 0.4F;
  math::vec3f desired_velocity = math::vec3f{ 0.0F, 0.0F, 0.0F };

  static void register_meta ();

private:
  static constexpr float min_half_height = 1e-3F;
  static constexpr float min_radius = 1e-3F;

  static void sanitize_dimensions (float &height, float &radius);
  static float capsule_half_height (float height, float radius);

  struct impl;
  std::unique_ptr<impl> m_impl;

  float m_applied_height = 1.8F;
  float m_applied_radius = 0.4F;
};

} // namespace comp

} // namespace wsl
