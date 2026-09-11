#pragma once

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif

namespace das
{
class Module;
class ModuleGroup;
}

namespace wsl::das
{

/**
 * Registers Weasel ECS types with daslang.
 *
 * This module registers engine component types (transform, camera, etc.)
 * as daScript types so they can be used in daslang scripts.
 */
void register_ecs_module (::das::ModuleGroup &module_group);

::das::Module *get_ecs_module ();

::das::Module *create_worker_ecs_module ();

} // namespace wsl::das
