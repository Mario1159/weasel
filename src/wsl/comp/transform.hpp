#pragma once

#include "../math/vector.hpp"
#include "component_meta.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <cereal/cereal.hpp>

namespace wsl
{

namespace comp
{

struct transform : world_component
{
  transform () = default;

  explicit transform (const math::vec3f &position_value)
    : position (position_value)
  {
  }

  explicit transform (const glm::vec3 &position_value)
    : position (position_value)
  {
  }

  transform (const math::vec3f &position_value,
             const math::quatf &rotation_value,
             const math::vec3f &scale_value)
    : position (position_value), rotation (rotation_value), scale (scale_value)
  {
  }

  math::vec3f position{ 0, 0, 0 };
  math::quatf rotation{ 0, 0, 0, 1 };
  math::vec3f scale{ 1, 1, 1 };

  math::vec3f
  get_rotation_xyz () const
  {
    glm::vec3 const euler
        = glm::degrees (glm::eulerAngles ((glm::quat)rotation));
    return math::vec3f{ euler };
  }

  void
  set_rotation_xyz (const math::vec3f &deg)
  {
    glm::vec3 const rad = glm::radians ((glm::vec3)deg);
    glm::quat const q = glm::quat (rad);
    rotation = math::quatf{ q };
  }

  glm::mat4
  model () const
  {
    glm::mat4 m (1.0F);

    m = glm::translate (m, (glm::vec3)position);
    m *= glm::mat4_cast ((glm::quat)rotation);
    m = glm::scale (m, (glm::vec3)scale);
    return m;
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::transform> ()
        .type (entt::type_hash<comp::transform>::value ())
        .custom<comp::meta_info> (meta_info{
            "Transform",
            "Controls local position, rotation and scale relative to parent",
            "engine://icons/comp_transform.svg" })

        .data<&comp::transform::position> ("position"_hs)
        .custom<comp::meta_info> (meta_info{
            "Position", "Local position relative to parent entity", "" })

        .data<&comp::transform::set_rotation_xyz,
              &comp::transform::get_rotation_xyz> ("rotation"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Rotation", "Euler XYZ in degrees", "" })

        .data<&comp::transform::scale> ("scale"_hs)
        .custom<comp::meta_info> (meta_info{
            "Scale", "Local non-uniform scale applied after rotation", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("position", position),
             cereal::make_nvp ("rotation", rotation),
             cereal::make_nvp ("scale", scale));
  }
};

} // namespace comp

} // namespace wsl
