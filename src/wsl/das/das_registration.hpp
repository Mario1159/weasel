#pragma once

#include "../reg/runtime_project_module_api.hpp"
#include <string>

namespace wsl::das
{

class das_engine;

/**
 * Registers a daslang file for later execution.
 *
 * This function is called during the file discovery phase to register
 * daslang files that will be executed during finalize_load().
 *
 * :param script_path: Path to the .das script file.
 * :param engine: Reference to the daslang engine.
 * :param ctx: Registration context for components, singletons, and systems.
 */
void register_das_file (const std::string &script_path, das_engine &engine,
                        reg::runtime::runtime_module_registration_context &ctx);

/** Clears all pending daslang registrations. */
void clear_das_registrations ();

} // namespace wsl::das
