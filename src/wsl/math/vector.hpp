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
 * Common math types and vector utilities.
 */
namespace math
{

struct vec2f
{
  vec2f () = default;
  vec2f (float x_val, float y_val) : m_x (x_val), m_y (y_val) {}
  vec2f (const glm::vec2 &v) : m_x (v.x), m_y (v.y) {}

  float
  x () const
  {
    return m_x;
  }
  float &
  x ()
  {
    return m_x;
  }
  float
  y () const
  {
    return m_y;
  }
  float &
  y ()
  {
    return m_y;
  }

  operator glm::vec2 () const { return glm::vec2{ m_x, m_y }; }

  bool
  operator== (const vec2f &other) const
  {
    return m_x == other.m_x && m_y == other.m_y;
  }
  bool
  operator!= (const vec2f &other) const
  {
    return !(*this == other);
  }

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
    float const w = (full - spacing) / 2.0F;

    bool changed = false;

    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##x", m_x, IM_COL32 (255, 0, 0, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##y", m_y, IM_COL32 (0, 200, 0, 255));

    ImGui::PopID ();
    return changed;
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    vec2f def{};
    wsl::comp::serialize_field_if_diff (archive, "x", m_x, def.m_x);
    wsl::comp::serialize_field_if_diff (archive, "y", m_y, def.m_y);
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<vec2f> ()
        .type (entt::type_hash<vec2f>::value ())
        .func<&vec2f::custom_inspect> ("custom_inspect"_hs)
        .data<&vec2f::m_x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&vec2f::m_y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" });
  }

private:
  float m_x{ 0 }, m_y{ 0 };
};

struct vec3f
{
  vec3f () = default;
  vec3f (float x_val, float y_val, float z_val)
      : m_x (x_val), m_y (y_val), m_z (z_val)
  {
  }
  vec3f (const glm::vec3 &v) : m_x (v.x), m_y (v.y), m_z (v.z) {}
  vec3f (const JPH::Vec3 &v) : m_x (v.GetX ()), m_y (v.GetY ()), m_z (v.GetZ ())
  {
  }

  float
  x () const
  {
    return m_x;
  }
  float &
  x ()
  {
    return m_x;
  }
  float
  y () const
  {
    return m_y;
  }
  float &
  y ()
  {
    return m_y;
  }
  float
  z () const
  {
    return m_z;
  }
  float &
  z ()
  {
    return m_z;
  }

  operator glm::vec3 () const { return glm::vec3{ m_x, m_y, m_z }; }
  operator JPH::Vec3 () const { return JPH::Vec3{ m_x, m_y, m_z }; }

  bool
  operator== (const vec3f &other) const
  {
    return m_x == other.m_x && m_y == other.m_y && m_z == other.m_z;
  }
  bool
  operator!= (const vec3f &other) const
  {
    return !(*this == other);
  }

  vec3f &
  operator+= (const glm::vec3 &v)
  {
    m_x += v.x;
    m_y += v.y;
    m_z += v.z;
    return *this;
  }

  vec3f &
  operator-= (const glm::vec3 &v)
  {
    m_x -= v.x;
    m_y -= v.y;
    m_z -= v.z;
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
    float const w = (full - (spacing * 2.0F)) / 3.0F;

    bool changed = false;

    // X
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##x", m_x, IM_COL32 (255, 0, 0, 255));

    ImGui::SameLine (0.0F, spacing);

    // Y
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##y", m_y, IM_COL32 (0, 200, 0, 255));

    ImGui::SameLine (0.0F, spacing);

    // Z
    ImGui::SetNextItemWidth (w);
    changed |= draw_drag_with_stripe ("##z", m_z, IM_COL32 (0, 128, 255, 255));

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

        .data<&vec3f::m_x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&vec3f::m_y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" })
        .data<&vec3f::m_z> ("z"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "z", "Z Coordinate", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    vec3f def{};
    wsl::comp::serialize_field_if_diff (archive, "x", m_x, def.m_x);
    wsl::comp::serialize_field_if_diff (archive, "y", m_y, def.m_y);
    wsl::comp::serialize_field_if_diff (archive, "z", m_z, def.m_z);
  }

private:
  float m_x{ 0 }, m_y{ 0 }, m_z{ 0 };
};

struct vec4f
{
  vec4f () = default;
  vec4f (float x_val, float y_val, float z_val, float w_val)
      : m_x (x_val), m_y (y_val), m_z (z_val), m_w (w_val)
  {
  }
  vec4f (const glm::vec4 &v) : m_x (v.x), m_y (v.y), m_z (v.z), m_w (v.w) {}

