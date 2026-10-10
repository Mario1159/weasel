#pragma once

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/glm.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/gtc/matrix_transform.hpp>
#endif

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

// windows.h (pulled in by SDL/RmlUi/Windows SDK headers) defines near/far
// as empty macros, which break these member declarations whenever such a
// header precedes this one. Save and clear them for this header only;
// Winsock's FD_* macros (which expand through FAR) are unaffected because
// no socket code lives here.
#ifdef near
#pragma push_macro("near")
#undef near
#define WSL_CAMERA_POP_NEAR
#endif
#ifdef far
#pragma push_macro("far")
#undef far
#define WSL_CAMERA_POP_FAR
#endif

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
};

#ifdef WSL_CAMERA_POP_FAR
#pragma pop_macro("far")
#undef WSL_CAMERA_POP_FAR
#endif
#ifdef WSL_CAMERA_POP_NEAR
#pragma pop_macro("near")
#undef WSL_CAMERA_POP_NEAR
#endif

} // namespace comp

} // namespace wsl
