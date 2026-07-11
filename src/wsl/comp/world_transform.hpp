#pragma once

#include <entt/entt.hpp>
#include "../math/matrix.hpp"

#include "component_meta.hpp"

#include <cereal/cereal.hpp>

namespace wsl::comp::singl
{
class runtime_context;
}

namespace wsl
{

namespace comp
{

struct world_transform : world_component
{
private:
  math::mat44f m_value;

public:
  math::mat44f const &
  value () const
  {
    return m_value;
  }
  math::mat44f &
  value ()
  {
    return m_value;
  }

  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    world_transform def{};
    serialize_field_if_diff (archive, "matrix", m_value, def.m_value);
  }
};

} // namespace comp

} // namespace wsl
