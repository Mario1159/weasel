#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "component_meta.hpp"
#include "world_transform.hpp"

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

  static glm::mat4
  view (const world_transform &wt)
  {
    // camera looks along -Z in its local space
    return glm::inverse (wt.value);
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

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::camera> ()
        .type (entt::type_hash<comp::camera>::value ())
        .custom<comp::meta_info> (meta_info{
            "Camera", "Projection parameters only", "engine://icons/comp_camera.svg" })
        .data<&camera::fov> ("fov"_hs)
        .custom<comp::meta_info> (
            meta_info{ "FOV", "Field of view in degrees", "" })
        .data<&camera::near> ("near"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Near", "Near clipping plane", "" })
        .data<&camera::far> ("far"_hs)
        .custom<comp::meta_info> (meta_info{ "Far", "Far clipping plane", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("fov", fov), cereal::make_nvp ("near", near),
             cereal::make_nvp ("far", far),
             cereal::make_nvp ("aspect_ratio", aspect_ratio),
             cereal::make_nvp ("only_for_editor", only_for_editor));
  }
};

} // namespace comp

} // namespace wsl
