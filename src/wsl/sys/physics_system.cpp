#include "physics_system.hpp"

#include "../comp/area3d.hpp"
#include "../comp/character_body.hpp"
#include "../comp/hierarchy.hpp"
#include "../comp/rigid_body.hpp"
#include "reg/sig/signal_hub.hpp"
#include "../comp/transform.hpp"
#include "../comp/world_transform.hpp"

#include "../comp/singl/physics_manager.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../phys/character_query_filters.hpp"
#include "../phys/physics_engine.hpp"
#include "../phys/utils.hpp"
#include "sys/system.hpp"
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/EActivation.h>
#include <cstddef>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/matrix.hpp>
#include <string>

#include "../../editor/physics_debug_drawer.hpp"
#include "../comp/singl/editor_context.hpp"
#include "../debug/debug_renderer.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/gtc/quaternion.hpp>

#include <unordered_map>


namespace wsl
{


namespace sys
{

physics_system::physics_system (const std::string &name) : ecs_system_t (name)
{
  set_relationships ({ "Transform System" });
}

physics_system::~physics_system () {}

void
physics_system::set_local_from_world (entt::registry &reg, entt::entity e,
                                      const glm::vec3 &world_pos,
                                      const glm::quat &world_rot) 
{
  comp::transform &t = reg.get<comp::transform> (e);

  if (auto *h = reg.try_get<comp::hierarchy> (e);
      (h != nullptr) && h->parent != entt::null
      && reg.all_of<comp::world_transform> (h->parent)) {

    const glm::mat4 &parent_wt
        = reg.get<comp::world_transform> (h->parent).value;

    glm::mat4 const inv_parent = glm::inverse (parent_wt);

    glm::vec4 const lp = inv_parent * glm::vec4 (world_pos, 1.0F);
    t.position = math::vec3f{ lp.x, lp.y, lp.z };

    glm::quat const parent_rot = glm::quat_cast (parent_wt);
    t.rotation = math::quatf (glm::inverse (parent_rot) * world_rot);
  } else {
    t.position = math::vec3f{ world_pos.x, world_pos.y, world_pos.z };
    t.rotation = math::quatf{ world_rot.x, world_rot.y, world_rot.z, world_rot.w };
  }

  // CRITICAL: Update world_transform immediately so other systems (like rendering)
  // see the new position in this same frame.
  if (auto *wt = reg.try_get<comp::world_transform> (e)) {
    wt->value = glm::translate (glm::mat4 (1.0F), world_pos)
                * glm::mat4_cast (world_rot)
                * glm::scale (glm::mat4 (1.0F), (glm::vec3)t.scale);
  }
}

comp::singl::physics_manager *
physics_system::get_registry_physics_manager (entt::registry &registry) 
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::physics_manager> ()) {
    return nullptr;
  }

  return &ctx.get<comp::singl::physics_manager> ();
}

void
physics_system::register_signals (reg::sig::signal_hub &hub)
{
  // these signal types may still live in component namespaces,
  // but ownership is now explicitly the physics system.
  hub.declare_signal<comp::area::entered, physics_system, comp::area> (
      +[](const void *sig) -> entt::entity {
        return static_cast<const comp::area::entered *>(sig)->area_entity;
      });
  hub.declare_signal<comp::area::exited, physics_system, comp::area> (
      +[](const void *sig) -> entt::entity {
        return static_cast<const comp::area::exited *>(sig)->area_entity;
      });
  }

void
physics_system::register_event_handlers (reg::sig::signal_hub &hub)
{
  // add declarations here later if you connect dispatcher sinks for this
  // system reg::sig::declare_handler<physics_system>(hub,
  // "on_some_event");
  (void)hub;
}

