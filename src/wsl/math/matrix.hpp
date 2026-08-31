#pragma once

#if defined(WSL_MODULE_BUILD)
// See vector.hpp: component_meta.hpp is imported by the module unit at top
// level as a header unit; not included/imported here (imports must be
// contiguous at the TU top inside a module purview).
#else
#include "../comp/component_meta.hpp"
#endif

// Inside a C++20 module interface these 3rd-party headers are consumed as
// header units (imported by the module), so they are skipped here. See
// vector.hpp.
#if !defined(WSL_MODULE_BUILD)

// See wsl/math/vector.hpp for why EnTT is skipped inside a module build.
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif

#if !defined(WSL_MODULE_BUILD)
#include <glm/glm.hpp>
#endif

#if !defined(WSL_MODULE_BUILD)
#include <imgui.h>
#endif
#endif

namespace wsl
{

namespace math
{

struct mat33f
{
  /** Column-major storage: data[(col * 3) + row] */
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

};

struct mat44f
{
  /** Column-major storage: data[(col * 4) + row] */
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

  /**
   * Column-major indexed access (read-only), compatible with
   * glm::mat4[col][row]
   */
  float const *
  operator[] (int col) const
  {
    return &m_data[static_cast<ptrdiff_t> (col) * 4];
  }

  /** Column-major indexed access (mutable), compatible with glm::mat4[col][row]
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

};

} // namespace math

} // namespace wsl
