#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "component_meta.hpp"
#include "world_transform.hpp"

namespace comp::singl
{
class runtime_context;
}

namespace wsl
{

namespace comp
{

struct camera : world_component
{
  float fov = 60.0F;
  float near = 0.5F;
  float far = 50.0F;
  float aspect_ratio = 1.0F;

  bool only_for_editor = false;

  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx);

  static glm::mat4
  view (const world_transform &wt)
  {
    // camera looks along -Z in its local space
    return glm::inverse (static_cast<glm::mat4> (wt.value));
  }

  glm::mat4
  proj () const
  {
    return glm::perspective (glm::radians (fov), aspect_ratio, near, far);
  }

  static bool
  has (entt::registry &r, entt::entity e)
  {
    return r.all_of<camera> (e);
  }

  static camera &
  get (entt::registry &r, entt::entity e)
  {
    return r.get<camera> (e);
  }

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    camera def{};
    serialize_field_if_diff (archive, "fov", fov, def.fov);
    serialize_field_if_diff (archive, "near", near, def.near);
    serialize_field_if_diff (archive, "far", far, def.far);
    serialize_field_if_diff (archive, "aspect_ratio", aspect_ratio,
                             def.aspect_ratio);
    serialize_field_if_diff (archive, "only_for_editor", only_for_editor,
                             def.only_for_editor);
  }
};

} // namespace comp

} // namespace wsl
