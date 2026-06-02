#pragma once

#include "../reg/sig/signal_hub.hpp"
#include "../comp/component_meta.hpp"
#include "wsl/log/log.hpp"

#include "entt/entity/fwd.hpp"
#include <SDL3/SDL_events.h>
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace wsl
{

/**
 * @namespace wsl::sys
 * @brief Logic and behavior implemented through ECS systems.
 */
namespace sys
{

class ecs_system
{
public:
  using iteration_fn_t = std::function<void (entt::registry &, double)>;

  struct registered_iteration
  {
    std::string name;
    iteration_fn_t fn;
  };

  explicit ecs_system (const std::string &name) : m_name (name) {}
  virtual ~ecs_system () = default;

  virtual entt::id_type get_type_id () const = 0;
  virtual const char *get_type_name () const = 0;

  virtual void
  register_signals (reg::sig::signal_hub &)
  {
  }
  virtual void
  register_event_handlers (reg::sig::signal_hub &)
  {
  }
  virtual void
  register_iterations (reg::sig::signal_hub &)
  {
  }

  virtual void
  on_init (entt::registry &)
  {
  }
  virtual void
  on_inactive (entt::registry &)
  {
  }
  virtual void
  on_update (entt::registry &, double /*dt*/)
  {
  }
  virtual void
  on_editor_update (entt::registry &, double /*dt*/)
  {
  }
  virtual void
  on_event (entt::registry &, const SDL_Event &)
  {
  }

  virtual void
  on_render_build_draw_data (entt::registry &)
  {
  }
  virtual void
  on_render_prepare_gpu_rsc (entt::registry &)
  {
  }
  virtual void
  on_render_record_draw_cmd (entt::registry &)
  {
  }

  void
  init (entt::registry *registry)
  {
    if (m_initialized) {
      return;
    }

    wsl::log::sys ()->trace ("Initializing {}", get_name ());
    on_init (*registry);
    m_initialized = true;
  }

  void
  shutdown (entt::registry *registry)
  {
    if (m_active && (registry != nullptr)) {
      wsl::log::sys ()->trace ("Shutting down {}", get_name ());
      on_inactive (*registry);
    }

    m_active = false;
    m_initialized = false;
  }

  void
  refresh_activation (entt::registry *registry, bool is_playing)
  {
    if (registry == nullptr) {
      m_active = false;
      return;
    }

    if (is_playing) {
      if (m_init_on_startup) {
        if (!m_active) {
          wsl::log::sys ()->trace ("Activating {}", get_name ());
          init (registry);
          m_active = true;
        }
      } else {
        if (m_active) {
          wsl::log::sys ()->trace ("Deactivating {}", get_name ());
          on_inactive (*registry);
          m_active = false;
        }
      }
    } else {
      if (m_active) {
        wsl::log::sys ()->trace ("Deactivating {}", get_name ());
        on_inactive (*registry);
        m_active = false;
      }
    }
  }

  void
  set_init_on_startup (bool value, entt::registry *registry, bool is_playing)
  {
    m_init_on_startup = value;
    refresh_activation (registry, is_playing);
  }

  void
  set_editor_active (bool value)
  {
    m_editor_active = value;
  }

  void
  set_active (bool value, entt::registry *registry)
  {
    if (m_active == value) {
      return;
    }

    if (value) {
      if (registry != nullptr) {
        init (registry);
        m_active = true;
      }
    } else {
      if (registry != nullptr) {
        on_inactive (*registry);
      }
      m_active = false;
    }
  }

  void
  update (entt::registry *registry, double dt)
  {
    if (m_active && (registry != nullptr)) {
      on_update (*registry, dt);
    }
  }

  void
  editor_update (entt::registry *registry, double dt)
  {
    if (m_editor_active && (registry != nullptr)) {
      on_editor_update (*registry, dt);
    }
  }

  void
  event_handler (entt::registry *registry, const SDL_Event &event)
  {
    if (m_active && (registry != nullptr)) {
      on_event (*registry, event);
    }
  }

  void
  render_build_draw_data (entt::registry *registry)
  {
    if ((m_active || m_editor_active) && (registry != nullptr)) {
      on_render_build_draw_data (*registry);
    }
  }

  void
  render_prepare_gpu_rsc (entt::registry *registry)
  {
    if ((m_active || m_editor_active) && (registry != nullptr)) {
      on_render_prepare_gpu_rsc (*registry);
    }
  }

  void
  render_record_draw_cmd (entt::registry *registry)
  {
    if ((m_active || m_editor_active) && (registry != nullptr)) {
      on_render_record_draw_cmd (*registry);
    }
  }

  const std::string &
  get_name () const
  {
    return m_name;
  }

  const std::vector<std::string> &
  get_dependencies () const
  {
    return m_dependencies;
  }

  const std::vector<std::string> &
  get_conflicts () const
  {
    return m_conflicts;
  }

  bool
  is_active () const
  {
    return m_active;
  }

  bool
  is_init_on_startup () const
  {
    return m_init_on_startup;
  }

  bool
  is_editor_active () const
  {
    return m_editor_active;
  }

protected:
  void
  set_dependencies (std::vector<std::string> dependencies)
  {
    m_dependencies = std::move (dependencies);
  }

  void
  set_conflicts (std::vector<std::string> conflicts)
  {
    m_conflicts = std::move (conflicts);
  }

  void
  set_relationships (std::vector<std::string> dependencies,
                     std::vector<std::string> conflicts = {})
  {
    m_dependencies = std::move (dependencies);
    m_conflicts = std::move (conflicts);
  }

  void
  clear_registered_iterations ()
  {
    m_iterations.clear ();
  }

  void
  run_registered_iterations (entt::registry &registry, double dt)
  {
    for (auto &it : m_iterations) {
      if (it.fn) {
        it.fn (registry, dt);
      }
    }
  }

  void
  run_registered_iteration (entt::registry &registry, double dt,
                            const char *iteration_name)
  {
    if (iteration_name == nullptr) {
      return;
    }

    for (auto &it : m_iterations) {
      if (it.name == iteration_name) {
        if (it.fn) {
          it.fn (registry, dt);
        }
        return;
      }
    }
  }

  std::vector<registered_iteration> m_iterations;

private:
  std::string m_name;
  std::vector<std::string> m_dependencies;
  std::vector<std::string> m_conflicts;
  bool m_active = false;
  bool m_init_on_startup = true;
  bool m_editor_active = true;
  bool m_initialized = false;
};

template <typename Derived> class ecs_system_t : public ecs_system
{
  ecs_system_t () = default;

public:
  using ecs_system::ecs_system;

  entt::id_type
  get_type_id () const override
  {
    return wsl::comp::stable_type_id<Derived> ();
  }

  const char *
  get_type_name () const override
  {
    static const std::string type_name
        = std::string (entt::type_name<Derived> ().value ());
    return type_name.c_str ();
  }

protected:
  template <typename... Components, typename Fn>
  void
  register_iteration (reg::sig::signal_hub &hub, const char *iteration_name,
                      Fn &&fn)
  {
    hub.template declare_iteration<Derived, Components...> (iteration_name);

    this->m_iterations.push_back (ecs_system::registered_iteration{
        iteration_name ? iteration_name : "",
        ecs_system::iteration_fn_t (std::forward<Fn> (fn)),
    });
  }

  friend Derived;
};

} // namespace sys

} // namespace wsl
