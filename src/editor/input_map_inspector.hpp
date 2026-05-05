#pragma once

#include <entt/entt.hpp>
#include <imgui.h>

#include "wsl/input.hpp"

namespace wsl::comp::singl { class runtime_context; }

namespace editor
{

class input_map_inspector
{
public:
  void draw (entt::registry &registry, wsl::comp::singl::runtime_context *runtime_ctx);

private:
  int m_remove_index = -1;
};

} // namespace editor
