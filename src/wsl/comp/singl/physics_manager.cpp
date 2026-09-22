#include "physics_manager.hpp"

#include "wsl/comp/singl/runtime_context.hpp"

namespace wsl
{
namespace comp::singl
{

void
physics_manager::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::singl::physics_manager> ()
      .type (entt::type_hash<comp::singl::physics_manager>::value ())
      .custom<comp::meta_info> (
          comp::meta_info{ "Physics Manager",
                           "Scene-owned physics settings and runtime "
                           "simulation state.",
                           "" })
      .func<&comp::singl::physics_manager::on_inspector_changed> (
          "on_inspector_changed"_hs)

      .data<&comp::singl::physics_manager::gravity> ("gravity"_hs)
      .custom<comp::meta_info> (
          comp::meta_info{ "Gravity", "Downward acceleration in m/s^2.", "" })

      .data<&comp::singl::physics_manager::fixed_timestep> ("fixed_timestep"_hs)
      .custom<comp::meta_info> (
          comp::meta_info{ "Fixed Timestep",
                           "Simulation tick length used by the physics "
                           "world.",
                           "" })

      .data<&comp::singl::physics_manager::max_frame_time> ("max_frame_time"_hs)
      .custom<comp::meta_info> (
          comp::meta_info{ "Max Frame Time",
                           "Clamp applied before physics catch-up to avoid "
                           "spiral-of-death frames.",
                           "" })

      .data<&comp::singl::physics_manager::max_substeps> ("max_substeps"_hs)
      .custom<comp::meta_info> (
          comp::meta_info{ "Max Substeps",
                           "Maximum fixed simulation steps allowed per "
                           "frame.",
                           "" })
      .data<&comp::singl::physics_manager::show_debug> ("show_debug"_hs)
      .custom<comp::meta_info> (
          comp::meta_info{ "Show Debug", "Show physics debug renderer.", "" });
}

} // namespace comp::singl
} // namespace wsl
