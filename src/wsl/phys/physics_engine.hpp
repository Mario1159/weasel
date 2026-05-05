#pragma once

#include <Jolt/Jolt.h>
#include <mutex>
#include <memory>
#include <unordered_set>
#include <vector>
#include "broad_phase_layer_interface.hpp"
#include "object_layer_pair_filter.hpp"
#include "object_vs_broad_phase_layer_filter.hpp"

// clang-format off
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
// clang-format on

namespace wsl
{

/**
 * @namespace wsl::phys
 * @brief Physics engine interfaces and Jolt Physics integration.
 */
namespace phys
{

using body_id = JPH::BodyID;

// Type aliases to decouple Jolt types from the wider engine API. Replace uses
// of these aliases in core headers so the engine can swap/mock physics
// backends.
using motion_type = JPH::EMotionType;
using allowed_do_fs = JPH::EAllowedDOFs;
using object_layer = JPH::ObjectLayer;

struct sensor_overlap_event
{
  body_id sensor;
  body_id other;
  bool entered = false; // true=enter, false=exit
};

class contact_listener_impl;

class engine
{
public:
  engine ();
  ~engine ();

  void step (double dt);
  void clear ();

  // void on_add_body(const JPH::BodyID &id);
  void on_remove_body (const body_id &id);

  // internal interfaces
  JPH::PhysicsSystem &get_system ();
  JPH::BodyInterface &get_body_interface ();
  const JPH::BodyLockInterfaceLocking &get_body_lock_interface ();
  const JPH::NarrowPhaseQuery &get_narrow_phase_query ();
  JPH::TempAllocatorImpl &get_temp_alloc ();

  // get/set
  double get_gravity () const;
  void set_gravity (double gravity);
  double get_fixed_step () const;
  void set_fixed_step (double step);
  double get_max_frame_time () const;
  void set_max_frame_time (double max_dt);
  int get_max_substeps () const;
  void set_max_substeps (int max_steps);

  void register_sensor (const body_id &id);
  void unregister_sensor (const body_id &id);
  bool is_sensor (const body_id &id) const;
  void push_sensor_event (const sensor_overlap_event &ev);

  std::vector<sensor_overlap_event> drain_sensor_events ();

private:
  // jolt objects
  std::unique_ptr<JPH::TempAllocatorImpl> m_temp_alloc;
  std::unique_ptr<JPH::JobSystemThreadPool> m_job_sys;
  JPH::PhysicsSystem m_phys_sys;

  // layer interfaces
  std::unique_ptr<broad_phase_layer_interface> m_bp_layer_if;
  std::unique_ptr<object_vs_broad_phase_layer_filter> m_obj_vs_bp_layer_filter;
  std::unique_ptr<object_layer_pair_filter> m_obj_layer_filter;

  double m_accumulator = 0.0;

  struct bodyid_hash
  {
    size_t
    operator() (const body_id &id) const noexcept
    {
      return (size_t)id.GetIndexAndSequenceNumber ();
    }
  };

  std::unordered_set<body_id, bodyid_hash> m_sensors;

  std::mutex m_sensor_evt_mtx;
  std::vector<sensor_overlap_event> m_sensor_events;

  std::unique_ptr<contact_listener_impl> m_contact_listener;

  double m_gravity_y = -9.8;
  double m_fixed_step = 1.0 / 60.0;
  double m_max_frame_time = 0.25;
  int m_max_substeps = 5;
};

class contact_listener_impl final : public JPH::ContactListener
{
public:
  explicit contact_listener_impl (phys::engine &e) : m_eng (e) {}

  JPH::ValidateResult
  OnContactValidate (
      const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
      JPH::RVec3 /*inBaseOffset*/,
      const JPH::CollideShapeResult & /*inCollisionResult*/) override
  {
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
  }

  void
  OnContactAdded (
      const JPH::Body &body1, const JPH::Body &body2,
      const JPH::ContactManifold & /*inManifold*/ /*inManifold*/ /*inManifold*/
      /*inManifold*/ /*inManifold*/ /*inManifold*/ /*inManifold*/,
      JPH::ContactSettings & /*ioSettings*/ /*ioSettings*/ /*ioSettings*/
      /*ioSettings*/ /*ioSettings*/ /*ioSettings*/ /*ioSettings*/) override
  {
    const JPH::BodyID a = body1.GetID ();
    const JPH::BodyID b = body2.GetID ();

    // Determine if one is a registered sensor
    const bool a_sensor = m_eng.is_sensor (a);
    const bool b_sensor = m_eng.is_sensor (b);
    if ((a_sensor ^ b_sensor) == 0) {
      return; // only care sensor vs non-sensor
    }

    phys::sensor_overlap_event ev;
    ev.entered = true;
    ev.sensor = a_sensor ? a : b;
    ev.other = a_sensor ? b : a;

    m_eng.push_sensor_event (ev);
  }

  void
  OnContactRemoved (const JPH::SubShapeIDPair &pair) override
  {
    const JPH::BodyID a = pair.GetBody1ID ();
    const JPH::BodyID b = pair.GetBody2ID ();

    const bool a_sensor = m_eng.is_sensor (a);
    const bool b_sensor = m_eng.is_sensor (b);
    if ((a_sensor ^ b_sensor) == 0) {
      {
        return;
      }
    }

    phys::sensor_overlap_event ev;
    ev.entered = false;
    ev.sensor = a_sensor ? a : b;
    ev.other = a_sensor ? b : a;

    m_eng.push_sensor_event (ev);
  }

private:
  phys::engine &m_eng;
};

} // namespace phys

} // namespace wsl
