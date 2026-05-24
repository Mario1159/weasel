// character_body.hpp
#pragma once

#include "../math/vector.hpp"
#include "../phys/physics_engine.hpp"
#include "component_meta.hpp"
#include "singl/runtime_context.hpp"
#include <glm/glm.hpp>

#include <cereal/cereal.hpp>
#include <entt/entt.hpp>

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
// clang-format on


namespace wsl
{

namespace comp
{

class character_body : public world_component
{
public:
  character_body () = default;

  character_body (phys::engine &physics, const JPH::Vec3 &position,
                  float height = 1.8F, float radius = 0.4F);

  // runtime ops
  void create_body (phys::engine &physics, const JPH::Vec3 &position);
  void destroy_body ();

  // must be called after load
  void recreate (phys::engine &physics, const JPH::Vec3 &position);

  // called by inspector after any field edit
  void on_inspector_changed (comp::singl::runtime_context *runtime,
                             const glm::vec3 &scale = { 1, 1, 1 });

  JPH::CharacterVirtual *
  get ()
  {
    return m_body.GetPtr ();
  }
  const JPH::CharacterVirtual *
  get () const
  {
    return m_body.GetPtr ();
  }

  JPH::BodyID
  get_id () const
  {
    return ((m_body != nullptr) ? m_body->GetInnerBodyID () : JPH::BodyID ());
  }

  float height = 1.8F;
  float radius = 0.4F;
  math::vec3f desired_velocity = math::vec3f{ 0.0F, 0.0F, 0.0F };

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::character_body> ()
        .type (entt::type_hash<comp::character_body>::value ())
        .custom<comp::meta_info> (
            meta_info{ "Character Body",
                       "Capsule-based kinematic character controller (Jolt)",
                        "engine://icons/comp_character_body.svg" })
        .func<&comp::character_body::on_inspector_changed> (
            "on_inspector_changed"_hs)

        .data<&comp::character_body::height> ("height"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Height", "Capsule height in meters" })

        .data<&comp::character_body::radius> ("radius"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Radius", "Capsule radius in meters" })

        .data<&comp::character_body::desired_velocity> ("desired_velocity"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Desired Velocity", "Target movement velocity" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("height", height),
             cereal::make_nvp ("radius", radius),
             cereal::make_nvp ("desired_vel", desired_velocity));

    if constexpr (std::is_same_v<Archive, cereal::BinaryInputArchive>
                  || std::is_same_v<Archive, cereal::JSONInputArchive>) {
      // runtime-only
      m_body = nullptr;

      // reset cache so next inspector edit applies cleanly
      m_applied_height = height;
      m_applied_radius = radius;
    }
  }

private:
  static constexpr float min_half_height = 1e-3F; // must be > 0 for Jolt assert
  static constexpr float min_radius = 1e-3F;

  static void sanitize_dimensions (float &height, float &radius) ;
  static float capsule_half_height (float height, float radius) ;

  void build_settings (JPH::CharacterVirtualSettings &settings) const;


  JPH::Ref<JPH::CharacterVirtual> m_body;

  // runtime cache to detect edits
  float m_applied_height = 1.8F;
  float m_applied_radius = 0.4F;
};

} // namespace comp

} // namespace wsl
