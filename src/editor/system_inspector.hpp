#pragma once

#include "ecs_inspector_utils.hpp"

namespace wsl::comp::singl { class runtime_context; class editor_context; }
namespace wsl::sys { class ecs_system; }

namespace editor
{

class system_inspector
{
public:
  system_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                    wsl::comp::singl::editor_context *editor_ctx,
                    ecs_selection *selection);

  void draw ();

  void set_visible (bool value);
  bool is_visible () const;

private:
  void draw_system_controls (wsl::sys::ecs_system &system);
  void draw_add_system_ui ();

  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  ecs_selection *m_selection;
  bool m_visible = true;
};

} // namespace editor