void
physics_system::register_iterations (reg::sig::signal_hub &hub)
{
  clear_registered_iterations ();

  register_iteration<comp::character_body> (
      hub, "update_character_controllers",
      [this] (entt::registry &registry, double dt) {
        update_character_controllers (registry, dt);
      });

  register_iteration<comp::transform, comp::rigid_body> (
      hub, "sync_transforms_to_rigid_bodies",
      [this] (entt::registry &registry, double dt) {
        sync_transforms_to_rigid_bodies (registry, dt);
      });

  register_iteration<> (hub, "step_world",
                        [this] (entt::registry &registry, double dt) {
                          step_world (registry, dt);
                        });

  register_iteration<comp::area> (hub, "dispatch_sensor_overlap_events",
                                  [this] (entt::registry &registry, double dt) {
                                    dispatch_sensor_overlap_events (registry,
                                                                    dt);
                                  });

  register_iteration<comp::transform, comp::rigid_body> (
      hub, "sync_rigid_bodies_to_transforms",
      [this] (entt::registry &registry, double dt) {
        sync_rigid_bodies_to_transforms (registry, dt);
      });

  register_iteration<comp::transform, comp::character_body> (
      hub, "sync_characters_to_transforms",
      [this] (entt::registry &registry, double dt) {
        sync_characters_to_transforms (registry, dt);
      });
}

void
physics_system::on_init (entt::registry &registry)
{
  this->m_registry = &registry;

  recreate_all_bodies (registry);

  registry.on_destroy<comp::rigid_body> ()
      .connect<&physics_system::on_rigid_body_removed> (this);
  registry.on_destroy<comp::area> ().connect<&physics_system::on_area_removed> (
      this);
  registry.on_destroy<comp::character_body> ()
      .connect<&physics_system::on_character_body_removed> (this);
}

void
physics_system::on_inactive (entt::registry &registry)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics != nullptr) {
    phys::engine &engine = physics->ensure_engine ();

    // Manually cleanup all bodies because they might not be destroyed yet
    // or the registry is being cleared after shutdown.
    auto rb_view = registry.view<comp::rigid_body> ();
    for (entt::entity const e : rb_view) {
      registry.get<comp::rigid_body> (e).destroy_body (engine);
    }

    auto area_view = registry.view<comp::area> ();
    for (entt::entity const e : area_view) {
      registry.get<comp::area> (e).destroy_body (engine);
    }

    auto char_view = registry.view<comp::character_body> ();
    for (entt::entity const e : char_view) {
      registry.get<comp::character_body> (e).destroy_body ();
    }
  }

  registry.on_destroy<comp::rigid_body> ()
      .disconnect<&physics_system::on_rigid_body_removed> (this);
  registry.on_destroy<comp::area> ()
      .disconnect<&physics_system::on_area_removed> (this);
  registry.on_destroy<comp::character_body> ()
      .disconnect<&physics_system::on_character_body_removed> (this);

  this->m_registry = nullptr;
}

void
physics_system::on_update (entt::registry &registry, double dt)
{
  this->m_registry = &registry;

  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }

  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();
  if (!runtime.is_running) {
    return;
  }

  run_registered_iterations (registry, dt);
}

void
physics_system::on_render_build_draw_data (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();

  if (!ctx.contains<comp::singl::editor_context *> ()) {
    return;
  }
  auto &editor_ctx = *ctx.get<comp::singl::editor_context *> ();

  auto *scene = runtime.scene_manager.get_active ();
  if (scene == nullptr) {
    return;
}

  comp::singl::editor_context::resolved_camera rc;
  if (!editor_ctx.resolve_game_view_camera (registry, scene, rc)) {
    return;
}

  wsl::debug::debug_renderer_interface *debug_renderer = editor_ctx.get_debug_renderer ();
  debug_renderer->set_camera_pos (rc.world_pos);
  debug_renderer->begin_frame ();

  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if ((physics != nullptr) && physics->show_debug) {
    phys::engine &engine = physics->ensure_engine ();
    editor::draw_physics_debug (engine, *debug_renderer);
  }

  debug_renderer->upload_buffers ();
}

void
physics_system::on_render_record_draw_cmd (entt::registry &registry)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();

  if (!ctx.contains<comp::singl::editor_context *> ()) {
    return;
  }
  auto &editor_ctx = *ctx.get<comp::singl::editor_context *> ();

  auto *scene = runtime.scene_manager.get_active ();
  if (scene == nullptr) {
    return;
}

  comp::singl::editor_context::resolved_camera rc;
  if (!editor_ctx.resolve_game_view_camera (registry, scene, rc)) {
    return;
}

  editor_ctx.get_debug_renderer ()->end_frame (rc.vp);
}

