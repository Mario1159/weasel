#pragma once

#ifndef IN_MODULE_INTERFACE
#include "../math/vector.hpp" // math::vec3f, math::quatf
#endif
#ifndef IN_MODULE_INTERFACE
#include "../phys/physics_engine.hpp" // phys::engine
#include "../phys/layers.hpp"
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif

namespace wsl
{

namespace comp::singl
{
class runtime_context;
}

namespace comp
{

struct area : world_component
{
  struct entered
  {
    entt::entity area_entity{ entt::null };
    entt::entity other_entity{ entt::null };
    phys::body_id other_body = phys::null_body_id;
  };

  struct exited
  {
    entt::entity area_entity{ entt::null };
    entt::entity other_entity{ entt::null };
    phys::body_id other_body = phys::null_body_id;
  };

  enum class shape_type
  {
    box = 0,
    sphere = 1
  };

  // --- authored state ---
  shape_type shape = shape_type::box;

  math::vec3f position{ 0, 0, 0 };
  math::quatf rotation{ 0, 0, 0, 1 };

  math::vec3f half_extents{ 0.5F, 0.5F, 0.5F };
  float radius = 0.5F;

  // runtime handle (NOT serialized)
  phys::body_id body_id;

  // --- runtime cache (editor-only, not serialized) ---
  shape_type applied_shape = shape_type::box;
  math::vec3f applied_half_extents{ 0.5F, 0.5F, 0.5F };
  float applied_radius = 0.5F;
  math::vec3f applied_position{ 0, 0, 0 };
  math::quatf applied_rotation{ 0, 0, 0, 1 };

  // runtime ops
  // world_pos and world_rot must be the entity's world-space position/rotation
  // (derived from its transform component). This body is offset by
  // position/rotation.
  void create_body (phys::engine &engine, const glm::vec3 &world_pos,
                    const glm::quat &world_rot,
                    const glm::vec3 &scale = { 1, 1, 1 });
  void destroy_body (phys::engine &engine);
  void rebuild_body (phys::engine &engine, const glm::vec3 &world_pos,
                     const glm::quat &world_rot,
                     const glm::vec3 &scale = { 1, 1, 1 });
  void apply_transform_to_body (phys::engine &engine) const;

  // called by inspector after any field edit
  void on_inspector_changed (comp::singl::runtime_context *runtime,
                             const glm::vec3 &scale = { 1, 1, 1 });

  // sync current authored values into applied_* cache
  void sync_applied_cache ();
  bool has_structural_change () const;
  bool has_transform_change () const;

  void
  sanitize_dimensions ()
  {
    half_extents.x () = std::max (half_extents.x (), 1e-3F);
    half_extents.y () = std::max (half_extents.y (), 1e-3F);
    half_extents.z () = std::max (half_extents.z (), 1e-3F);
    radius = std::max (radius, 1e-3F);
  }

  static void register_meta ();
};

} // namespace comp

} // namespace wsl
