#include "animation_loader.hpp"

#include "wsl/log/log.hpp"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

ozz::unique_ptr<ozz::animation::Animation>
wsl::rsc::animation_loader::load (const std::string &path)
{
  ozz::io::File file (path.c_str (), "rb");
  if (!file.opened ()) {
    wsl::log::rsc ()->error ("Animation file not found: {}", path);
    return nullptr;
  }

  ozz::io::IArchive archive (&file);
  // Guard against loading a skeleton (or any other archive) as an
  // animation. TestTag rewinds the stream, so the read below still starts
  // at the tag.
  if (!archive.TestTag<ozz::animation::Animation> ()) {
    wsl::log::rsc ()->error ("Not an ozz animation archive: {}", path);
    return nullptr;
  }

  ozz::unique_ptr<ozz::animation::Animation> animation
      = ozz::make_unique<ozz::animation::Animation> ();
  archive >> *animation;
  return animation;
}
