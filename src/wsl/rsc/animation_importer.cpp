#include "animation_importer.hpp"

#include "wsl/log/log.hpp"
#include "wsl/serialize/serialize.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include <fastgltf/core.hpp>
#include <fmt/format.h>

namespace wsl
{

namespace fs = std::filesystem;

namespace rsc
{

namespace
{

/** Sidecar path used to remember a model's conversion state. */
fs::path
manifest_path (const std::string &gltf_path)
{
  const fs::path source (gltf_path);
  return source.parent_path () / (source.stem ().string () + ".import.json");
}

/** Records the conversion result so later scans can skip the work. */
bool
write_manifest (const std::string &gltf_path,
                const animation_import_manifest &manifest)
{
  const fs::path sidecar = manifest_path (gltf_path);
  std::ofstream file (sidecar);
  if (!file) {
    wsl::log::rsc ()->warn ("Could not write animation cache {}",
                            sidecar.string ());
    return false;
  }

  std::string error;
  const std::string json = wsl::serialize::json_write (manifest, &error);
  if (json.empty ()) {
    wsl::log::rsc ()->warn ("Could not serialize animation cache: {}", error);
    return false;
  }
  file << json;
  return true;
}

/**
 * Emits a conversion warning at most once per process.
 *
 * A project scan retries every model, so a missing tool or a broken asset
 * would otherwise repeat the same message for every model on every load.
 */
template <typename... Args>
void
warn_conversion_once (fmt::format_string<Args...> format, Args &&...args)
{
  static bool already_warned = false;
  if (already_warned) {
    return;
  }
  already_warned = true;
  wsl::log::rsc ()->warn (format, std::forward<Args> (args)...);
}

} // namespace

namespace
{

#if defined(_WIN32)
constexpr const char *tool_filename = "gltf2ozz.exe";
#else
constexpr const char *tool_filename = "gltf2ozz";
#endif

/** Escapes a value for embedding inside a JSON string literal. */
std::string
json_escape (const std::string &value)
{
  std::string out;
  out.reserve (value.size ());
  for (const char c : value) {
    if (c == '\\' || c == '"') {
      out.push_back ('\\');
    }
    out.push_back (c);
  }
  return out;
}

/** Normalizes a path to forward slashes so it embeds cleanly in JSON. */
std::string
json_path (const fs::path &path)
{
  return json_escape (path.lexically_normal ().generic_string ());
}

/** Quotes a value for the platform shell (POSIX sh / cmd.exe). */
std::string
shell_quote (const std::string &value)
{
#if defined(_WIN32)
  std::string out = "\"";
  for (const char c : value) {
    if (c == '"') {
      out += "\\\"";
    } else {
      out.push_back (c);
    }
  }
  out += "\"";
  return out;
#else
  // POSIX single-quote escaping: close, escaped quote, reopen.
  std::string out = "'";
  for (const char c : value) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back (c);
    }
  }
  out += "'";
  return out;
#endif
}

} // namespace

std::string
animation_importer::find_tool ()
{
  // 1. Explicit override wins; returned verbatim so a bad path surfaces
  //    as a loud, obvious failure instead of a silent fallback.
  if (const char *env = std::getenv ("WEASEL_GLTF2OZZ"); env != nullptr) {
    return env;
  }

  // 2. Newest xmake package install (mirrors the daslib/slangc lookups).
  const char *home = std::getenv ("HOME");
  if (home != nullptr) {
    const fs::path pkg_root
        = fs::path (home) / ".xmake" / "packages" / "o" / "ozz-animation";
    std::error_code ec;
    if (fs::is_directory (pkg_root, ec)) {
      fs::path best;
      fs::file_time_type best_time{};
      for (fs::recursive_directory_iterator it (pkg_root, ec), last;
           it != last && !ec; it.increment (ec)) {
        if (it->path ().filename () != tool_filename) {
          continue;
        }
        std::error_code file_ec;
        if (!fs::is_regular_file (it->path (), file_ec)) {
          continue;
        }
        const fs::file_time_type written
            = fs::last_write_time (it->path (), file_ec);
        if (file_ec) {
          continue;
        }
        if (best.empty () || written > best_time) {
          best = it->path ();
          best_time = written;
        }
      }
      if (!best.empty ()) {
        return best.string ();
      }
    }
  }

  // 3. Bare name: the spawning shell resolves it through PATH.
  return tool_filename;
}