  float
  x () const
  {
    return m_x;
  }
  float &
  x ()
  {
    return m_x;
  }
  float
  y () const
  {
    return m_y;
  }
  float &
  y ()
  {
    return m_y;
  }
  float
  z () const
  {
    return m_z;
  }
  float &
  z ()
  {
    return m_z;
  }
  float
  w () const
  {
    return m_w;
  }
  float &
  w ()
  {
    return m_w;
  }

  operator glm::vec4 () const { return glm::vec4{ m_x, m_y, m_z, m_w }; }

  bool
  operator== (const vec4f &other) const
  {
    return m_x == other.m_x && m_y == other.m_y && m_z == other.m_z
           && m_w == other.m_w;
  }
  bool
  operator!= (const vec4f &other) const
  {
    return !(*this == other);
  }

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
    float const item_w = (full - (spacing * 3.0F)) / 4.0F;

    bool changed = false;

    ImGui::SetNextItemWidth (item_w);
    changed |= draw_drag_with_stripe ("##x", m_x, IM_COL32 (255, 0, 0, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (item_w);
    changed |= draw_drag_with_stripe ("##y", m_y, IM_COL32 (0, 200, 0, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (item_w);
    changed |= draw_drag_with_stripe ("##z", m_z, IM_COL32 (0, 128, 255, 255));
    ImGui::SameLine (0.0F, spacing);

    ImGui::SetNextItemWidth (item_w);
    changed
        |= draw_drag_with_stripe ("##w", m_w, IM_COL32 (255, 255, 255, 255));

    ImGui::PopID ();
    return changed;
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    vec4f def{};
    wsl::comp::serialize_field_if_diff (archive, "x", m_x, def.m_x);
    wsl::comp::serialize_field_if_diff (archive, "y", m_y, def.m_y);
    wsl::comp::serialize_field_if_diff (archive, "z", m_z, def.m_z);
    wsl::comp::serialize_field_if_diff (archive, "w", m_w, def.m_w);
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<vec4f> ()
        .type (entt::type_hash<vec4f>::value ())
        .func<&vec4f::custom_inspect> ("custom_inspect"_hs)
        .data<&vec4f::m_x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&vec4f::m_y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" })
        .data<&vec4f::m_z> ("z"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "z", "Z Coordinate", "" })
        .data<&vec4f::m_w> ("w"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "w", "W Coordinate", "" });
  }

private:
  float m_x{ 0 }, m_y{ 0 }, m_z{ 0 }, m_w{ 0 };
};

struct quatf
{
  quatf () = default;
  quatf (float x_val, float y_val, float z_val, float w_val)
      : m_x (x_val), m_y (y_val), m_z (z_val), m_w (w_val)
  {
  }
  quatf (const glm::quat &q) : m_x (q.x), m_y (q.y), m_z (q.z), m_w (q.w) {}

  float
  x () const
  {
    return m_x;
  }
  float &
  x ()
  {
    return m_x;
  }
  float
  y () const
  {
    return m_y;
  }
  float &
  y ()
  {
    return m_y;
  }
  float
  z () const
  {
    return m_z;
  }
  float &
  z ()
  {
    return m_z;
  }
  float
  w () const
  {
    return m_w;
  }
  float &
  w ()
  {
    return m_w;
  }

  operator glm::quat () const { return glm::quat{ m_w, m_x, m_y, m_z }; }

  bool
  operator== (const quatf &other) const
  {
    return m_x == other.m_x && m_y == other.m_y && m_z == other.m_z
           && m_w == other.m_w;
  }
  bool
  operator!= (const quatf &other) const
  {
    return !(*this == other);
  }

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
    float const w = (full - (spacing * 2.0F)) / 3.0F;

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
      self.m_x = q.x;
      self.m_y = q.y;
      self.m_z = q.z;
      self.m_w = q.w;
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
        .data<&quatf::m_x> ("x"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "x", "X Coordinate", "" })
        .data<&quatf::m_y> ("y"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "y", "Y Coordinate", "" })
        .data<&quatf::m_z> ("z"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "z", "Z Coordinate", "" })
        .data<&quatf::m_w> ("w"_hs)
        .custom<comp::meta_info> (comp::meta_info{ "w", "W Coordinate", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    quatf def{};
    wsl::comp::serialize_field_if_diff (archive, "x", m_x, def.m_x);
    wsl::comp::serialize_field_if_diff (archive, "y", m_y, def.m_y);
    wsl::comp::serialize_field_if_diff (archive, "z", m_z, def.m_z);
    wsl::comp::serialize_field_if_diff (archive, "w", m_w, def.m_w);
  }

private:
  float m_x{ 0 }, m_y{ 0 }, m_z{ 0 }, m_w{ 1 };
};

} // namespace math

} // namespace wsl
