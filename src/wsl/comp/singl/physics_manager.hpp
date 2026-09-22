#pragma once

#include "../../phys/physics_engine.hpp"
#include "../component_meta.hpp"

#ifndef IN_MODULE_INTERFACE
#include <algorithm>
#endif
#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <memory>
#endif

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
  on_inspector_changed (comp::singl::runtime_context * /*unused*/)
  {
    apply_runtime_settings ();
  }

  static void register_meta ();
};

} // namespace comp::singl

} // namespace wsl
