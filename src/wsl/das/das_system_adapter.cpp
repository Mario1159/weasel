#include "das_system_adapter.hpp"
#include "das_engine.hpp"
#include "wsl_api_module.hpp"
#include "../log/log.hpp"

#include "daScript/daScript.h"

namespace wsl::das
{

das_system_adapter::das_system_adapter (const std::string &name,
                                        const std::string &script_path,
                                        das_engine &engine,
                                        entt::id_type type_id, void *class_ptr,
                                        const StructInfo *class_info,
                                        Context *ctx)
    : sys::ecs_system (name), EcsSystemAdapter (class_info),
      m_script_path (script_path), m_engine (engine), m_type_id (type_id),
      m_class_ptr (class_ptr), m_ctx (ctx)
{
}

entt::id_type
das_system_adapter::get_type_id () const
{
  return m_type_id;
}

const char *
das_system_adapter::get_type_name () const
{
  return "das_system";
}

void
das_system_adapter::on_init (entt::registry &registry)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  wsl_api_set_active_registry (&registry);
  if (auto fn = get_on_init (m_class_ptr)) {
    invoke_on_init (m_ctx, fn, m_class_ptr);
  }
}

void
das_system_adapter::on_update (entt::registry &registry, double dt)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  wsl_api_set_active_registry (&registry);
  if (auto fn = get_on_update (m_class_ptr)) {
    invoke_on_update (m_ctx, fn, m_class_ptr, static_cast<float> (dt));
  }
}

void
das_system_adapter::on_inactive (entt::registry &registry)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  wsl_api_set_active_registry (&registry);
  if (auto fn = get_on_inactive (m_class_ptr)) {
    invoke_on_inactive (m_ctx, fn, m_class_ptr);
  }
}

void
das_system_adapter::on_event (registry_handle reg, const engine_event &ev)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  wsl_api_set_active_registry (reg.get ());
  wsl_api_set_current_event (&ev);
  if (auto fn = get_on_event (m_class_ptr)) {
    invoke_on_event (m_ctx, fn, m_class_ptr);
  }
  wsl_api_set_current_event (nullptr);
}

} // namespace wsl::das
