#pragma once

#include "wsl/comp/singl/runtime_context_fwd.hpp"
#include <entt/entt.hpp>
#include <imgui.h>

#include "wsl/input.hpp"

namespace editor
{

class input_map_inspector
{
public:
  void draw (entt::registry &registry,
             wsl::comp::singl::runtime_context *runtime_ctx);

private:
  int m_remove_index = -1;
};

} // namespace editor
