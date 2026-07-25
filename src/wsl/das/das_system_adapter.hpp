#pragma once

#include "../sys/system.hpp"
#include "modules/weasel_ecs_adapter_gen.inc"
#include <string>

namespace das
{
struct StructInfo;
class Context;
}

namespace wsl::das
{

class das_engine;

/*!
 * \brief Dual-inheritance bridge: ecs_system (C++) + EcsSystemAdapter
 * (daslang).
 *
 * When C++ calls on_init/on_update/on_event/on_inactive, the adapter
 * checks whether the daslang class overrides the method and invokes it
 * through das_invoke_function. This is the class adapter pattern from
 * daScript tutorial 19.
 */
class das_system_adapter : public sys::ecs_system, public EcsSystemAdapter
{
public:
  /*!
   * \brief Constructs a daslang-backed system.
   * \param name Display name for the system.
   * \param script_path Path to the .das script file.
   * \param engine Reference to the daslang engine.
   * \param type_id Per-instance type identifier (from registration).
   * \param class_ptr Pointer to the daslang class instance (VM heap).
   * \param class_info StructInfo of the daslang class (for adapter offsets).
   * \param ctx daslang execution context.
   */
  das_system_adapter (const std::string &name, const std::string &script_path,
                      das_engine &engine, entt::id_type type_id,
                      void *class_ptr, const StructInfo *class_info,
                      Context *ctx);

  ~das_system_adapter () override = default;

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
  void *m_class_ptr;
  Context *m_ctx;
};

} // namespace wsl::das
