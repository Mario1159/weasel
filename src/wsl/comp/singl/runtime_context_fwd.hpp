#pragma once

/**
 * Forward-declaration header for comp::singl::runtime_context.
 *
 * Include this instead of runtime_context.hpp when you only need a
 * pointer or reference to runtime_context (e.g. storing it as a
 * member variable).  This avoids pulling in ~6,700 lines of
 * transitive headers (resource_manager, event_hub, component_registry,
 * render_window, etc.) into every translation unit that touches the
 * runtime context.
 *
 * Include the full runtime_context.hpp in your .cpp file when you need
 * to call methods on the context or access its subsystems.
 */

namespace wsl
{

namespace rsc
{
class world;
class scene_manager;
class resource_manager;
struct resource_manager_view;
class scene;
struct project;
} // namespace rsc

namespace reg
{
class component_registry;
class singleton_registry;
class system_factory_registry;
class registry_queries;
namespace runtime
{
class runtime_project_module;
} // namespace runtime
} // namespace reg

namespace event
{
struct event_debug_db;
struct event_hub;
class message_bus;
} // namespace event

namespace gfx
{
class render_context;
class render_window;
class scene_renderer;
} // namespace gfx

namespace phys
{
class engine;
} // namespace phys

namespace sys
{
class core_systems;
} // namespace sys

namespace input
{
class action_map;
} // namespace input

namespace comp::singl
{
class ui_manager;
struct rendering_manager;
struct physics_manager;
class editor_context;

class runtime_context;

} // namespace comp::singl

} // namespace wsl
