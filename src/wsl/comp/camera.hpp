#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "component_meta.hpp"
#include "world_transform.hpp"

namespace wsl::comp::singl
{
class runtime_context;
}

namespace wsl
{

namespace comp
{

struct camera : world_component
{
private:
  float m_fov = 60.0F;
  float m_near = 0.5F;
  float m_far = 50.0F;
  float m_aspect_ratio = 1.0F;

  bool m_only_for_editor = false;

public:
  float
  fov () const
  {
    return m_fov;
  }
  float &
  fov ()
  {
    return m_fov;
  }

  float
  near () const
  {
    return m_near;
  }
  float &
  near ()
  {
    return m_near;
  }

  float
  far () const
  {
    return m_far;
  }
  float &
  far ()
  {
    return m_far;
  }

  float
  aspect_ratio () const
  {
    return m_aspect_ratio;
  }
  float &
  aspect_ratio ()
  {
    return m_aspect_ratio;
  }

  bool
  only_for_editor () const
  {
    return m_only_for_editor;
  }
  bool &
  only_for_editor ()
  {
    return m_only_for_editor;
  }

  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx);

  static glm::mat4
  view (const world_transform &transform)
  {
    return glm::inverse (static_cast<glm::mat4> (transform.value ()));
  }

  glm::mat4
  proj () const
  {
    return glm::perspective (glm::radians (m_fov), m_aspect_ratio, m_near,
                             m_far);
  }

  static bool
  has (entt::registry &registry, entt::entity entity)
  {
    return registry.all_of<camera> (entity);
  }

  static camera &
  get (entt::registry &registry, entt::entity entity)
  {
    return registry.get<camera> (entity);
  }

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    camera def{};
    serialize_field_if_diff (archive, "fov", m_fov, def.m_fov);
    serialize_field_if_diff (archive, "near", m_near, def.m_near);
    serialize_field_if_diff (archive, "far", m_far, def.m_far);
    serialize_field_if_diff (archive, "aspect_ratio", m_aspect_ratio,
                             def.m_aspect_ratio);
    serialize_field_if_diff (archive, "only_for_editor", m_only_for_editor,
                             def.m_only_for_editor);
  }
};

} // namespace comp

} // namespace wsl
