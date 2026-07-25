#pragma once

#include "../sys/system.hpp"
#include <string>

namespace wsl::das
{

class das_engine;

/*!
 * \brief A system implementation backed by a daslang script.
 *
 * Compiles a .das file and delegates lifecycle callbacks to module-level
 * wrapper functions (on_init, on_update, on_event, on_inactive) which
 * in turn call class method overrides.
 */
class das_system : public sys::ecs_system
{
public:
  das_system (const std::string &name, const std::string &script_path,
              das_engine &engine, entt::id_type type_id);

  ~das_system () override = default;

  entt::id_type get_type_id () const override;
  const char *get_type_name () const override;

  void on_init (entt::registry &registry) override;
  void on_update (entt::registry &registry, double dt) override;
  void on_inactive (entt::registry &registry) override;
  void on_event (registry_handle registry, const engine_event &ev) override;

  const std::string &
  script_path () const
  {
    return m_script_path;
  }

private:
  std::string m_script_path;
  das_engine &m_engine;
  entt::id_type m_type_id;
};

} // namespace wsl::das
