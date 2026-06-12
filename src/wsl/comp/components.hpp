#pragma once

#include "area3d.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "character_body.hpp"
#include "directional_light.hpp"
#include "hierarchy.hpp"
#include "model_instance_3d.hpp"
#include "point_light.hpp"
#include "sprite_2d.hpp"
#include "prefab_instance.hpp"
#include "rigid_body.hpp"
#include "singl/ui_manager.hpp"
#include "singl/physics_manager.hpp"
#include "singl/rendering_manager.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "spot_light.hpp"
#include "transform.hpp"
#include "world_transform.hpp"
#include "../rsc/resource_manager.hpp"
#include "../rsc/scene_manager.hpp"

namespace wsl
{

namespace comp
{

template <typename List> struct for_each_type;

template <typename... Types> struct for_each_type<entt::type_list<Types...>>
{
  template <typename Func>
  static void
  apply (Func func)
  {
    (func.template operator()<Types> (), ...);
  }
};

using component_types
    = entt::type_list<hierarchy, world_transform, transform, model_instance_3d,
                      camera, point_light, spot_light, directional_light,
                      rigid_body, area, character_body, audio,
                      prefab_instance, sprite_2d>;

using singleton_types
    = entt::type_list<comp::singl::runtime_context, comp::singl::editor_context,
                      rsc::scene_manager, rsc::resource_manager_view,
                      comp::singl::ui_manager,
                      comp::singl::rendering_manager,
                      comp::singl::physics_manager>;

} // namespace comp

} // namespace wsl
