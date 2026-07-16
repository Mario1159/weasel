#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/entt.hpp>

namespace wsl
{

/*!
 * \brief Lightweight, non-owning handle to an entt::registry.
 *
 * Replaces raw `entt::registry&` / `entt::registry*` in public APIs so that
 * daslang (and other scripting) bindings receive a single, stable type
 * instead of leaking the registry implementation detail.
 *
 * The handle does NOT own the registry and must not outlive it.  All
 * const-accessors return pointers/references so callers can short-circuit
 * when the handle is empty.
 */
class registry_handle
{
public:
  registry_handle () = default;
  explicit registry_handle (entt::registry &r) : m_registry (&r) {}
  explicit registry_handle (entt::registry *r) : m_registry (r) {}

  entt::registry *
  get () const
  {
    return m_registry;
  }

  entt::registry &
  operator* () const
  {
    return *m_registry;
  }

  entt::registry *
  operator->() const
  {
    return m_registry;
  }

  bool
  valid () const
  {
    return m_registry != nullptr;
  }

  explicit
  operator bool () const
  {
    return m_registry != nullptr;
  }

private:
  entt::registry *m_registry = nullptr;
};

} // namespace wsl
