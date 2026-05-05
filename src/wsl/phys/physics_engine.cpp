#include "physics_engine.hpp"
#include "jolt_runtime.hpp"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Collision/NarrowPhaseQuery.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "phys/broad_phase_layer_interface.hpp"
#include "phys/object_layer_pair_filter.hpp"
#include "phys/object_vs_broad_phase_layer_filter.hpp"
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/BodyManager.h>

#include <Jolt/Physics/PhysicsSettings.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <vector>


namespace wsl
{

phys::engine::engine ()
{
  phys::retain_jolt_runtime ();

  m_temp_alloc = std::make_unique<JPH::TempAllocatorImpl> (10 * 1024 * 1024); // 10MB temp
  m_job_sys = std::make_unique<JPH::JobSystemThreadPool> (
      JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
      std::max (1U, std::thread::hardware_concurrency () - 1));

  m_bp_layer_if = std::make_unique<broad_phase_layer_interface> ();
  m_obj_vs_bp_layer_filter = std::make_unique<object_vs_broad_phase_layer_filter> ();
  m_obj_layer_filter = std::make_unique<object_layer_pair_filter> ();

  const uint max_bodies = 1024;
  const uint num_body_mutexes = 0;
  const uint max_body_pairs = 1024;
  const uint max_contact_constraints = 1024;

  m_phys_sys.Init (max_bodies, num_body_mutexes, max_body_pairs,
                 max_contact_constraints, *m_bp_layer_if, *m_obj_vs_bp_layer_filter,
                 *m_obj_layer_filter);

  m_contact_listener = std::make_unique<contact_listener_impl> (*this);
  m_phys_sys.SetContactListener (m_contact_listener.get());
  set_gravity (m_gravity_y);
}

phys::engine::~engine ()
{
  m_phys_sys.SetContactListener (nullptr);
  // unique_ptr fields will be destroyed automatically
  phys::release_jolt_runtime ();
}

void
phys::engine::step (double dt)
{
  // Prevent spiral of death on huge frame drops
  dt = std::min (dt, m_max_frame_time);

  m_accumulator += dt;

  int steps = 0;
  while (m_accumulator >= m_fixed_step && steps < m_max_substeps) {
    m_phys_sys.Update (static_cast<float> (m_fixed_step), 1, m_temp_alloc.get(), m_job_sys.get());
    m_accumulator -= m_fixed_step;
    steps++;
  }
}



void
phys::engine::clear ()
{
  auto &bi = m_phys_sys.GetBodyInterface ();
  JPH::BodyIDVector all_bodies;
  m_phys_sys.GetBodies (all_bodies);

  for (const JPH::BodyID &id : all_bodies) {
    bi.RemoveBody (id);
    bi.DestroyBody (id);
  }

  m_sensors.clear ();
  {
    std::scoped_lock const lk (m_sensor_evt_mtx);
    m_sensor_events.clear ();
  }

  m_accumulator = 0.0;
}

JPH::PhysicsSystem &
phys::engine::get_system ()
{
  return m_phys_sys;
}

JPH::BodyInterface &
phys::engine::get_body_interface ()
{
  return m_phys_sys.GetBodyInterface ();
}

const JPH::BodyLockInterfaceLocking &
phys::engine::get_body_lock_interface ()
{
  return m_phys_sys.GetBodyLockInterface ();
}

const JPH::NarrowPhaseQuery &
phys::engine::get_narrow_phase_query ()
{
  return m_phys_sys.GetNarrowPhaseQuery ();
}

JPH::TempAllocatorImpl &
phys::engine::get_temp_alloc ()
{
  return *m_temp_alloc;
}

double
phys::engine::get_gravity () const
{
  return m_gravity_y;
}

void
phys::engine::set_gravity (double gravity)
{
  m_gravity_y = gravity;
  m_phys_sys.SetGravity (JPH::Vec3 (0.0F, static_cast<float> (m_gravity_y), 0.0F));
}

double
phys::engine::get_fixed_step () const
{
  return m_fixed_step;
}

void
phys::engine::set_fixed_step (double step)
{
  m_fixed_step = std::max (step, 1.0e-4);
}

double
phys::engine::get_max_frame_time () const
{
  return m_max_frame_time;
}

void
phys::engine::set_max_frame_time (double max_dt)
{
  m_max_frame_time = std::max (max_dt, m_fixed_step);
}

int
phys::engine::get_max_substeps () const
{
  return m_max_substeps;
}

void
phys::engine::set_max_substeps (int max_steps)
{
  m_max_substeps = std::max (max_steps, 1);
}

void
phys::engine::on_remove_body (const JPH::BodyID &id)
{
  auto &body_interface = m_phys_sys.GetBodyInterface ();
  if (!id.IsInvalid ()) {
    body_interface.RemoveBody (id);
    body_interface.DestroyBody (id);
  }
}

// TODO: separate this into another class

void
phys::engine::register_sensor (const JPH::BodyID &id)
{
  if (!id.IsInvalid ()) {
    m_sensors.insert (id);
}
}

void
phys::engine::unregister_sensor (const JPH::BodyID &id)
{
  if (!id.IsInvalid ()) {
    m_sensors.erase (id);
}
}

bool
phys::engine::is_sensor (const JPH::BodyID &id) const
{
  return !id.IsInvalid () && m_sensors.contains (id);
}

void
phys::engine::push_sensor_event (const phys::sensor_overlap_event &ev)
{
  std::scoped_lock const lk (m_sensor_evt_mtx);
  m_sensor_events.push_back (ev);
}

std::vector<phys::sensor_overlap_event>
phys::engine::drain_sensor_events ()
{
  std::scoped_lock const lk (m_sensor_evt_mtx);
  std::vector<phys::sensor_overlap_event> out;
  out.swap (m_sensor_events);
  return out;
}

} // namespace wsl
