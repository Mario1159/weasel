#pragma once

#include <entt/entt.hpp>
#include <string>

namespace wsl::das
{

/*!
 * \brief Registers Weasel ECS types with daslang.
 *
 * This function should be called after the daslang engine is initialized
 * to expose ECS types and functions to daslang scripts.
 */
void register_ecs_module ();

/*!
 * \brief Returns the active registry for daslang operations.
 *
 * The registry must be set before calling daslang functions that
 * interact with the ECS.
 */
entt::registry *get_active_registry ();

/*!
 * \brief Sets the active registry for daslang operations.
 *
 * \param registry Pointer to the active registry (can be nullptr to clear).
 */
void set_active_registry (entt::registry *registry);

} // namespace wsl::das
