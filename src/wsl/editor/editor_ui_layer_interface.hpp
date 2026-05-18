#pragma once

#include <SDL3/SDL.h>
#include <entt/entity/fwd.hpp>
#include <functional>
#include <string>

namespace wsl::comp::singl
{
class runtime_context;
class editor_context;
}

namespace wsl::editor
{

class editor_ui_layer_interface
{
public:
  virtual ~editor_ui_layer_interface () = default;

  virtual void initialize () = 0;
  virtual void
  set_console_command_handler (std::function<std::string (const std::string &)> handler) = 0;
  virtual void handle_event (const SDL_Event &e) = 0;
  virtual void build_draw_data (entt::registry &reg) = 0;
  virtual void prepare_gpu_resources () = 0;
  virtual void record_draw_commands () = 0;
};

} // namespace wsl::editor
