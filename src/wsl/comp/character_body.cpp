// character_body.cpp
#include "character_body.hpp"

#include "../phys/layers.hpp"
#include "comp/singl/runtime_context.hpp"
#include "phys/physics_engine.hpp"

#include <algorithm> // std::max
#include <glm/ext/vector_float3.hpp>

namespace wsl
{

namespace comp
{

struct character_body::impl
{
};

character_body::character_body () = default;
character_body::~character_body () = default;

character_body::character_body (character_body &&) noexcept = default;
character_body &
character_body::operator= (character_body &&) noexcept = default;

bool
character_body::valid () const noexcept
{
  return false;
}

phys::body_id
character_body::get_id () const noexcept
{
  return phys::null_body_id;
}

void *
character_body::native_handle () noexcept
{
  return nullptr;
}

const void *
character_body::native_handle () const noexcept
{
  return const_cast<character_body *> (this)->native_handle ();
}

void
character_body::sanitize_dimensions (float &h, float &r)
{
  // Make sure radius is valid
  r = std::max (r, min_radius);

  // Need: half_h = 0.5*h - r > 0  =>  h > 2*r
  // We enforce a strict margin so half_h >= min_half_height.
  const float min_h = 2.0F * (r + min_half_height);
  h = std::max (h, min_h);
}

float
character_body::capsule_half_height (float h, float r)
{
  // After sanitize_dimensions this is guaranteed >= min_half_height,
  // but keep it robust anyway.
  const float half_h = (0.5F * h) - r;
  return std::max (min_half_height, half_h);
}

character_body::character_body (phys::engine &physics,
                                const glm::vec3 &position, float h, float r)
  : m_impl (std::make_unique<impl> ())
{
  (void)physics;
  (void)position;
  height = h;
  radius = r;
  sanitize_dimensions (height, radius);
}

void
character_body::create_body (phys::engine &physics, const glm::vec3 &position)
{
  (void)physics;
  (void)position;
  sanitize_dimensions (height, radius);
  m_applied_height = height;
  m_applied_radius = radius;
}

void
character_body::destroy_body ()
{
}

void
character_body::recreate (phys::engine &physics, const glm::vec3 &position)
{
  create_body (physics, position);
}

void
character_body::on_inspector_changed (comp::singl::runtime_context *runtime,
                                      const glm::vec3 & /*unused*/)
{
  phys::engine *engine
      = (runtime != nullptr) ? runtime->try_get_active_physics_engine () : nullptr;
  if (engine == nullptr) {
    return;
  }

  // If user typed invalid values (height <= 2*radius), fix them here.
  float new_h = height;
  float new_r = radius;
  sanitize_dimensions (new_h, new_r);

  // Write back sanitized values so UI shows the truth.
  height = new_h;
  radius = new_r;

  const bool changed = (height != m_applied_height) || (radius != m_applied_radius);
  if (!changed) {
    return;
  }

  // Keep current world position if we already exist; otherwise origin.
  glm::vec3 pos{ 0.0F, 0.0F, 0.0F };
  recreate (*engine, pos);
}

void
character_body::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<comp::character_body> ()
      .type (entt::type_hash<comp::character_body>::value ())
      .custom<comp::meta_info> (
          meta_info{ "Character Body",
                     "Capsule-based kinematic character controller (Box3D)",
                     "engine://icons/comp_character_body.svg" })
      .func<&comp::character_body::on_inspector_changed> (
          "on_inspector_changed"_hs)

      .data<&comp::character_body::height> ("height"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Height", "Capsule height in meters", "" })

      .data<&comp::character_body::radius> ("radius"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Radius", "Capsule radius in meters", "" })

      .data<&comp::character_body::desired_velocity> ("desired_velocity"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Desired Velocity", "Target movement velocity", "" });
}

} // namespace comp

} // namespace wsl