void
physics_system::update_character_controllers (entt::registry &registry,
                                              double dt)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  const float step = static_cast<float> (dt);

  auto view = registry.view<comp::character_body> ();

  for (entt::entity const e : view) {
    comp::character_body &char_body = view.get<comp::character_body> (e);
    JPH::CharacterVirtual *character = char_body.get ();

    if (character == nullptr) {
      continue;
}

    JPH::Vec3 vel = char_body.desired_velocity;
    vel.SetY (vel.GetY () + engine.get_gravity ());

    character->SetLinearVelocity (vel);

    phys::character_broad_phase_filter const bp_filter;
    phys::character_object_layer_filter const obj_filter;
    phys::character_body_filter const body_filter (char_body.get_id ());
    phys::character_shape_filter const shape_filter;

    character->Update (step, JPH::Vec3 (0.0F, engine.get_gravity (), 0.0F),
                       bp_filter, obj_filter, body_filter, shape_filter,
                       engine.get_temp_alloc ());
  }
}

void
physics_system::step_world (entt::registry &registry, double dt)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  const float step = static_cast<float> (dt);
  engine.step (step);
}

void
physics_system::dispatch_sensor_overlap_events (entt::registry &registry,
                                                double /*dt*/)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  auto events = engine.drain_sensor_events ();
  if (events.empty ()) {
    return;
}

  struct bid_hash
  {
    std::size_t
    operator() (const phys::body_id &id) const noexcept
    {
      return static_cast<std::size_t> (id.GetIndexAndSequenceNumber ());
    }
  };

  std::unordered_map<phys::body_id, entt::entity, bid_hash> body_to_entity;

  {
    auto view = registry.view<comp::rigid_body> ();
    for (entt::entity const e : view) {
      comp::rigid_body  const&rb = view.get<comp::rigid_body> (e);
      if (!rb.body_id.IsInvalid ()) {
        body_to_entity[rb.body_id] = e;
      }
    }
  }

  {
    auto view = registry.view<comp::area> ();
    for (entt::entity const e : view) {
      comp::area  const&a = view.get<comp::area> (e);
      if (!a.body_id.IsInvalid ()) {
        body_to_entity[a.body_id] = e;
      }
    }
  }

  {
    auto view = registry.view<comp::character_body> ();
    for (entt::entity const e : view) {
      comp::character_body  const&character = view.get<comp::character_body> (e);
      const phys::body_id body_id = character.get_id ();
      if (!body_id.IsInvalid ()) {
        body_to_entity[body_id] = e;
      }
    }
  }

  for (auto &ev : events) {
    auto it_area = body_to_entity.find (ev.sensor);
    if (it_area == body_to_entity.end ()) {
      continue;
}

    entt::entity const area_ent = it_area->second;
    entt::entity other_ent = entt::null;

    if (auto it_other = body_to_entity.find (ev.other);
        it_other != body_to_entity.end ()) {
      other_ent = it_other->second;
    }

    if (ev.entered) {
      wsl::reg::sig::emit<comp::area::entered> (
          runtime.signal_hub,
          comp::area::entered{ area_ent, other_ent, ev.other });
    } else {
      wsl::reg::sig::emit<comp::area::exited> (
          runtime.signal_hub,
          comp::area::exited{ area_ent, other_ent, ev.other });
    }
  }
}

void
physics_system::sync_transforms_to_rigid_bodies (entt::registry &registry,
                                                 double /*dt*/)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();
  auto &bi = engine.get_body_interface ();

  auto view = registry.view<comp::transform, comp::rigid_body> ();

  for (entt::entity const e : view) {
    comp::rigid_body  const&rb = view.get<comp::rigid_body> (e);
    if (rb.body_id.IsInvalid ()) {
      continue;
}

    comp::transform  const&t = view.get<comp::transform> (e);

    // Dynamic bodies should not be reset by their transform during gameplay,
    // as physics is the source of truth. Transform -> Physics sync should only
    // happen for Kinematic/Static bodies or when explicitly moved in editor.
    if (rb.motion_type.value == phys::motion_type::Dynamic) {
      continue;
}

    glm::vec3 world_pos = t.position;
    glm::quat world_rot = t.rotation;

    // If it has a parent, we MUST use the WorldTransform because
    // ECS transform is local but Jolt wants world.
    if (auto *h = registry.try_get<comp::hierarchy> (e);
        (h != nullptr) && h->parent != entt::null) {
      if (auto *wt = registry.try_get<comp::world_transform> (e)) {
        world_pos = glm::vec3 (wt->value[3]);
        world_rot = glm::quat_cast (wt->value);
      }
    }

    bi.SetPositionAndRotation (rb.body_id, to_jolt (world_pos),
                               to_jolt (world_rot), JPH::EActivation::Activate);
  }
}

