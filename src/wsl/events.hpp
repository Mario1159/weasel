#pragma once

#include "wsl/rsc/scene.hpp"

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
