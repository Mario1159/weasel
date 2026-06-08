#pragma once

#include "../comp/component_meta.hpp"

#include <cereal/cereal.hpp>

#include <entt/entt.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>

namespace wsl
{

namespace math
{

struct mat33f
{
  /*! Column-major storage: data[col * 3 + row] */
  float data[9]{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };

  mat33f () = default;

  mat33f (const glm::mat3 &m)
  {
    for (int col = 0; col < 3; ++col)
      for (int row = 0; row < 3; ++row)
        data[col * 3 + row] = m[col][row];
  }

  operator glm::mat3 () const
  {
    glm::mat3 r;
    for (int col = 0; col < 3; ++col)
      for (int row = 0; row < 3; ++row)
        r[col][row] = data[col * 3 + row];
    return r;
  }

  mat33f &
  operator= (const glm::mat3 &m)
  {
    for (int col = 0; col < 3; ++col)
      for (int row = 0; row < 3; ++row)
        data[col * 3 + row] = m[col][row];
    return *this;
  }

  bool
  operator== (const mat33f &other) const
  {
    for (int i = 0; i < 9; ++i)
      if (data[i] != other.data[i])
        return false;
    return true;
  }
  bool
  operator!= (const mat33f &other) const
  {
    return !(*this == other);
  }

  bool
  custom_inspect (const char *label)
  {
    ImGui::PushID (label);

    float const full = ImGui::CalcItemWidth ();
    float const spacing = ImGui::GetStyle ().ItemInnerSpacing.x;
    float const w = (full - spacing * 2.0F) / 3.0F;

    bool changed = false;

    for (int row = 0; row < 3; ++row) {
      ImGui::PushID (row);
      for (int col = 0; col < 3; ++col) {
        ImGui::SetNextItemWidth (w);
        if (col > 0)
          ImGui::SameLine (0.0F, spacing);
        changed |= ImGui::DragFloat ("##v", &data[col * 3 + row], 0.1F);
      }
      ImGui::PopID ();
    }

    ImGui::PopID ();
    return changed;
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<mat33f> ()
        .type (entt::type_hash<mat33f>::value ())
        .func<&mat33f::custom_inspect> ("custom_inspect"_hs);
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    mat33f def{};
    wsl::comp::serialize_field_if_diff (archive, "m00", data[0], def.data[0]);
    wsl::comp::serialize_field_if_diff (archive, "m01", data[1], def.data[1]);
    wsl::comp::serialize_field_if_diff (archive, "m02", data[2], def.data[2]);
    wsl::comp::serialize_field_if_diff (archive, "m10", data[3], def.data[3]);
    wsl::comp::serialize_field_if_diff (archive, "m11", data[4], def.data[4]);
    wsl::comp::serialize_field_if_diff (archive, "m12", data[5], def.data[5]);
    wsl::comp::serialize_field_if_diff (archive, "m20", data[6], def.data[6]);
    wsl::comp::serialize_field_if_diff (archive, "m21", data[7], def.data[7]);
    wsl::comp::serialize_field_if_diff (archive, "m22", data[8], def.data[8]);
  }
};

struct mat44f
{
  /*! Column-major storage: data[col * 4 + row] */
  float data[16]{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

  mat44f () = default;

  mat44f (const glm::mat4 &m)
  {
    for (int col = 0; col < 4; ++col)
      for (int row = 0; row < 4; ++row)
        data[col * 4 + row] = m[col][row];
  }

  operator glm::mat4 () const
  {
    glm::mat4 r;
    for (int col = 0; col < 4; ++col)
      for (int row = 0; row < 4; ++row)
        r[col][row] = data[col * 4 + row];
    return r;
  }

  mat44f &
  operator= (const glm::mat4 &m)
  {
    for (int col = 0; col < 4; ++col)
      for (int row = 0; row < 4; ++row)
        data[col * 4 + row] = m[col][row];
    return *this;
  }

  /*! Column-major indexed access (read-only), compatible with
   * glm::mat4[col][row] */
  float const *
  operator[] (int col) const
  {
    return &data[col * 4];
  }

  /*! Column-major indexed access (mutable), compatible with glm::mat4[col][row]
   */
  float *
  operator[] (int col)
  {
    return &data[col * 4];
  }

  bool
  operator== (const mat44f &other) const
  {
    for (int i = 0; i < 16; ++i)
      if (data[i] != other.data[i])
        return false;
    return true;
  }
  bool
  operator!= (const mat44f &other) const
  {
    return !(*this == other);
  }

  bool
  custom_inspect (const char *label)
  {
    ImGui::PushID (label);

    float const full = ImGui::CalcItemWidth ();
    float const spacing = ImGui::GetStyle ().ItemInnerSpacing.x;
    float const w = (full - spacing * 3.0F) / 4.0F;

    bool changed = false;

    for (int row = 0; row < 4; ++row) {
      ImGui::PushID (row);
      for (int col = 0; col < 4; ++col) {
        ImGui::SetNextItemWidth (w);
        if (col > 0)
          ImGui::SameLine (0.0F, spacing);
        changed |= ImGui::DragFloat ("##v", &data[col * 4 + row], 0.1F);
      }
      ImGui::PopID ();
    }

    ImGui::PopID ();
    return changed;
  }

  static void
  register_meta ()
  {
    using namespace entt::literals;
    entt::meta_factory<mat44f> ()
        .type (entt::type_hash<mat44f>::value ())
        .func<&mat44f::custom_inspect> ("custom_inspect"_hs);
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    mat44f def{};
    wsl::comp::serialize_field_if_diff (archive, "m00", data[0], def.data[0]);
    wsl::comp::serialize_field_if_diff (archive, "m01", data[1], def.data[1]);
    wsl::comp::serialize_field_if_diff (archive, "m02", data[2], def.data[2]);
    wsl::comp::serialize_field_if_diff (archive, "m03", data[3], def.data[3]);
    wsl::comp::serialize_field_if_diff (archive, "m10", data[4], def.data[4]);
    wsl::comp::serialize_field_if_diff (archive, "m11", data[5], def.data[5]);
    wsl::comp::serialize_field_if_diff (archive, "m12", data[6], def.data[6]);
    wsl::comp::serialize_field_if_diff (archive, "m13", data[7], def.data[7]);
    wsl::comp::serialize_field_if_diff (archive, "m20", data[8], def.data[8]);
    wsl::comp::serialize_field_if_diff (archive, "m21", data[9], def.data[9]);
    wsl::comp::serialize_field_if_diff (archive, "m22", data[10], def.data[10]);
    wsl::comp::serialize_field_if_diff (archive, "m23", data[11], def.data[11]);
    wsl::comp::serialize_field_if_diff (archive, "m30", data[12], def.data[12]);
    wsl::comp::serialize_field_if_diff (archive, "m31", data[13], def.data[13]);
    wsl::comp::serialize_field_if_diff (archive, "m32", data[14], def.data[14]);
    wsl::comp::serialize_field_if_diff (archive, "m33", data[15], def.data[15]);
  }
};

} // namespace math

} // namespace wsl
