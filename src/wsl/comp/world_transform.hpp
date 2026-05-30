#pragma once

#include <entt/entt.hpp>
#include "../math/matrix.hpp"

#include "component_meta.hpp"

#include <cereal/cereal.hpp>

namespace wsl::comp::singl
{
struct runtime_context;
}

namespace wsl
{

namespace comp
{

struct world_transform : world_component
{
  math::mat44f value{};

  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);

  static void register_meta ();

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    archive (cereal::make_nvp ("matrix", value));
  }
};

} // namespace comp

} // namespace wsl
