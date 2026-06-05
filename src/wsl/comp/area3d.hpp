#pragma once

#include "../math/vector.hpp"         // math::vec3f, math::quatf
#include "../phys/physics_engine.hpp" // phys::engine
#include <cereal/cereal.hpp>
#include <glm/glm.hpp>

#include <entt/entt.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

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
    JPH::BodyID other_body;
  };

  struct exited
  {
    entt::entity area_entity{ entt::null };
    entt::entity other_entity{ entt::null };
    JPH::BodyID other_body;
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
    half_extents.x = std::max (half_extents.x, 1e-3F);
    half_extents.y = std::max (half_extents.y, 1e-3F);
    half_extents.z = std::max (half_extents.z, 1e-3F);
    radius = std::max (radius, 1e-3F);
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::area::shape_type> ()
        .type (entt::type_hash<comp::area::shape_type>::value ())
        .conv<int> ()
        .data<comp::area::shape_type::box> ("box"_hs)
        .custom<const char *> ("Box")
        .data<comp::area::shape_type::sphere> ("sphere"_hs)
        .custom<const char *> ("Sphere");

    entt::meta_factory<comp::area> ()
        .type (entt::type_hash<comp::area>::value ())
        .custom<comp::meta_info> (meta_info{
            "Area3D", "Jolt sensor (trigger) to detect bodies entering/exiting",
            "" })
        .func<&comp::area::on_inspector_changed> ("on_inspector_changed"_hs)

        .data<&comp::area::shape> ("shape"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Shape", "Sensor collision shape", "" })

        .data<&comp::area::position> ("position"_hs)
        .custom<comp::meta_info> (meta_info{
            "Position",
            "Local position offset relative to the entity's Transform position",
            "" })

        .data<&comp::area::rotation> ("rotation"_hs)
        .custom<comp::meta_info> (meta_info{
            "Rotation",
            "Local rotation offset relative to the entity's Transform rotation",
            "" })

        .data<&comp::area::half_extents> ("half_extents"_hs)
        .custom<comp::meta_info> (meta_info{
            "Half Extents",
            "Box half size (scaled by the entity's Transform scale)", "" })

        .data<&comp::area::radius> ("radius"_hs)
        .custom<comp::meta_info> (meta_info{
            "Radius", "Sphere radius (scaled by the entity's Transform scale)",
            "" });
  }

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    int shape_i = (int)shape;
    ar (cereal::make_nvp ("shape", shape_i),
        cereal::make_nvp ("position", position),
        cereal::make_nvp ("rotation", rotation),
        cereal::make_nvp ("half_extents", half_extents),
        cereal::make_nvp ("radius", radius));

    if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase,
                                    Archive>) {
      shape = (shape_type)shape_i;

      sanitize_dimensions ();

      body_id = phys::body_id{};

      sync_applied_cache ();
    }
  }
};

} // namespace comp

} // namespace wsl