animation_import_result
animation_importer::import (const std::string &gltf_path)
{
  animation_import_result result;

  const fs::path source (gltf_path);
  std::error_code ec;
  if (!fs::is_regular_file (source, ec)) {
    result.error = "glTF file not found: " + gltf_path;
    return result;
  }

  const std::string tool = find_tool ();
  if (tool.empty ()) {
    result.error = "gltf2ozz not found; set WEASEL_GLTF2OZZ to the tool path";
    return result;
  }

  const fs::path dir = source.parent_path ();
  const std::string base = source.stem ().string ();
  const fs::path skeleton_out = dir / (base + ".skel.ozz");
  // gltf2ozz replaces '*' in the filename with each clip's name.
  const fs::path animations_out = dir / (base + "_*.anim.ozz");

  // Config goes through a temp file: keeps shell/JSON escaping out of the
  // command line entirely.
  const fs::path config_path
      = fs::temp_directory_path (ec)
        / ("weasel_gltf2ozz_"
           + std::to_string (
               std::chrono::steady_clock::now ().time_since_epoch ().count ())
           + ".json");
  {
    std::ofstream out (config_path);
    if (!out) {
      result.error = "Failed to write temp config: " + config_path.string ();
      return result;
    }
    out << "{\"skeleton\":{\"filename\":\"" << json_path (skeleton_out)
        << "\"},\"animations\":[{\"clip\":\"*\",\"filename\":\""
        << json_path (animations_out) << "\"}]}";
  }

  const fs::file_time_type started = fs::file_time_type::clock::now ();

  const std::string command
      = shell_quote (tool) + " --file=" + shell_quote (source.string ())
        + " --config_file=" + shell_quote (config_path.string ());

  // Capture output: the pipe buffer would deadlock gltf2ozz if nobody
  // drained it, and re-logging it keeps failures diagnosable.
  std::string output;
  FILE *pipe = popen (command.c_str (), "r");
  if (pipe == nullptr) {
    fs::remove (config_path, ec);
    result.error = "Failed to spawn gltf2ozz";
    return result;
  }
  char buffer[512];
  while (fgets (buffer, sizeof (buffer), pipe) != nullptr) {
    output += buffer;
  }
  // 0 means success for both POSIX wait status and cmd.exe exit codes.
  const int status = pclose (pipe);
  fs::remove (config_path, ec);

  if (status != 0) {
    result.error = "gltf2ozz failed (status " + std::to_string (status) + "):\n"
                   + output;
    return result;
  }

  // Collect the files this run produced (guards against reporting stale
  // outputs left over from an earlier import).
  const std::string skel_name = base + ".skel.ozz";
  const std::string anim_prefix = base + "_";
  const std::string anim_suffix = ".anim.ozz";
  std::error_code list_ec;
  for (const fs::directory_entry &entry :
       fs::directory_iterator (dir, list_ec)) {
    if (!entry.is_regular_file ()) {
      continue;
    }
    const std::string name = entry.path ().filename ().string ();
    const bool is_skeleton = name == skel_name;
    const bool is_animation
        = name.starts_with (anim_prefix) && name.ends_with (anim_suffix);
    if (!is_skeleton && !is_animation) {
      continue;
    }
    std::error_code time_ec;
    if (fs::last_write_time (entry.path (), time_ec) < started || time_ec) {
      continue;
    }
    result.outputs.push_back (entry.path ().string ());
  }
  std::sort (result.outputs.begin (), result.outputs.end ());

  if (result.outputs.empty ()) {
    result.error = "gltf2ozz reported success but produced no outputs";
    return result;
  }

  result.ok = true;
  wsl::log::rsc ()->info ("Imported {} animation asset(s) from {}",
                          result.outputs.size (), gltf_path);
  return result;
}

