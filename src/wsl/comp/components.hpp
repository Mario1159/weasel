#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "area3d.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "audio.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "camera_2d.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "camera.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "character_body.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "directional_light.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "hierarchy.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "model_instance_3d.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "point_light.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "sprite_2d.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "prefab_instance.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "rigid_body.hpp"
#endif
#include "singl/ui_manager.hpp"
#include "singl/physics_manager.hpp"
#include "singl/rendering_manager.hpp"
#if !defined(WSL_MODULE_BUILD)
#include "subviewport.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "transform_2d.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "wsl/comp/singl/editor_context.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "spot_light.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "transform.hpp"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "world_transform.hpp"
#endif
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
                      camera, camera_2d, point_light, spot_light,
                      directional_light, rigid_body, area, character_body,
                      audio, prefab_instance, sprite_2d, subviewport,
                      transform_2d>;

using singleton_types
    = entt::type_list<comp::singl::runtime_context, comp::singl::editor_context,
                      rsc::scene_manager, rsc::resource_manager_view,
                      comp::singl::ui_manager, comp::singl::rendering_manager,
                      comp::singl::physics_manager>;

} // namespace comp

} // namespace wsl
