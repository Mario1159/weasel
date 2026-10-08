#include "scene_loader.hpp"

#include "wsl/comp/singl/editor_context.hpp"
#include "scene_snapshot_serializer.hpp"
#include "wsl/log/log.hpp"

#include <array>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace wsl
{

namespace
{

/**
 * True when \p path holds a JSON document.
 *
 * Scenes are single JSON documents that carry the `.wscn` extension
 * (`rsc::scene_file::extension`), so the extension alone cannot decide which
 * reader to use: msgpack snapshots written before the formats were unified
 * live behind the very same `.wscn` name. The first non-space byte settles
 * it -- a scene document opens with `{`, msgpack never does.
 */
bool
holds_json_document (const std::string &path)
{
  std::ifstream file (path, std::ios::binary);
  if (!file) {
    return false;
  }

  std::array<char, 16> prefix{};
  file.read (prefix.data (), static_cast<std::streamsize> (prefix.size ()));

  const std::streamsize got = file.gcount ();
  for (std::streamsize i = 0; i < got; ++i) {
    const auto c
        = static_cast<unsigned char> (prefix[static_cast<std::size_t> (i)]);
    if (std::isspace (c)) {
      continue;
    }
    return c == '{' || c == '[';
  }

  return false;
}

} // namespace

std::shared_ptr<rsc::scene>
rsc::scene_loader::operator() (comp::singl::runtime_context *runtime_ctx,
                               comp::singl::editor_context *editor_ctx,
                               const std::string &path) const
{
  try {
    wsl::log::rsc ()->trace ("Loading scene: {}", path);
    std::filesystem::path const p (path);

    std::shared_ptr<scene> scn = std::make_shared<scene> (
        runtime_ctx, editor_ctx, p.stem ().string ());

    wsl::log::rsc ()->trace ("Creating serializer for scene: {}",
                             scn->get_name ());
    rsc::io::scene_snapshot_serializer serializer{ runtime_ctx, *scn };

    // The payload decides, not the suffix: reading a JSON scene with the
    // msgpack reader fails silently and yields an empty scene -- a project
    // opened that way would show no camera, light or sample geometry.
    const bool is_json = holds_json_document (path);
    wsl::log::rsc ()->trace ("Loading as {}", is_json ? "JSON" : "binary");

    const bool loaded
        = is_json ? serializer.load_json (path) : serializer.load_binary (path);
    if (!loaded) {
      wsl::log::rsc ()->error ("Failed to load scene: {}", path);
      return {};
    }

    wsl::log::rsc ()->debug ("Loaded scene: {}", path);
    return scn;
  } catch (const std::exception &e) {
    wsl::log::rsc ()->error ("Scene loader exception for '{}': {}", path,
                             e.what ());
  } catch (...) {
    wsl::log::rsc ()->error (
        "Scene loader exception for '{}': unknown exception", path);
  }

  return {};
}

std::shared_ptr<rsc::scene>
rsc::scene_loader::operator() (scene &&ready_scene) const
{
  return std::make_shared<scene> (std::move (ready_scene));
}

bool
rsc::scene_loader::save (comp::singl::runtime_context *runtime_ctx,
                         const scene &scene, const std::string &path,
                         bool is_prefab)
{
  rsc::scene &mutable_scene = const_cast<rsc::scene &> (scene);
  rsc::io::scene_snapshot_serializer serializer (runtime_ctx, mutable_scene);
  serializer.is_prefab = is_prefab;

  // `.wscn` is JSON by contract, so scenes saved from the editor stay
  // readable by the CLI and the project generator.
  if (path.ends_with (".json") || path.ends_with (".scene")
      || path.ends_with (".prefab") || path.ends_with (".wscn")) {
    return serializer.save_json (path);
  }

  return serializer.save_binary (path);
}

} // namespace wsl
