#include "skeleton_loader.hpp"

#include "wsl/log/log.hpp"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

ozz::unique_ptr<ozz::animation::Skeleton>
wsl::rsc::skeleton_loader::load (const std::string &path)
{
  ozz::io::File file (path.c_str (), "rb");
  if (!file.opened ()) {
    wsl::log::rsc ()->error ("Skeleton file not found: {}", path);
    return nullptr;
  }

  ozz::io::IArchive archive (&file);
  // Guard against loading an animation (or any other archive) as a
  // skeleton. TestTag rewinds the stream, so the read below still starts
  // at the tag.
  if (!archive.TestTag<ozz::animation::Skeleton> ()) {
    wsl::log::rsc ()->error ("Not an ozz skeleton archive: {}", path);
    return nullptr;
  }

  ozz::unique_ptr<ozz::animation::Skeleton> skeleton
      = ozz::make_unique<ozz::animation::Skeleton> ();
  archive >> *skeleton;
  return skeleton;
}
