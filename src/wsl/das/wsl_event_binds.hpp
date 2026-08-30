#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "daScript/ast/ast.h"
#endif
#if !defined(WSL_MODULE_BUILD)
#include "daScript/ast/ast_handle.h"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <entt/core/fwd.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <cstddef>
#endif

namespace das
{
class Module;
class ModuleLibrary;
}

namespace wsl::das
{

/**
 * Registers Daslang bindings for the engine input message structs
 * (`wsl::input::*`) and a pull API backed by the active runtime context's
 * `message_bus`. Scripts consume input by reading the per-frame published
 * messages through the `count`/`at` accessors, replacing the old raw-SDL
 * `on_event` path.
 */
void register_event_message_bindings (::das::Module &module,
                                      ::das::ModuleLibrary &lib);

/**
 * Records a message type in the name-keyed message registry used by
 * `message_post` / `message_count` / `for_each_message`.
 *
 * :param name: Script-visible message name (e.g. ``"mouse_motion"``).
 * :param id: Stable message type id (`comp::stable_type_id<T>()`).
 * :param size: Payload size in bytes (`sizeof (T)`).
 */
void register_message_type (const char *name, entt::id_type id,
                            std::size_t size);

/**
 * Drops every message subscription created by the given daslang context.
 * Called when a script program context is destroyed so stored callbacks can
 * never dangle.
 *
 * :param context: The context being destroyed.
 */
void wsl_api_on_context_destroyed (::das::Context *context);

} // namespace wsl::das
