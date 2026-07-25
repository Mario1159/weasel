#include "das_system.hpp"
#include "das_engine.hpp"
#include "wsl_api_module.hpp"
#include "../log/log.hpp"

namespace wsl::das
{

das_system::das_system (const std::string &name, const std::string &script_path,
                        das_engine &engine, entt::id_type type_id)
    : sys::ecs_system (name), m_script_path (script_path), m_engine (engine),
      m_type_id (type_id)
{
  set_editor_active (false);
}

entt::id_type
das_system::get_type_id () const
{
  return m_type_id;
}

const char *
das_system::get_type_name () const
{
  return "das_system";
}

void
das_system::on_init (entt::registry &registry)
{
  wsl_api_set_active_registry (&registry);
  if (!m_engine.call_void_function (m_script_path, "on_init")) {
    wsl::log::cmake ()->error ("das_system::on_init failed: {}",
                               m_engine.last_error ());
  }
}

void
das_system::on_update (entt::registry &registry, double dt)
{
  wsl_api_set_active_registry (&registry);
  wsl_api_set_delta_time (dt);
  if (!m_engine.call_void_function (m_script_path, "on_update")) {
    wsl::log::cmake ()->error ("das_system::on_update failed: {}",
                               m_engine.last_error ());
  }
}

void
das_system::on_inactive (entt::registry &registry)
{
  wsl_api_set_active_registry (&registry);
  if (!m_engine.call_void_function (m_script_path, "on_inactive")) {
    wsl::log::cmake ()->error ("das_system::on_inactive failed: {}",
                               m_engine.last_error ());
  }
}

void
das_system::on_event (registry_handle reg, const engine_event &ev)
{
  wsl_api_set_active_registry (reg.get ());
  wsl_api_set_current_event (&ev);
  if (!m_engine.call_void_function (m_script_path, "on_event")) {
    wsl::log::cmake ()->error ("das_system::on_event failed: {}",
                               m_engine.last_error ());
  }
  wsl_api_set_current_event (nullptr);
}

} // namespace wsl::das
