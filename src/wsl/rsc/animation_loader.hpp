#pragma once

#ifndef IN_MODULE_INTERFACE
#include <ozz/animation/runtime/animation.h>
#endif
#ifndef IN_MODULE_INTERFACE
#include <ozz/base/memory/unique_ptr.h>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif

namespace wsl
{

namespace rsc
{

/**
 * Loads runtime animation clip data (`.anim.ozz`) produced by the
 * gltf2ozz import step (see OZZ_ANIMATION_PLAN.md, M2).
 */
class animation_loader
{
public:
  /**
   * Loads a runtime animation clip from disk.
   * :param path: Filesystem path of the `.anim.ozz` file.
   * :return: The animation, or nullptr if the file is missing, is not an
   *          ozz animation archive, or fails to deserialize.
   */
  [[nodiscard]] static ozz::unique_ptr<ozz::animation::Animation>
  load (const std::string &path);
};

} // namespace rsc

} // namespace wsl
