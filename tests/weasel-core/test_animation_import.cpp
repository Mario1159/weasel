// Asset-pipeline side of M5/M6: the engine reads the animation clip names a
// glTF declares, and converts a model to ozz runtime data on project scan when
// (and only when) that conversion is missing or stale.
//
// See OZZ_ANIMATION_PLAN.md (M2 import, M4 playback, M6 picker).

#include <doctest/doctest.h>

#include "wsl/rsc/animation_importer.hpp"
#include "wsl/rsc/model_loader.hpp"
#include "wsl/rsc/project.hpp"
#include "wsl/rsc/project_loader.hpp"
#include "wsl/serialize/serialize.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifndef WEASEL_SOURCE_DIR
#define WEASEL_SOURCE_DIR "."
#endif

namespace
{

std::string
source_path (const std::string &relative)
{
  return std::string (WEASEL_SOURCE_DIR) + "/" + relative;
}

/** Copies a model into a temp dir so conversion never touches the repo. */
std::filesystem::path
staged_model (const std::string &relative)
{
  const std::filesystem::path dir
      = std::filesystem::temp_directory_path () / "weasel_anim_import_test";
  std::error_code ec;
  std::filesystem::create_directories (dir, ec);

  const std::filesystem::path target
      = dir / std::filesystem::path (relative).filename ();
  std::filesystem::copy_file (source_path (relative), target,
                              std::filesystem::copy_options::overwrite_existing,
                              ec);
  return target;
}

void
remove_staged (const std::filesystem::path &model)
{
  std::error_code ec;
  std::filesystem::remove (model.parent_path (), ec);
}

bool
file_exists (const std::filesystem::path &p)
{
  std::error_code ec;
  return std::filesystem::is_regular_file (p, ec);
}

} // namespace

TEST_CASE ("glTF animation names are read without a full model load")
{
  const std::vector<std::string> names
      = wsl::rsc::animation_importer::read_animation_names (
          source_path ("examples/animation/rsc/models/Fox.glb"));

  REQUIRE (names.size () == 3);
  CHECK (names[0] == "Survey");
  CHECK (names[1] == "Walk");
  CHECK (names[2] == "Run");
}

TEST_CASE ("models without animations report no clips instead of failing")
{
  // A static prop must never cost a conversion, so the read has to be able to
  // answer "nothing to do" without erroring.
  CHECK (wsl::rsc::animation_importer::read_animation_names (
             source_path ("examples/go-demo/rsc/models/black_stone.glb"))
             .empty ());

  // Missing / non-glTF files are also answered with an empty list.
  CHECK (
      wsl::rsc::animation_importer::read_animation_names ("/nope/missing.glb")
          .empty ());
}

TEST_CASE ("model loading exposes the declared clip names")
{
  wsl::rsc::model_loader loader{ nullptr };
  const std::shared_ptr<wsl::rsc::raw::cpu_model> model
      = loader.load_cpu (source_path ("examples/animation/rsc/models/Fox.glb"));
  REQUIRE (model != nullptr);
  REQUIRE_FALSE (model->animation_names.empty ());

  // Names only, and in the order the file declares them.
  CHECK (model->animation_names
         == wsl::rsc::animation_importer::read_animation_names (
             source_path ("examples/animation/rsc/models/Fox.glb")));
}

TEST_CASE ("a model with no clips is recorded once and never converted")
{
  const std::filesystem::path model
      = staged_model ("examples/go-demo/rsc/models/black_stone.glb");
  REQUIRE (file_exists (model));

  const std::filesystem::path sidecar
      = model.parent_path () / (model.stem ().string () + ".import.json");
  std::error_code ec;
  std::filesystem::remove (sidecar, ec);

  // Succeeds purely by recording the negative answer: no subprocess runs, so
  // this holds even where gltf2ozz is unavailable.
  CHECK (wsl::rsc::animation_importer::ensure_imported (model.string ()));
  REQUIRE (file_exists (sidecar));

  // No ozz files may be produced for a model that declares no clips.
  for (const auto &entry :
       std::filesystem::directory_iterator (model.parent_path (), ec)) {
    const std::string name = entry.path ().filename ().string ();
    CHECK (name.find (".ozz") == std::string::npos);
  }

  // Second call is served entirely from the sidecar.
  CHECK (wsl::rsc::animation_importer::ensure_imported (model.string ()));
  remove_staged (model);
}

