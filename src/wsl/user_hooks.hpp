#pragma once

/*!
 * \file user_hooks.hpp
 * \brief C callback hooks for user project code.
 *
 * User projects can optionally implement these functions in their C++ code
 * to receive lifecycle notifications from the engine. The functions use
 * C linkage so they can be resolved via dlsym from the compiled shared
 * library.
 *
 * In standalone mode (user's main()), the user's app class calls these
 * directly. In editor mode, the editor resolves them from the project's
 * compiled shared library.
 *
 * Weak default implementations are provided so linking never fails.
 */

namespace wsl
{
class app;
}

extern "C" {

/*!
 * \brief Called once when the project is loaded.
 *
 * Use this for one-time initialization: registering C++ components,
 * setting up custom resources, etc.
 */
__attribute__ ((weak)) void wsl_on_project_init (wsl::app &engine);

/*!
 * \brief Called every frame during gameplay.
 *
 * Use this for per-frame C++ logic that runs alongside das systems.
 */
__attribute__ ((weak)) void wsl_on_project_update (wsl::app &engine, double dt);

/*!
 * \brief Called once when the project is unloaded or the editor exits.
 *
 * Use this for cleanup: releasing custom resources, etc.
 */
__attribute__ ((weak)) void wsl_on_project_shutdown (wsl::app &engine);

} // extern "C"