void
physics_system::recreate_all_bodies (entt::registry &registry)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  // Recreate rigid bodies
  {
    auto view = registry.view<comp::rigid_body> ();
    for (entt::entity const e : view) {
      comp::rigid_body &rb = view.get<comp::rigid_body> (e);
      if (rb.body_id.IsInvalid ()) {
        glm::vec3 scale{ 1.0F, 1.0F, 1.0F };
        if (auto *t = registry.try_get<comp::transform> (e); t) {
          scale = glm::vec3 (t->scale.x, t->scale.y, t->scale.z);
        }
        rb.create_body (engine, scale);
      }
    }
  }

  // Recreate sensor areas
  {
    auto view = registry.view<comp::area> ();
    for (entt::entity const e : view) {
      comp::area &a = view.get<comp::area> (e);
      if (a.body_id.IsInvalid ()) {
        glm::vec3 scale{ 1.0F, 1.0F, 1.0F };
        if (auto *t = registry.try_get<comp::transform> (e); t) {
          scale = glm::vec3 (t->scale.x, t->scale.y, t->scale.z);
        }
        a.create_body (engine, scale);
      }
    }
  }

  // Recreate character bodies
  {
    auto view = registry.view<comp::character_body> ();
    for (entt::entity const e : view) {
      comp::character_body &c = view.get<comp::character_body> (e);
      if (c.get () == nullptr) {
        if (auto *wt = registry.try_get<comp::world_transform> (e); wt) {
          c.recreate (engine, to_jolt (glm::vec3 (wt->value[3])));
        } else if (auto *t = registry.try_get<comp::transform> (e); t) {
          c.recreate (engine, to_jolt ((glm::vec3)t->position));
        }
      }
    }
  }
}

void
physics_system::sync_rigid_bodies_to_transforms (entt::registry &registry,
                                                 double /*dt*/)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  const JPH::BodyLockInterfaceLocking &lock_interface
      = engine.get_body_lock_interface ();

  auto view = registry.view<comp::transform, comp::rigid_body> ();

  for (entt::entity const e : view) {
    comp::rigid_body  const&rb = view.get<comp::rigid_body> (e);

    if (rb.body_id.IsInvalid ()) {
      continue;
}

    JPH::BodyLockRead const lock (lock_interface, rb.body_id);
    if (!lock.Succeeded ()) {
      continue;
}

    const JPH::Body &body = lock.GetBody ();

    set_local_from_world (registry, e, to_glm (body.GetCenterOfMassPosition ()),
                          to_glm (body.GetRotation ()));
  }
}

void
physics_system::sync_characters_to_transforms (entt::registry &registry,
                                               double /*dt*/)
{
  auto view = registry.view<comp::transform, comp::character_body> ();

  for (entt::entity const e : view) {
    comp::character_body &c = view.get<comp::character_body> (e);
    if (c.get () == nullptr) {
      continue;
}

    set_local_from_world (registry, e, to_glm (c.get ()->GetPosition ()),
                          to_glm (c.get ()->GetRotation ()));
  }
}

void
physics_system::on_rigid_body_removed (entt::registry &registry,
                                       entt::entity entity)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  if (auto *rb = registry.try_get<comp::rigid_body> (entity)) {
    rb->destroy_body (engine);
  }
}

void
physics_system::on_area_removed (entt::registry &registry, entt::entity entity)
{
  comp::singl::physics_manager *physics = get_registry_physics_manager (registry);
  if (physics == nullptr) {
    return;
  }
  phys::engine &engine = physics->ensure_engine ();

  if (auto *a = registry.try_get<comp::area> (entity)) {
    a->destroy_body (engine);
  }
}

void
physics_system::on_character_body_removed (entt::registry &registry,
                                           entt::entity entity)
{
  if (auto *cb = registry.try_get<comp::character_body> (entity)) {
    cb->destroy_body ();
  }
}

} // namespace sys

} // namespace wsl
