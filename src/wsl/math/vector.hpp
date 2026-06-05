#pragma once

#include "../comp/component_meta.hpp"

#include <cereal/cereal.hpp>

#include <entt/entt.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <imgui_internal.h>

namespace wsl
{

/**
 * @namespace wsl::math
 * @brief Common math types and vector utilities.
 */
namespace math
{

struct vec3f
{
  float x{ 0 }, y{ 0 }, z{ 0 };

  vec3f () = default;
  vec3f (float x, float y, float z) : x (x), y (y), z (z) {}
  vec3f (const glm::vec3 &v) : x (v.x), y (v.y), z (v.z) {}
  vec3f (const JPH::Vec3 &v) : x (v.GetX ()), y (v.GetY ()), z (v.GetZ ()) {}

  operator glm::vec3 () const { return glm::vec3{ x, y, z }; }
  operator JPH::Vec3 () const { return JPH::Vec3{ x, y, z }; }

  vec3f &
  operator+= (const glm::vec3 &v)
  {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  }

  vec3f &
  operator-= (const glm::vec3 &v)
  {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
  }

  // ---- NEW: custom inspector ----
  // Returns true if any value changed.
  bool
  custom_inspect (const char *label)
  {
    // Draw 3 floats on one line, each with a colored stripe on the left.
    // Stripe colors: X=red, Y=blue, Z=green (as requested).

    auto draw_drag_with_stripe
        = [] (const char *id, float &v, ImU32 stripe_col) -> bool {
      // Keep width reasonable even when the caller already placed us on
      // SameLine.
      ImGui::SetNextItemWidth (ImMax (1.0F, ImGui::CalcItemWidth ()));

      bool const changed = ImGui::DragFloat (id, &v, 0.1F);

      // Stripe overlay on the widget we just drew:
      ImDrawList *dl = ImGui::GetWindowDrawList ();
      ImVec2 const mn = ImGui::GetItemRectMin ();
      ImVec2 const mx = ImGui::GetItemRectMax ();

      const float stripe_w = 3.0F;
      dl->AddRectFilled (mn, ImVec2 (mn.x + stripe_w, mx.y), stripe_col);

      return changed;
    };

    // Use label as an ID seed so multiple vec3f on the same window don't
    // collide.
    ImGui::PushID (label);

    // Split available width into 3 items with spacing.
    float const full = ImGui::CalcItemWidth ();
    float const spacing = ImGui::GetStyle ().ItemInnerSpacing.x;
    float const w = (full - ((((((spacing * 2.0F))))))) / 3.0F;

    bool changed = false;

    // X
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##x", x, IM_COL32 (255, 0, 0, 255));

    ImGui::SameLine (0.0F, spacing);

    // Y
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##y", y, IM_COL32 (0, 200, 0, 255));

    ImGui::SameLine (0.0F, spacing);

    // Z
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##z", z, IM_COL32 (0, 128, 255, 255));

    ImGui::PopID ();
    return changed;
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<vec3f> ()
        .type (entt::type_hash<vec3f>::value ())

        // ---- NEW: register the custom inspector function in meta ----
        .func<&vec3f::custom_inspect> ("custom_inspect"_hs)

        .data<&vec3f::x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&vec3f::y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" })
        .data<&vec3f::z> ("z"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "z", "Z Coordinate", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("x", x), cereal::make_nvp ("y", y),
             cereal::make_nvp ("z", z));
  }
};

struct quatf
{
  float x{ 0 }, y{ 0 }, z{ 0 }, w{ 1 };

  quatf () = default;
  quatf (float x, float y, float z, float w) : x (x), y (y), z (z), w (w) {}
  quatf (const glm::quat &q) : x (q.x), y (q.y), z (q.z), w (q.w) {}

  operator glm::quat () const { return glm::quat{ w, x, y, z }; }

  bool
  custom_inspect (const char *label)
  {
    auto draw_drag_with_stripe
        = [] (const char *id, float &v, ImU32 stripe_col) -> bool {
      ImGui::SetNextItemWidth (ImMax (1.0F, ImGui::CalcItemWidth ()));
      bool const changed = ImGui::DragFloat (id, &v, 0.1F);
      ImDrawList *dl = ImGui::GetWindowDrawList ();
      ImVec2 const mn = ImGui::GetItemRectMin ();
      ImVec2 const mx = ImGui::GetItemRectMax ();
      const float stripe_w = 3.0F;
      dl->AddRectFilled (mn, ImVec2 (mn.x + stripe_w, mx.y), stripe_col);
      return changed;
    };

    ImGui::PushID (label);

    float const full = ImGui::CalcItemWidth ();
    float const spacing = ImGui::GetStyle ().ItemInnerSpacing.x;
    float const w = (full - spacing * 2.0F) / 3.0F;

    glm::vec3 euler
        = glm::degrees (glm::eulerAngles (static_cast<glm::quat> (*this)));

    bool changed = false;

    ImGui::SetNextItemWidth (w);
    changed
        |= draw_drag_with_stripe ("##x", euler.x, IM_COL32 (255, 0, 0, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (w);
    changed
        |= draw_drag_with_stripe ("##y", euler.y, IM_COL32 (0, 200, 0, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (w);
    changed
        |= draw_drag_with_stripe ("##z", euler.z, IM_COL32 (0, 128, 255, 255));

    if (changed) {
      glm::quat const q = glm::quat (glm::radians (euler));
      quatf &self = const_cast<quatf &> (*this);
      self.x = q.x;
      self.y = q.y;
      self.z = q.z;
      self.w = q.w;
    }

    ImGui::PopID ();
    return changed;
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<quatf> ()
        .type (entt::type_hash<quatf>::value ())
        .func<&quatf::custom_inspect> ("custom_inspect"_hs)
        .data<&quatf::x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&quatf::y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" })
        .data<&quatf::z> ("z"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "z", "Z Coordinate", "" })
        .data<&quatf::w> ("w"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "w", "W Coordinate", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("x", x), cereal::make_nvp ("y", y),
             cereal::make_nvp ("z", z), cereal::make_nvp ("w", w));
  }
};

} // namespace math

} // namespace wsl
