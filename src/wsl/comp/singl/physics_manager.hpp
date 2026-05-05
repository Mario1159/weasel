#pragma once

#include "../../phys/physics_engine.hpp"
#include "../component_meta.hpp"

#include <algorithm>
#include <cereal/cereal.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace wsl
{

namespace comp::singl
{

class runtime_context;

struct physics_manager : singleton_component
{
  std::unique_ptr<phys::engine> engine;

  float gravity = -9.8F;
  float fixed_timestep = 1.0F / 60.0F;
  float max_frame_time = 0.25F;
  int max_substeps = 5;
  bool show_debug = false;

  void
  sanitize_settings ()
  {
    fixed_timestep = std::max (fixed_timestep, 1.0e-4F);
    max_frame_time = std::max (max_frame_time, fixed_timestep);
    max_substeps = std::max (max_substeps, 1);
  }

  void
  apply_runtime_settings ()
  {
    sanitize_settings ();

    if (!engine) {
      return;
    }

    engine->set_gravity (gravity);
    engine->set_fixed_step (fixed_timestep);
    engine->set_max_frame_time (max_frame_time);
    engine->set_max_substeps (max_substeps);
  }

  phys::engine &
  ensure_engine ()
  {
    if (!engine) {
      engine = std::make_unique<phys::engine> ();
      apply_runtime_settings ();
    }

    return *engine;
  }

  phys::engine *
  try_engine ()
  {
    return const_cast<phys::engine *> (engine.get ());
  }

  const phys::engine *
  try_engine () const
  {
    return engine.get ();
  }

  void
  on_inspector_changed (
      comp::singl::runtime_context * /*unused*/)
  {
    apply_runtime_settings ();
  }

  static void
  register_meta ()
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

        .data<&comp::singl::physics_manager::fixed_timestep> (
            "fixed_timestep"_hs)
        .custom<comp::meta_info> (
            comp::meta_info{ "Fixed Timestep",
                             "Simulation tick length used by the physics "
                             "world.",
                             "" })

        .data<&comp::singl::physics_manager::max_frame_time> (
            "max_frame_time"_hs)
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
        .custom<comp::meta_info> (comp::meta_info{
            "Show Debug", "Show physics debug renderer.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    sanitize_settings ();
    archive (cereal::make_nvp ("gravity", gravity),
             cereal::make_nvp ("fixed_timestep", fixed_timestep),
             cereal::make_nvp ("max_frame_time", max_frame_time),
             cereal::make_nvp ("max_substeps", max_substeps),
             cereal::make_nvp ("show_debug", show_debug));
  }
};

} // namespace comp::singl

} // namespace wsl
