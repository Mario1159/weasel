#pragma once

#include "component_registry.hpp"
#include "singleton_registry.hpp"
#include "system_factory_registry.hpp"

#include <vector>

namespace wsl
{

namespace comp::singl
{
class runtime_context;
}

namespace reg
{
namespace runtime
{

/**
 * Returns the engine-owned runtime context for currently loaded
 *        runtime project code.
 * :return: Pointer to the active runtime context.
 */
comp::singl::runtime_context *active_runtime_context ();

/**
 * Shared registries exposed to runtime code during registration.
 *
 * Runtime components, singletons, and systems register themselves against
 * these engine-owned registries after their headers or interpreted code have
 * been loaded.
 */
struct runtime_module_registration_context
{
  component_registry &components;
  singleton_registry &singletons;
  system_factory_registry &systems;
};

/**
 * Helper class that centralizes runtime registration helpers and
 * buckets.
 *
 * This class replaces the previous free-floating runtime_detail namespace and
 * groups registration callbacks and helpers used by interpreted/compiled
 * runtime modules.
 */
class runtime_registrar
{
public:
  using registration_fn = void (*) (runtime_module_registration_context &);

  /**
 * Makes the current runtime boundary use the engine-owned meta
 * context.
 * :param meta_ctx_ptr: Pointer to the engine's default \c entt::meta_ctx.
 *
 * EnTT stores reflection data in a per-boundary default context. Runtime code
 * compiled or interpreted outside the engine must adopt the engine-owned
 * context before registering reflected types, otherwise the inspector
 * resolves a different registry than the one runtime components populated.
 */
  static void
  sync_runtime_state (void *meta_ctx_ptr)
  {
    if (meta_ctx_ptr != nullptr) {
      entt::locator<entt::meta_ctx>::reset (
          static_cast<entt::meta_ctx *> (meta_ctx_ptr), [] (auto *) {});
    }
  }

  /**
 * Sets the active runtime context for the current runtime boundary.
 * :param runtime_ctx: Pointer to the runtime context to activate.
 */
  static void
  set_active_runtime_context (comp::singl::runtime_context *runtime_ctx);

  /**
 * Returns the component registration bucket for runtime code.
 * :return: The shared component registration callback list.
 */
  static std::vector<registration_fn> &component_registrations ();

  /**
 * Returns the singleton registration bucket for runtime code.
 * :return: The shared singleton registration callback list.
 */
  static std::vector<registration_fn> &singleton_registrations ();

  /**
 * Returns the system registration bucket for runtime code.
 * :return: The shared system registration callback list.
 */
  static std::vector<registration_fn> &system_registrations ();

  /** Adds a registration callback to one of the runtime buckets. */
  struct registration_hook
  {
    /**
 * Registers a callback function into a specific bucket.
 * :param bucket: Function that returns the target registration bucket.
 * :param fn: The registration callback to add.
 */
    registration_hook (std::vector<registration_fn> &(*bucket) (),
                       registration_fn fn)
    {
      bucket ().push_back (fn);
    }
  };

  /**
 * Helper to register a component type in the registration context.
 * :param T: The component type to register.
 * :param ctx: The registration context.
 * :param display_name: User-facing name for the component.
 */
  template <typename T>
  static void
  register_component (runtime_module_registration_context &ctx,
                      const char *display_name)
  {
    ::wsl::reg::world_component_registration_options options{};
    if (display_name != nullptr) {
      options.display_name = display_name;
    }
    options.runtime_registered = true;
    ctx.components.register_world_component<T> (options);
  }

  /**
 * Helper to register a singleton type in the registration context.
 * :param T: The singleton type to register.
 * :param ctx: The registration context.
 * :param display_name: User-facing name for the singleton.
 */
  template <typename T>
  static void
  register_singleton (runtime_module_registration_context &ctx,
                      const char *display_name)
  {
    ::wsl::reg::singleton_component_registration_options options{};
    if (display_name != nullptr) {
      options.display_name = display_name;
    }
    options.runtime_registered = true;
    ctx.singletons.register_singleton_component<T> (options);
  }

  /**
 * Helper to register a system type in the registration context.
 * :param T: The system type to register.
 * :param ctx: The registration context.
 * :param display_name: User-facing name for the system.
 */
  template <typename T>
  static void
  register_system (runtime_module_registration_context &ctx,
                   const char *display_name)
  {
    ::wsl::reg::system_registration_options options{};
    if (display_name != nullptr) {
      options.display_name = display_name;
    }
    options.runtime_registered = true;
    ctx.systems.register_system_type<T> (options);
  }
};

// Backwards-compatible alias for a gradual migration from the old namespace
using runtime_detail = runtime_registrar;

} // namespace runtime
} // namespace reg

} // namespace wsl

