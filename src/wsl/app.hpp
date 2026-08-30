#pragma once

#include "comp/singl/runtime_context.hpp"
#if !defined(WSL_MODULE_BUILD)
#include "wsl/event.hpp"
#endif

#if !defined(WSL_MODULE_BUILD)
#include <SDL3/SDL.h>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <SDL3/SDL_gpu.h>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <memory>
#endif

/**
 * Game framework library for the Weasel engine.
 */
namespace wsl
{

class app
{
public:
  app (const std::string &name, int width, int height,
       const std::string &engine_res_path = ".");
  virtual ~app ();

  auto run () -> int;
  void set_project_path (const std::string &path);
  void set_engine_resource_path (const std::string &path);

protected:
  std::unique_ptr<wsl::comp::singl::runtime_context> m_runtime_context;

  virtual void on_init () = 0;
  virtual void on_event (const wsl::engine_event & /*unused*/) {};
  virtual void on_update (double /*unused*/) {};
  virtual void on_render ();

  template <wsl::comp::has_register_meta... components>
  void
  register_components ()
  {
    (components::register_meta (), ...);
  }

private:
};

} // namespace wsl
