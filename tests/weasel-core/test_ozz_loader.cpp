// M2: ozz resource loading — `.skel.ozz` / `.anim.ozz` produced by the
// gltf2ozz import step must load through the engine loaders, and loaders
// must reject missing files and mismatched archive tags.
// See OZZ_ANIMATION_PLAN.md (M2).

#include <doctest/doctest.h>

#include "wsl/rsc/animation_loader.hpp"
#include "wsl/rsc/skeleton_loader.hpp"

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/base/memory/unique_ptr.h>

#include <filesystem>
#include <string>

#ifndef WEASEL_SOURCE_DIR
#define WEASEL_SOURCE_DIR "."
#endif

namespace
{

std::string
example (const std::string &relative)
{
  return std::string (WEASEL_SOURCE_DIR) + "/examples/animation/rsc/models/"
         + relative;
}

} // namespace

TEST_CASE ("ozz loaders read gltf2ozz outputs")
{
  auto skeleton = wsl::rsc::skeleton_loader::load (example ("Fox.skel.ozz"));
  REQUIRE (skeleton != nullptr);
  CHECK (skeleton->num_joints () > 0);

  // The Fox model ships three clips; all must load and match the skeleton.
  for (const char *clip : { "Survey", "Walk", "Run" }) {
    const std::string path
        = example ("Fox_" + std::string (clip) + ".anim.ozz");
    auto animation = wsl::rsc::animation_loader::load (path);
    REQUIRE (animation != nullptr);
    CHECK (animation->duration () > 0.f);
    CHECK (animation->num_tracks () == skeleton->num_joints ());
    CHECK (std::string (animation->name ()) == clip);
  }
}

TEST_CASE ("ozz loaders reject missing and mismatched archives")
{
  CHECK (wsl::rsc::skeleton_loader::load (example ("missing.skel.ozz"))
         == nullptr);
  // An animation archive must not load as a skeleton (tag check)...
  CHECK (wsl::rsc::skeleton_loader::load (example ("Fox_Walk.anim.ozz"))
         == nullptr);
  // ...and vice versa.
  CHECK (wsl::rsc::animation_loader::load (example ("Fox.skel.ozz"))
         == nullptr);
}

TEST_CASE ("ozz skeleton_loader round-trips an offline-built skeleton")
{
  // Hermetic: build -> save -> load, independent of checked-in assets.
  ozz::animation::offline::RawSkeleton raw;
  raw.roots.resize (1);
  raw.roots[0].name = "root";
  raw.roots[0].children.resize (1);
  raw.roots[0].children[0].name = "child";
  raw.roots[0].children[0].transform.translation
      = ozz::math::Float3 (0.f, 2.f, 0.f);

  ozz::animation::offline::SkeletonBuilder builder;
  ozz::unique_ptr<ozz::animation::Skeleton> built = builder (raw);
  REQUIRE (built != nullptr);

  const std::filesystem::path tmp
      = std::filesystem::temp_directory_path ()
        / "weasel_test_roundtrip.skel.ozz";
  {
    ozz::io::File file (tmp.string ().c_str (), "wb");
    REQUIRE (file.opened ());
    ozz::io::OArchive archive (&file);
    archive << *built;
  }

  auto loaded = wsl::rsc::skeleton_loader::load (tmp.string ());
  REQUIRE (loaded != nullptr);
  CHECK (loaded->num_joints () == built->num_joints ());
  CHECK (std::string (loaded->joint_names ()[1]) == "child");

  std::error_code ec;
  std::filesystem::remove (tmp, ec);
}
