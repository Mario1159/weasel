#include "rigid_body.hpp"

#include "../phys/utils.hpp"
#include "comp/component_meta.hpp"
#include "comp/singl/runtime_context.hpp"
#include "phys/layers.hpp"
#include "phys/physics_engine.hpp"

#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <Jolt/Physics/EActivation.h>
#include <cstdint>
#include <cstdio>
#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/meta/factory.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <imgui.h>

namespace wsl
{

namespace
{

bool
draw_layer_button (const char *label, bool active)
{
  const ImVec2 size{ 28.0F, 28.0F };
  const ImVec4 color = active ? ImVec4 (0.26F, 0.56F, 0.96F, 1.0F)
                              : ImVec4 (0.19F, 0.20F, 0.23F, 1.0F);
  const ImVec4 hovered = active ? ImVec4 (0.33F, 0.63F, 1.0F, 1.0F)
                                : ImVec4 (0.25F, 0.27F, 0.31F, 1.0F);
  const ImVec4 pressed = active ? ImVec4 (0.21F, 0.49F, 0.88F, 1.0F)
                                : ImVec4 (0.16F, 0.17F, 0.20F, 1.0F);

  ImGui::PushStyleColor (ImGuiCol_Button, color);
  ImGui::PushStyleColor (ImGuiCol_ButtonHovered, hovered);
  ImGui::PushStyleColor (ImGuiCol_ButtonActive, pressed);
  const bool clicked = ImGui::Button (label, size);
  ImGui::PopStyleColor (3);
  return clicked;
}

template <typename IsActiveFn, typename OnPressFn>
bool
draw_layer_grid (const char *label, IsActiveFn &&is_active,
                 OnPressFn &&on_press)
{
  bool changed = false;

  ImGui::PushID (label);
  ImGui::BeginGroup ();

  for (uint32_t i = 0; i < phys::layers::collision_layer_count; ++i) {
    if (i > 0 && (i % 4) != 0) {
      ImGui::SameLine ();
    }

    char button_label[4];
    std::snprintf (button_label, sizeof (button_label), "%u", i + 1);

    if (draw_layer_button (button_label, is_active (i))) {
      on_press (i);
      changed = true;
    }
  }

  ImGui::EndGroup ();
  ImGui::PopID ();

  return changed;
}

} // namespace

namespace comp
{

// ---------------- rigid_body::allowed_dofs_ui ----------------

void
rigid_body::allowed_dofs_ui::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<rigid_body::allowed_dofs_ui> ()
      .type (entt::type_hash<rigid_body::allowed_dofs_ui>::value ())
      .func<&rigid_body::allowed_dofs_ui::custom_inspect> ("custom_inspect"_hs)
      .data<&rigid_body::allowed_dofs_ui::value> ("value"_hs);
}

bool
rigid_body::allowed_dofs_ui::custom_inspect (
    const char *label, comp::singl::runtime_context * /*unused*/)
{
  static const char *const names[] = { "All",
                                       "Translation X",
                                       "Translation Y",
                                       "Translation Z",
                                       "Rotation X",
                                       "Rotation Y",
                                       "Rotation Z",
                                       "Translation XY",
                                       "Translation XZ",
                                       "Translation YZ",
                                       "Rotation XY",
                                       "Rotation XZ",
                                       "Rotation YZ" };

  static const phys::allowed_do_fs vals[] = {
    phys::allowed_do_fs::All,
    phys::allowed_do_fs::TranslationX,
    phys::allowed_do_fs::TranslationY,
    phys::allowed_do_fs::TranslationZ,
    phys::allowed_do_fs::RotationX,
    phys::allowed_do_fs::RotationY,
    phys::allowed_do_fs::RotationZ,
    phys::allowed_do_fs::TranslationX | phys::allowed_do_fs::TranslationY,
    phys::allowed_do_fs::TranslationX | phys::allowed_do_fs::TranslationZ,
    phys::allowed_do_fs::TranslationY | phys::allowed_do_fs::TranslationZ,
    phys::allowed_do_fs::RotationX | phys::allowed_do_fs::RotationY,
    phys::allowed_do_fs::RotationX | phys::allowed_do_fs::RotationZ,
    phys::allowed_do_fs::RotationY | phys::allowed_do_fs::RotationZ,
  };

  int cur = 0;
  for (int i = 0; i < (int)(sizeof (vals) / sizeof (vals[0])); ++i) {
    if (value == vals[i]) {
      cur = i;
      break;
    }
  }

  bool changed = false;
  if (ImGui::BeginCombo (label, names[cur])) {
    for (int i = 0; i < (int)(sizeof (vals) / sizeof (vals[0])); ++i) {
      const bool sel = (i == cur);
      if (ImGui::Selectable (names[i], sel)) {
        value = vals[i];
        changed = true;
      }
      if (sel) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }
  return changed;
}

// ---------------- rigid_body::motion_type_ui ----------------

void
rigid_body::motion_type_ui::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<rigid_body::motion_type_ui> ()
      .type (entt::type_hash<rigid_body::motion_type_ui>::value ())
      .func<&rigid_body::motion_type_ui::custom_inspect> ("custom_inspect"_hs)
      .data<&rigid_body::motion_type_ui::value> ("value"_hs);
}

bool
rigid_body::motion_type_ui::custom_inspect (
    const char *label, comp::singl::runtime_context * /*unused*/)
{
  static const char *const names[] = { "Static", "Kinematic", "Dynamic" };
  static const phys::motion_type vals[]
      = { phys::motion_type::Static, phys::motion_type::Kinematic,
          phys::motion_type::Dynamic };

  int cur = 0;
  for (int i = 0; i < 3; ++i) {
    if (value == vals[i]) {
      cur = i;
      break;
    }
  }

  bool changed = false;
  if (ImGui::BeginCombo (label, names[cur])) {
    for (int i = 0; i < 3; ++i) {
      const bool sel = (i == cur);
      if (ImGui::Selectable (names[i], sel)) {
        value = vals[i];
        changed = true;
      }
      if (sel) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }

  return changed;
}

// ---------------- rigid_body::collision_layer_ui ----------------

void
rigid_body::collision_layer_ui::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<rigid_body::collision_layer_ui> ()
      .type (entt::type_hash<rigid_body::collision_layer_ui>::value ())
      .func<&rigid_body::collision_layer_ui::custom_inspect> (
          "custom_inspect"_hs)
      .data<&rigid_body::collision_layer_ui::value> ("value"_hs);
}

bool
rigid_body::collision_layer_ui::custom_inspect (
    const char *label, comp::singl::runtime_context * /*unused*/)
{
  return draw_layer_grid (
      label, [this] (uint32_t index) { return value == index; },
      [this] (uint32_t index) {
        value = phys::layers::clamp_layer_index (
            static_cast<phys::layers::layer_index_t> (index));
      });
}

// ---------------- rigid_body::collision_mask_ui ----------------

void
rigid_body::collision_mask_ui::register_meta ()
{
  using namespace entt::literals;

  entt::meta_factory<rigid_body::collision_mask_ui> ()
      .type (entt::type_hash<rigid_body::collision_mask_ui>::value ())
      .func<&rigid_body::collision_mask_ui::custom_inspect> (
          "custom_inspect"_hs)
      .data<&rigid_body::collision_mask_ui::value> ("value"_hs);
}

bool
rigid_body::collision_mask_ui::custom_inspect (
    const char *label, comp::singl::runtime_context * /*unused*/)
{
  return draw_layer_grid (
      label,
      [this] (uint32_t index) {
        return (value
                & phys::layers::bit_for_layer (
                    static_cast<phys::layers::layer_index_t> (index)))
               != 0;
      },
      [this] (uint32_t index) {
        const phys::layers::layer_mask_t bit = phys::layers::bit_for_layer (
            static_cast<phys::layers::layer_index_t> (index));
        value = (value & bit) != 0 ? value & ~bit : value | bit;
        value = phys::layers::clamp_layer_mask (value);
      });
}

// ---------------- rigid_body runtime ----------------

void
rigid_body::sync_applied_cache ()
{
  applied_shape = shape;
  applied_half_extents = half_extents;
  applied_radius = radius;
  applied_dynamic = dynamic;
  applied_motion = motion_type.value;
  applied_dofs = allowed_dofs.value;
  applied_collision_layer = collision_layer.value;
  applied_collision_mask = collision_mask.value;
  applied_friction = friction;
  applied_restitution = restitution;
  applied_position = position;
  applied_rotation = rotation;
}

JPH::ObjectLayer
rigid_body::object_layer () const
{
  const phys::layers::motion_bucket motion
      = motion_type.value == phys::motion_type::Static
            ? phys::layers::motion_bucket::static_body
            : phys::layers::motion_bucket::moving_body;

  return phys::layers::make_rigidbody_object_layer (
      collision_layer.value, collision_mask.value, motion);
}

bool
rigid_body::has_structural_change () const
{
  return shape != applied_shape || half_extents.x != applied_half_extents.x
         || half_extents.y != applied_half_extents.y
         || half_extents.z != applied_half_extents.z || radius != applied_radius
         || dynamic != applied_dynamic || motion_type.value != applied_motion
         || allowed_dofs.value != applied_dofs;
}

bool
rigid_body::has_surface_change () const
{
  return collision_layer.value != applied_collision_layer
         || collision_mask.value != applied_collision_mask
         || friction != applied_friction || restitution != applied_restitution;
}

bool
rigid_body::has_transform_change () const
{
  return position.x != applied_position.x || position.y != applied_position.y
         || position.z != applied_position.z || rotation.x != applied_rotation.x
         || rotation.y != applied_rotation.y || rotation.z != applied_rotation.z
         || rotation.w != applied_rotation.w;
}

rigid_body
rigid_body::create_box_body (phys::engine &engine, const glm::vec3 &pos,
                             const glm::quat &rot, const glm::vec3 &he,
                             bool dyn)
{
  rigid_body rb;
  rb.shape = shape_type::box;
  rb.position = math::vec3f{ pos };
  rb.rotation = math::quatf{ rot };
  rb.half_extents = math::vec3f{ he };

  rb.dynamic = dyn;
  rb.motion_type.value
      = dyn ? phys::motion_type::Dynamic : phys::motion_type::Static;
  rb.allowed_dofs.value = JPH::EAllowedDOFs::All;

  rb.sync_applied_cache ();
  rb.create_body (engine);
  return rb;
}

rigid_body
rigid_body::create_sphere_body (phys::engine &engine, const glm::vec3 &pos,
                                float r, phys::motion_type motion,
                                JPH::EAllowedDOFs dofs)
{
  rigid_body rb;
  rb.shape = shape_type::sphere;
  rb.position = math::vec3f{ pos };
  rb.rotation = math::quatf{ glm::quat (1, 0, 0, 0) };
  rb.radius = r;

  rb.motion_type.value = motion;
  rb.allowed_dofs.value = dofs;
  rb.dynamic = (motion == phys::motion_type::Dynamic);

  rb.sync_applied_cache ();
  rb.create_body (engine);
  return rb;
}

void
rigid_body::destroy_body (phys::engine &engine)
{
  if (body_id.IsInvalid ()) {
    return;
  }

  engine.on_remove_body (body_id);
  body_id = phys::body_id{};
}

void
rigid_body::rebuild_body (phys::engine &engine, const glm::vec3 &scale)
{
  destroy_body (engine);
  create_body (engine, scale);
}

void
rigid_body::create_body (phys::engine &engine, const glm::vec3 &scale)
{
  if (!body_id.IsInvalid ()) {
    destroy_body (engine);
  }

  JPH::ShapeRefC shape_ref;

  if (shape == shape_type::box) {
    JPH::RefConst<JPH::ShapeSettings> const s = new JPH::BoxShapeSettings (
        to_jolt (half_extents) * JPH::Vec3 (scale.x, scale.y, scale.z));
    shape_ref = s->Create ().Get ();
  } else {
    // For spheres we use the average scale or just X
    float const avg_scale = (scale.x + scale.y + scale.z) / 3.0F;
    JPH::RefConst<JPH::ShapeSettings> const s
        = new JPH::SphereShapeSettings (radius * avg_scale);
    shape_ref = s->Create ().Get ();
  }

  dynamic = (motion_type.value == phys::motion_type::Dynamic);

  const JPH::RVec3 pos = to_jolt (position);
  const JPH::Quat rot = to_jolt (rotation);

  JPH::BodyCreationSettings settings (shape_ref, pos, rot, motion_type.value,
                                      object_layer ());

  settings.mAllowedDOFs = allowed_dofs.value;
  settings.mFriction = friction;
  settings.mRestitution = restitution;
  settings.mOverrideMassProperties
      = JPH::EOverrideMassProperties::CalculateMassAndInertia;

  body_id = engine.get_body_interface ().CreateAndAddBody (
      settings, JPH::EActivation::Activate);
}

void
rigid_body::apply_transform_to_body (phys::engine &engine) const
{
  if (body_id.IsInvalid ()) {
    return;
  }

  auto &bi = engine.get_body_interface ();
  bi.SetPositionAndRotation (body_id, to_jolt (position), to_jolt (rotation),
                             JPH::EActivation::Activate);
}

void
rigid_body::apply_surface_properties_to_body (phys::engine &engine) const
{
  if (body_id.IsInvalid ()) {
    return;
  }

  auto &bi = engine.get_body_interface ();
  bi.SetFriction (body_id, friction);
  bi.SetRestitution (body_id, restitution);
  bi.SetObjectLayer (body_id, object_layer ());
}

void
rigid_body::on_inspector_changed (comp::singl::runtime_context *runtime,
                                  const glm::vec3 &scale)
{
  phys::engine *engine = (runtime != nullptr)
                             ? runtime->try_get_active_physics_engine ()
                             : nullptr;
  if (engine == nullptr) {
    return;
  }

  // Sanitize dimensions
  sanitize_dimensions ();
  sanitize_surface_properties ();

  const bool structural_change = has_structural_change ();
  const bool surface_change = has_surface_change ();
  const bool xform_change = has_transform_change ();

  if (body_id.IsInvalid ()) {
    create_body (*engine, scale);
  } else if (structural_change) {
    rebuild_body (*engine, scale);
  } else {
    if (xform_change) {
      apply_transform_to_body (*engine);
    }
    if (surface_change) {
      apply_surface_properties_to_body (*engine);
    }
  }

  sync_applied_cache ();
}

void
rigid_body::register_meta ()
{
  using namespace entt::literals;

  rigid_body::motion_type_ui::register_meta ();
  rigid_body::allowed_dofs_ui::register_meta ();
  rigid_body::collision_layer_ui::register_meta ();
  rigid_body::collision_mask_ui::register_meta ();

  entt::meta_factory<comp::rigid_body::shape_type> ()
      .type (entt::type_hash<comp::rigid_body::shape_type>::value ())
      .conv<int> ()
      .data<comp::rigid_body::shape_type::box> ("box"_hs)
      .custom<const char *> ("Box")
      .data<comp::rigid_body::shape_type::sphere> ("sphere"_hs)
      .custom<const char *> ("Sphere");

  entt::meta_factory<comp::rigid_body> ()
      .type (entt::type_hash<comp::rigid_body>::value ())
      .custom<comp::meta_info> (meta_info{ "Rigid Body",
                                           "Physics body simulated by Jolt",
                                            "engine://icons/comp_rigidbody.svg" })
      .func<&comp::rigid_body::on_inspector_changed> ("on_inspector_changed"_hs)

      .data<&comp::rigid_body::shape> ("shape"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Shape", "Collision shape type", "" })

      .data<&comp::rigid_body::position> ("position"_hs)
      .custom<comp::meta_info> (meta_info{ "Position", "World position", "" })

      .data<&comp::rigid_body::rotation> ("rotation"_hs)
      .custom<comp::meta_info> (meta_info{ "Rotation", "World rotation", "" })

      .data<&comp::rigid_body::half_extents> ("half_extents"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Half Extents", "Box half size", "" })

      .data<&comp::rigid_body::radius> ("radius"_hs)
      .custom<comp::meta_info> (meta_info{ "Radius", "Sphere radius", "" })

      .data<&comp::rigid_body::dynamic> ("dynamic"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Dynamic", "Derived from motion type", "" })

      .data<&comp::rigid_body::motion_type> ("motion_type"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Motion Type", "Jolt motion type", "" })

      .data<&comp::rigid_body::allowed_dofs> ("allowed_dofs"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Allowed DOFs", "Movement constraints", "" })

      .data<&comp::rigid_body::collision_layer> ("collision_layer"_hs)
      .custom<comp::meta_info> (meta_info{
          "Collision Layer", "Which physics layer this body belongs to", "" })

      .data<&comp::rigid_body::collision_mask> ("collision_mask"_hs)
      .custom<comp::meta_info> (
          meta_info{ "Collision Mask",
                     "Which physics layers this body collides with", "" })

      .data<&comp::rigid_body::friction> ("friction"_hs)
      .custom<comp::meta_info> (meta_info{
          "Friction", "Surface friction used by the physics solver", "" })

      .data<&comp::rigid_body::restitution> ("restitution"_hs)
      .custom<comp::meta_info> (meta_info{
          "Restitution", "Surface bounciness used by the physics solver", "" })

      .data<&comp::rigid_body::body_id> ("body_id"_hs);
}

} // namespace comp

} // namespace wsl
