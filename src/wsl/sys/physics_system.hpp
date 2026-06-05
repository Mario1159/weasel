#pragma once

#include "system.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace wsl
{

namespace comp
{
struct area;
namespace singl
{
struct physics_manager;
}
}

namespace sys
{

class physics_system : public sys::ecs_system_t<physics_system>
{
public:
  explicit physics_system (const std::string &name);
  ~physics_system () override;

  void register_signals (reg::sig::signal_hub &hub) override;
  void register_event_handlers (reg::sig::signal_hub &hub) override;
  void register_iterations (reg::sig::signal_hub &hub) override;

  void on_init (entt::registry &registry) override;
  void on_inactive (entt::registry &registry) override;
  void on_update (entt::registry &registry, double dt) override;
  void on_editor_update (entt::registry &registry, double dt) override;

  void on_render_build_draw_data (entt::registry &registry) override;
  void on_render_record_draw_cmd (entt::registry &registry) override;

private:
  static void set_local_from_world (entt::registry &registry,
                                    entt::entity entity,
                                    const glm::vec3 &world_pos,
                                    const glm::quat &world_rot);
  static comp::singl::physics_manager *
  get_registry_physics_manager (entt::registry &registry);
  void update_character_controllers (entt::registry &registry, double dt);
  void sync_transforms_to_rigid_bodies (entt::registry &registry, double dt,
                                        bool force_all = false);
  void sync_transforms_to_areas (entt::registry &registry, double dt);
  void step_world (entt::registry &registry, double dt);
  void dispatch_sensor_overlap_events (entt::registry &registry, double dt);
  void sync_rigid_bodies_to_transforms (entt::registry &registry, double dt);
  void sync_characters_to_transforms (entt::registry &registry, double dt);

  void recreate_all_bodies (entt::registry &registry);

  void on_rigid_body_constructed (entt::registry &registry,
                                  entt::entity entity);
  void on_rigid_body_removed (entt::registry &registry, entt::entity entity);
  void on_area_constructed (entt::registry &registry, entt::entity entity);
  void on_area_removed (entt::registry &registry, entt::entity entity);
  void on_character_body_removed (entt::registry &registry,
                                  entt::entity entity);

  entt::registry *m_registry = nullptr;
};

} // namespace sys

} // namespace wsl
