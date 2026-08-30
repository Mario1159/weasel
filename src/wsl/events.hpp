#pragma once

#if !defined(WSL_MODULE_BUILD)
#include "wsl/rsc/scene.hpp"
#endif

namespace wsl
{

/**
 * Editor-wide events and messages.
 */
namespace event
{

struct scene_changed
{
  wsl::rsc::scene *old_scene;
  wsl::rsc::scene *new_scene;
};

}

} // namespace wsl