TEST_CASE ("the cache sidecar round-trips and goes stale with its source")
{
  const std::filesystem::path model
      = staged_model ("examples/animation/rsc/models/black_stone.glb");
  REQUIRE (file_exists (model));

  wsl::rsc::animation_import_manifest manifest;
  manifest.has_animation = true;
  manifest.source_size = 4242;
  manifest.source_mtime = -12345;
  manifest.skeleton = "res://rsc/models/Fox.skel.ozz";
  manifest.clips.emplace_back ("Walk", "res://rsc/models/Fox_Walk.anim.ozz");

  const std::string json = wsl::serialize::json_write (manifest);
  REQUIRE_FALSE (json.empty ());

  wsl::rsc::animation_import_manifest back;
  REQUIRE (wsl::serialize::json_read (json, back));
  CHECK (back.version == manifest.version);
  CHECK (back.has_animation == manifest.has_animation);
  CHECK (back.source_size == manifest.source_size);
  CHECK (back.source_mtime == manifest.source_mtime);
  CHECK (back.skeleton == manifest.skeleton);
  REQUIRE (back.clips.size () == 1);
  CHECK (back.clips[0].first == "Walk");
  CHECK (back.clips[0].second == "res://rsc/models/Fox_Walk.anim.ozz");

  // Recorded values that disagree with the real file are rejected, which is
  // what forces a re-conversion after the source is re-exported.
  const std::filesystem::path sidecar
      = model.parent_path () / (model.stem ().string () + ".import.json");
  std::ofstream file (sidecar);
  file << json;
  file.close ();

  CHECK (
      wsl::rsc::animation_importer::read_manifest (model.string ()).has_value ()
      == false);

  remove_staged (model);
}

TEST_CASE ("a project scan converts rigged models and registers their clips")
{
  if (wsl::rsc::animation_importer::find_tool ().empty ()) {
    // Conversion is best-effort by design, so the engine stays usable where
    // gltf2ozz is not installed. There is nothing to assert in that case.
    return;
  }

  const std::filesystem::path root
      = std::filesystem::temp_directory_path () / "weasel_scan_import_test";
  std::error_code ec;
  std::filesystem::remove_all (root, ec);
  std::filesystem::create_directories (root / "rsc" / "models", ec);
  REQUIRE (std::filesystem::is_directory (root / "rsc" / "models", ec));

  std::filesystem::copy_file (
      source_path ("examples/animation/rsc/models/Fox.glb"),
      root / "rsc" / "models" / "Fox.glb",
      std::filesystem::copy_options::overwrite_existing, ec);
  REQUIRE_FALSE (ec);

  wsl::rsc::project proj;
  proj.name = "scan-import";
  proj.root_path = root.string ();
  proj.models_path = "rsc/models";
  proj.scenes_path = "rsc/scenes";

  const wsl::rsc::project_assets assets
      = wsl::rsc::project_loader::scan_assets (proj);

  // The conversion runs before the .ozz scan, so the clips it produced are
  // registered in the same pass rather than needing a second scan.
  CHECK (assets.skeletons.size () == 1);
  CHECK (assets.animations.size () == 3);

  REQUIRE (file_exists (root / "rsc" / "models" / "Fox.skel.ozz"));
  for (const char *clip : { "Survey", "Walk", "Run" }) {
    CHECK (file_exists (root / "rsc" / "models"
                        / (std::string ("Fox_") + clip + ".anim.ozz")));
  }

  // The sidecar records which clip produced which file, so a clip picker does
  // not have to re-derive the mapping by scanning the directory.
  const std::optional<wsl::rsc::animation_import_manifest> manifest
      = wsl::rsc::animation_importer::read_manifest (
          (root / "rsc" / "models" / "Fox.glb").string ());
  REQUIRE (manifest.has_value ());
  CHECK (manifest->has_animation);
  REQUIRE (manifest->clips.size () == 3);
  CHECK (manifest->clips[0].first == "Run");
  CHECK (manifest->clips[1].first == "Survey");
  CHECK (manifest->clips[2].first == "Walk");

  // The clip picker looks the sidecar up through the *runtime* path form, not
  // the one used above. resource_manager::get_resource_path() returns
  // "res://rsc/models/Fox.glb" for anything under the project root, and
  // animation_system passes that through resource_manager::resolve_path()
  // before calling read_manifest(), which stats the filesystem and takes no
  // scheme. Feeding it the logical path directly silently yields no clips for
  // correctly imported models, so the two forms are pinned together here.
  const std::string logical
      = "res://"
        + std::filesystem::path ("rsc/models/Fox.glb").generic_string ();
  const std::string resolved = root.string () + "/" + logical.substr (6);
  const std::optional<wsl::rsc::animation_import_manifest> via_runtime_path
      = wsl::rsc::animation_importer::read_manifest (resolved);
  REQUIRE (via_runtime_path.has_value ());
  REQUIRE (via_runtime_path->clips.size () == 3);

  // ...and the untranslated logical form is not a filesystem path, which is
  // exactly why the translation step exists.
  CHECK (wsl::rsc::animation_importer::read_manifest (logical).has_value ()
         == false);

  // Second scan is served from the sidecar: same results, no new work.
  const auto first_write = std::filesystem::last_write_time (
      root / "rsc" / "models" / "Fox.skel.ozz", ec);
  const wsl::rsc::project_assets again
      = wsl::rsc::project_loader::scan_assets (proj);
  CHECK (again.animations.size () == 3);
  CHECK (std::filesystem::last_write_time (
             root / "rsc" / "models" / "Fox.skel.ozz", ec)
         == first_write);

  std::filesystem::remove_all (root, ec);
}
