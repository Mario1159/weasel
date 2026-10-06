#pragma once

#ifndef IN_MODULE_INTERFACE
#include <cstdint>
#endif
#ifndef IN_MODULE_INTERFACE
#include <optional>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <utility>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl
{

namespace rsc
{

/** Outcome of a gltf2ozz import run. */
struct animation_import_result
{
  /** True when gltf2ozz exited successfully and produced outputs. */
  bool ok = false;
  /** Failure description when ok is false. */
  std::string error;
  /** Absolute paths of the produced `.skel.ozz` / `.anim.ozz` files. */
  std::vector<std::string> outputs;
};

/**
 * Cache sidecar written next to the source model after a successful (or
 * deliberately skipped) conversion.
 *
 * Its presence is what makes the scan-time import cheap: the source's size and
 * modification time are compared against the recorded values, so an unchanged
 * model is never re-parsed and never spawns a subprocess. `has_animation` is
 * recorded even when the model has no clips, so a static prop is only ever
 * checked once.
 */
struct animation_import_manifest
{
  /** Schema version, bumped when the sidecar layout changes. */
  int version = 1;
  /** True when the source declares at least one animation clip. */
  bool has_animation = false;
  /** Source file size in bytes at conversion time. */
  std::uintmax_t source_size = 0;
  /** Source file modification time (ns since epoch) at conversion time. */
  std::int64_t source_mtime = 0;
  /** Resolved path of the produced runtime skeleton, when any. */
  std::string skeleton;
  /** Clip name to resolved `.anim.ozz` path. */
  std::vector<std::pair<std::string, std::string>> clips;
};

/**
 * Converts rigged glTF assets into ozz runtime data using the gltf2ozz
 * tool shipped with the ozz-animation package (OZZ_ANIMATION_PLAN.md, M2).
 *
 * Outputs are written next to the source file:
 * - `<base>.skel.ozz`        — runtime skeleton
 * - `<base>_<clip>.anim.ozz` — one runtime animation per clip
 * - `<base>.import.json`     — cache sidecar (see animation_import_manifest)
 */
class animation_importer
{
public:
  /**
   * Locates the gltf2ozz executable.
   * Search order: WEASEL_GLTF2OZZ environment variable, the xmake package
   * cache (newest ozz-animation install), then a bare `gltf2ozz` name that
   * the spawning shell resolves through PATH.
   * :return: Tool path or name to pass to the shell; empty only when the
   *          environment override is set but unusable.
   */
  [[nodiscard]] static std::string find_tool ();

  /**
   * Imports skeleton + animation clips from a glTF file.
   * :param gltf_path: Path to a .gltf/.glb file containing skins/animations.
   * :return: Result with produced output paths, or a failure description.
   */
  [[nodiscard]] static animation_import_result
  import (const std::string &gltf_path);

  /**
   * Reads only the animation clip names declared by a glTF file.
   *
   * Uses a minimal fastgltf parse that skips buffers, images and meshes, so
   * this is far cheaper than a full model load. An unreadable or non-glTF file
   * yields an empty list rather than an error: callers use this to decide
   * whether a conversion is worth attempting, not as validation.
   */
  [[nodiscard]] static std::vector<std::string>
  read_animation_names (const std::string &gltf_path);

  /**
   * Ensures ozz runtime data exists for \p gltf_path, converting only when
   * the cache sidecar is missing or stale.
   *
   * Best-effort by design: a model that cannot be converted still loads and
   * renders, it just has no clips. Callers on a project scan path can invoke
   * this for every model without special-casing failures.
   *
   * :param gltf_path: Source .gltf/.glb path.
   * :return: True when ozz data is present and current afterwards.
   */
  [[nodiscard]] static bool ensure_imported (const std::string &gltf_path);

  /**
   * Reads the cache sidecar for \p gltf_path if one exists and is current.
   * :return: The manifest, or std::nullopt when absent, stale or unreadable.
   */
  [[nodiscard]] static std::optional<animation_import_manifest>
  read_manifest (const std::string &gltf_path);
};

} // namespace rsc

} // namespace wsl
