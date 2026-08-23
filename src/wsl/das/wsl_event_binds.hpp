#pragma once

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_handle.h"

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

} // namespace wsl::das