#define WEASEL_DETAIL_CAT_IMPL(a, b) a##b
#define WEASEL_DETAIL_CAT(a, b) WEASEL_DETAIL_CAT_IMPL (a, b)

#define WEASEL_RUNTIME_COMPONENT(Type, ...)                                    \
  WEASEL_RUNTIME_COMPONENT_NAMED (Type, "" __VA_ARGS__, __COUNTER__)

#define WEASEL_RUNTIME_COMPONENT_NAMED(Type, DisplayNameLiteral, N)            \
  namespace wsl::comp                                                          \
  {                                                                            \
  template <> struct type_traits<Type>                                         \
  {                                                                            \
    static constexpr std::string_view                                          \
    name ()                                                                    \
    {                                                                          \
      return #Type;                                                            \
    }                                                                          \
  };                                                                           \
  }                                                                            \
  static void WEASEL_DETAIL_CAT (weasel_runtime_component_register_, N) (      \
      wsl::reg::runtime::runtime_module_registration_context & ctx)            \
  {                                                                            \
    wsl::reg::runtime::runtime_detail::register_component<Type> (              \
        ctx, DisplayNameLiteral);                                              \
  }                                                                            \
  static wsl::reg::runtime::runtime_detail::registration_hook                  \
      WEASEL_DETAIL_CAT (weasel_runtime_component_hook_, N) (                  \
          wsl::reg::runtime::runtime_detail::component_registrations,          \
          &WEASEL_DETAIL_CAT (weasel_runtime_component_register_, N));

#define WEASEL_RUNTIME_SYSTEM(Type, ...)                                       \
  WEASEL_RUNTIME_SYSTEM_NAMED (Type, "" __VA_ARGS__, __COUNTER__)

#define WEASEL_RUNTIME_SYSTEM_NAMED(Type, DisplayNameLiteral, N)               \
  namespace wsl::comp                                                          \
  {                                                                            \
  template <> struct type_traits<Type>                                         \
  {                                                                            \
    static constexpr std::string_view                                          \
    name ()                                                                    \
    {                                                                          \
      return #Type;                                                            \
    }                                                                          \
  };                                                                           \
  }                                                                            \
  static void WEASEL_DETAIL_CAT (weasel_runtime_system_register_, N) (         \
      wsl::reg::runtime::runtime_module_registration_context & ctx)            \
  {                                                                            \
    wsl::reg::runtime::runtime_detail::register_system<Type> (                 \
        ctx, DisplayNameLiteral);                                              \
  }                                                                            \
  static wsl::reg::runtime::runtime_detail::registration_hook                  \
      WEASEL_DETAIL_CAT (weasel_runtime_system_hook_, N) (                     \
          wsl::reg::runtime::runtime_detail::system_registrations,             \
          &WEASEL_DETAIL_CAT (weasel_runtime_system_register_, N));

#define WEASEL_RUNTIME_SINGLETON(Type, ...)                                    \
  WEASEL_RUNTIME_SINGLETON_NAMED (Type, "" __VA_ARGS__, __COUNTER__)

#define WEASEL_RUNTIME_SINGLETON_NAMED(Type, DisplayNameLiteral, N)            \
  namespace wsl::comp                                                          \
  {                                                                            \
  template <> struct type_traits<Type>                                         \
  {                                                                            \
    static constexpr std::string_view                                          \
    name ()                                                                    \
    {                                                                          \
      return #Type;                                                            \
    }                                                                          \
  };                                                                           \
  }                                                                            \
  static void WEASEL_DETAIL_CAT (weasel_runtime_singleton_register_, N) (      \
      wsl::reg::runtime::runtime_module_registration_context & ctx)            \
  {                                                                            \
    wsl::reg::runtime::runtime_detail::register_singleton<Type> (              \
        ctx, DisplayNameLiteral);                                              \
  }                                                                            \
  static wsl::reg::runtime::runtime_detail::registration_hook                  \
      WEASEL_DETAIL_CAT (weasel_runtime_singleton_hook_, N) (                  \
          wsl::reg::runtime::runtime_detail::singleton_registrations,          \
          &WEASEL_DETAIL_CAT (weasel_runtime_singleton_register_, N));
