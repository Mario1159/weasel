#include "das_ecs_binds.hpp"

namespace wsl::das
{

namespace
{

entt::registry *g_active_registry = nullptr;

} // anonymous namespace

void
register_ecs_module ()
{
  // TODO: Register daslang module with ECS bindings
  // This will expose entity creation, component access, etc.
}

entt::registry *
get_active_registry ()
{
  return g_active_registry;
}

void
set_active_registry (entt::registry *registry)
{
  g_active_registry = registry;
}

} // namespace wsl::das
