#pragma once

#include "../comp/component_meta.hpp"

#include <cereal/cereal.hpp>

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <imgui.h>

namespace wsl
{

namespace math
{

struct mat33f
{
  /*! Column-major storage: data[(col * 3) + row] */
private:
  float m_data[9]{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };

public:
  mat33f () = default;

  mat33f (const glm::mat3 &mat)
  {
    for (int col = 0; col < 3; ++col) {
      for (int row = 0; row < 3; ++row) {
        m_data[static_cast<ptrdiff_t> ((col * 3) + row)] = mat[col][row];
      }
    }
  }

  float const *
  data () const
  {
    return m_data;
  }
  float *
  data ()
  {
    return m_data;
  }

  operator glm::mat3 () const
  {
    glm::mat3 result;
    for (int col = 0; col < 3; ++col) {
      for (int row = 0; row < 3; ++row) {
        result[col][row] = m_data[static_cast<ptrdiff_t> ((col * 3) + row)];
      }
    }
    return result;
  }

  mat33f &
  operator= (const glm::mat3 &mat)
  {
    for (int col = 0; col < 3; ++col) {
      for (int row = 0; row < 3; ++row) {
        m_data[static_cast<ptrdiff_t> ((col * 3) + row)] = mat[col][row];
      }
    }
    return *this;
  }

  bool
  operator== (const mat33f &other) const
  {
    for (int i = 0; i < 9; ++i) {
      if (m_data[i] != other.m_data[i]) {
        return false;
      }
    }
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
    float const width = (full - (spacing * 2.0F)) / 3.0F;

    bool changed = false;

    for (int row = 0; row < 3; ++row) {
      ImGui::PushID (row);
      for (int col = 0; col < 3; ++col) {
        ImGui::SetNextItemWidth (width);
        if (col > 0) {
          ImGui::SameLine (0.0F, spacing);
        }
        changed |= ImGui::DragFloat (
            "##v", &m_data[static_cast<ptrdiff_t> ((col * 3) + row)], 0.1F);
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
    auto &&factory = entt::meta_factory<mat33f> ().type (
        entt::type_hash<mat33f>::value ());
    (factory.func<&mat33f::custom_inspect>)("custom_inspect"_hs);
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    mat33f def{};
    wsl::comp::serialize_field_if_diff (archive, "m00", m_data[0],
                                        def.m_data[0]);
    wsl::comp::serialize_field_if_diff (archive, "m01", m_data[1],
                                        def.m_data[1]);
    wsl::comp::serialize_field_if_diff (archive, "m02", m_data[2],
                                        def.m_data[2]);
    wsl::comp::serialize_field_if_diff (archive, "m10", m_data[3],
                                        def.m_data[3]);
    wsl::comp::serialize_field_if_diff (archive, "m11", m_data[4],
                                        def.m_data[4]);
    wsl::comp::serialize_field_if_diff (archive, "m12", m_data[5],
                                        def.m_data[5]);
    wsl::comp::serialize_field_if_diff (archive, "m20", m_data[6],
                                        def.m_data[6]);
    wsl::comp::serialize_field_if_diff (archive, "m21", m_data[7],
                                        def.m_data[7]);
    wsl::comp::serialize_field_if_diff (archive, "m22", m_data[8],
                                        def.m_data[8]);
  }
};

struct mat44f
{
  /*! Column-major storage: data[(col * 4) + row] */
private:
  float m_data[16]{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

public:
  mat44f () = default;

  mat44f (const glm::mat4 &mat)
  {
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        m_data[static_cast<ptrdiff_t> ((col * 4) + row)] = mat[col][row];
      }
    }
  }

  float const *
  data () const
  {
    return m_data;
  }
  float *
  data ()
  {
    return m_data;
  }

  operator glm::mat4 () const
  {
    glm::mat4 result;
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        result[col][row] = m_data[static_cast<ptrdiff_t> ((col * 4) + row)];
      }
    }
    return result;
  }

  mat44f &
  operator= (const glm::mat4 &mat)
  {
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        m_data[static_cast<ptrdiff_t> ((col * 4) + row)] = mat[col][row];
      }
    }
    return *this;
  }

  /*! Column-major indexed access (read-only), compatible with
   * glm::mat4[col][row] */
  float const *
  operator[] (int col) const
  {
    return &m_data[static_cast<ptrdiff_t> (col) * 4];
  }

  /*! Column-major indexed access (mutable), compatible with glm::mat4[col][row]
   */
  float *
  operator[] (int col)
  {
    return &m_data[static_cast<ptrdiff_t> (col) * 4];
  }

  bool
  operator== (const mat44f &other) const
  {
    for (int i = 0; i < 16; ++i) {
      if (m_data[i] != other.m_data[i]) {
        return false;
      }
    }
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
    float const width = (full - (spacing * 3.0F)) / 4.0F;

    bool changed = false;

    for (int row = 0; row < 4; ++row) {
      ImGui::PushID (row);
      for (int col = 0; col < 4; ++col) {
        ImGui::SetNextItemWidth (width);
        if (col > 0) {
          ImGui::SameLine (0.0F, spacing);
        }
        changed |= ImGui::DragFloat (
            "##v", &m_data[static_cast<ptrdiff_t> ((col * 4) + row)], 0.1F);
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
    auto &&factory = entt::meta_factory<mat44f> ().type (
        entt::type_hash<mat44f>::value ());
    (factory.func<&mat44f::custom_inspect>)("custom_inspect"_hs);
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    mat44f def{};
    wsl::comp::serialize_field_if_diff (archive, "m00", m_data[0],
                                        def.m_data[0]);
    wsl::comp::serialize_field_if_diff (archive, "m01", m_data[1],
                                        def.m_data[1]);
    wsl::comp::serialize_field_if_diff (archive, "m02", m_data[2],
                                        def.m_data[2]);
    wsl::comp::serialize_field_if_diff (archive, "m03", m_data[3],
                                        def.m_data[3]);
    wsl::comp::serialize_field_if_diff (archive, "m10", m_data[4],
                                        def.m_data[4]);
    wsl::comp::serialize_field_if_diff (archive, "m11", m_data[5],
                                        def.m_data[5]);
    wsl::comp::serialize_field_if_diff (archive, "m12", m_data[6],
                                        def.m_data[6]);
    wsl::comp::serialize_field_if_diff (archive, "m13", m_data[7],
                                        def.m_data[7]);
    wsl::comp::serialize_field_if_diff (archive, "m20", m_data[8],
                                        def.m_data[8]);
    wsl::comp::serialize_field_if_diff (archive, "m21", m_data[9],
                                        def.m_data[9]);
    wsl::comp::serialize_field_if_diff (archive, "m22", m_data[10],
                                        def.m_data[10]);
    wsl::comp::serialize_field_if_diff (archive, "m23", m_data[11],
                                        def.m_data[11]);
    wsl::comp::serialize_field_if_diff (archive, "m30", m_data[12],
                                        def.m_data[12]);
    wsl::comp::serialize_field_if_diff (archive, "m31", m_data[13],
                                        def.m_data[13]);
    wsl::comp::serialize_field_if_diff (archive, "m32", m_data[14],
                                        def.m_data[14]);
    wsl::comp::serialize_field_if_diff (archive, "m33", m_data[15],
                                        def.m_data[15]);
  }
};

} // namespace math

} // namespace wsl
