#pragma once

#ifndef IN_MODULE_INTERFACE
#include <ozz/animation/runtime/skeleton.h>
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
 * Loads runtime skeleton data (`.skel.ozz`) produced by the gltf2ozz
 * import step (see OZZ_ANIMATION_PLAN.md, M2).
 */
class skeleton_loader
{
public:
  /**
   * Loads a runtime skeleton from disk.
   * :param path: Filesystem path of the `.skel.ozz` file.
   * :return: The skeleton, or nullptr if the file is missing, is not an ozz
   *          skeleton archive, or fails to deserialize.
   */
  [[nodiscard]] static ozz::unique_ptr<ozz::animation::Skeleton>
  load (const std::string &path);
};

} // namespace rsc

} // namespace wsl
