module;

#define WSL_MODULE_BUILD

import "thirdparty/thirdparty_all.hpp";
import "phys/jolt_all.hpp";
import "das/daScript_all.hpp";

export module wsl.sys;

export {
#include "audio_system.hpp"
#include "core_systems.hpp"
#include "lighting_system.hpp"
#include "physics_system.hpp"
#include "render_2d_system.hpp"
#include "render_3d_system.hpp"
#include "render_frame.hpp"
#include "render_ui_system.hpp"
#include "shadow_system.hpp"
#include "skybox_system.hpp"
#include "stage_registry.hpp"
#include "system_dependency_graph.hpp"
#include "system.hpp"
#include "system_scheduler.hpp"
#include "task_pool.hpp"
#include "transform_system.hpp"
}
