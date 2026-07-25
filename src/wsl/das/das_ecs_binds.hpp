#pragma once

#include <entt/entt.hpp>
#include <string>

namespace das
{
class Module;
class ModuleGroup;
}

namespace wsl::das
{

/*!
 * \brief Registers Weasel ECS types with daslang.
 *
 * This module registers engine component types (transform, camera, etc.)
 * as daScript types so they can be used in daslang scripts.
 */
void register_ecs_module (::das::ModuleGroup &module_group);

::das::Module *get_ecs_module ();

::das::Module *create_worker_ecs_module ();

} // namespace wsl::das