std::vector<std::string>
animation_importer::read_animation_names (const std::string &gltf_path)
{
  std::vector<std::string> names;

  const fs::path source (gltf_path);
  std::error_code ec;
  if (!fs::is_regular_file (source, ec)) {
    return names;
  }

  // Only the animation records are needed, so skip buffers, images and
  // meshes. This is a header-level parse of the JSON/GLB container, not a
  // data load, which keeps it cheap enough to run during a project scan.
  fastgltf::Parser parser;
  auto file = fastgltf::MappedGltfFile::FromPath (source);
  if (!file) {
    return names;
  }

  auto asset = parser.loadGltf (file.get (), source.parent_path (),
                                fastgltf::Options::None,
                                fastgltf::Category::Animations);
  if (!asset) {
    return names;
  }

  names.reserve (asset->animations.size ());
  for (const fastgltf::Animation &animation : asset->animations) {
    // fastgltf names are std::pmr::string; convert to the engine's string.
    names.emplace_back (animation.name.data (), animation.name.size ());
  }
  return names;
}

std::optional<animation_import_manifest>
animation_importer::read_manifest (const std::string &gltf_path)
{
  const fs::path source (gltf_path);
  std::error_code ec;
  if (!fs::is_regular_file (source, ec)) {
    return std::nullopt;
  }

  const fs::path sidecar = manifest_path (gltf_path);
  if (!fs::is_regular_file (sidecar, ec)) {
    return std::nullopt;
  }

  std::ifstream file (sidecar);
  if (!file) {
    return std::nullopt;
  }
  std::stringstream ss;
  ss << file.rdbuf ();

  animation_import_manifest manifest;
  if (!wsl::serialize::json_read (ss.str (), manifest)) {
    return std::nullopt;
  }

  // Freshness gate: anything that disagrees with the source is re-converted.
  if (manifest.source_size != fs::file_size (source, ec) || ec) {
    return std::nullopt;
  }
  const auto written = fs::last_write_time (source, ec);
  if (ec) {
    return std::nullopt;
  }
  if (manifest.source_mtime
      != std::chrono::duration_cast<std::chrono::nanoseconds> (
             written.time_since_epoch ())
             .count ()) {
    return std::nullopt;
  }

  return manifest;
}

bool
animation_importer::ensure_imported (const std::string &gltf_path)
{
  // Fast path: unchanged model, nothing to do and no subprocess.
  if (read_manifest (gltf_path).has_value ()) {
    return true;
  }

  // Only convert models that actually declare clips. A static prop must never
  // cost a subprocess, and recording that answer in the sidecar means it is
  // only ever computed once per source revision.
  const std::vector<std::string> names = read_animation_names (gltf_path);
  if (names.empty ()) {
    wsl::log::rsc ()->trace (
        "No animations in {}; recording and skipping conversion", gltf_path);
    return write_manifest (gltf_path, animation_import_manifest{});
  }

  if (find_tool ().empty ()) {
    warn_conversion_once (
        "gltf2ozz is unavailable (set WEASEL_GLTF2OZZ); {} keeps no clips",
        gltf_path);
    return false;
  }

  wsl::log::rsc ()->info ("Converting {} animation clip(s) from {}",
                          names.size (), gltf_path);
  const animation_import_result result = import (gltf_path);
  if (!result.ok) {
    // Best-effort: the model still loads and renders, it simply has no clips.
    warn_conversion_once ("gltf2ozz conversion of {} failed: {}", gltf_path,
                          result.error);
    return false;
  }

  animation_import_manifest manifest;
  manifest.has_animation = true;

  const fs::path source (gltf_path);
  std::error_code ec;
  manifest.source_size = fs::file_size (source, ec);
  const auto written = fs::last_write_time (source, ec);
  manifest.source_mtime = std::chrono::duration_cast<std::chrono::nanoseconds> (
                              written.time_since_epoch ())
                              .count ();

  // Map produced files back to clip names using gltf2ozz's `<base>_<clip>`
  // naming so the sidecar can drive a picker without a directory scan.
  const std::string base = source.stem ().string ();
  const std::string anim_prefix = base + "_";
  const std::string anim_suffix = ".anim.ozz";
  for (const std::string &output : result.outputs) {
    const fs::path out_path (output);
    const std::string name = out_path.filename ().string ();
    if (name == base + ".skel.ozz") {
      manifest.skeleton = output;
      continue;
    }
    if (!name.starts_with (anim_prefix) || !name.ends_with (anim_suffix)) {
      continue;
    }
    manifest.clips.emplace_back (
        name.substr (anim_prefix.size (),
                     name.size () - anim_prefix.size () - anim_suffix.size ()),
        output);
  }
  std::sort (manifest.clips.begin (), manifest.clips.end ());

  return write_manifest (gltf_path, manifest);
}

} // namespace rsc

} // namespace wsl
