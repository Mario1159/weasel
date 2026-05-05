#include "scene_loader.hpp"

#include "wsl/comp/singl/editor_context.hpp"
#include "scene_snapshot_serializer.hpp"

#include <exception>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>


namespace wsl
{

std::shared_ptr<rsc::scene>
rsc::scene_loader::operator() (comp::singl::runtime_context *runtime_ctx,
                               comp::singl::editor_context *editor_ctx,
                               const std::string &path) const
{
  try {
    spdlog::debug ("scene_loader: operator() started for path: {}", path);
    std::filesystem::path const p (path);

    std::shared_ptr<scene> scn = std::make_shared<scene> (
        runtime_ctx, editor_ctx, p.stem ().string ());

    spdlog::debug ("scene_loader: creating serializer for scene: {}",
                   scn->get_name ());
    rsc::io::scene_snapshot_serializer serializer{ runtime_ctx, *scn };

    const bool is_json = path.ends_with (".json") || path.ends_with (".scene")
                         || path.ends_with (".prefab");
    spdlog::debug ("scene_loader: loading as {}", is_json ? "JSON" : "binary");

    const bool loaded = is_json ? serializer.load_json (path)
                                : serializer.load_binary (path);
    if (!loaded) {
      spdlog::error ("Failed to load scene: {}", path);
      return {};
    }

    spdlog::debug ("scene_loader: successfully loaded scene: {}", path);
    return scn;
  } catch (const std::exception &e) {
    spdlog::error ("Scene loader exception for '{}': {}", path, e.what ());
  } catch (...) {
    spdlog::error ("Scene loader exception for '{}': unknown exception", path);
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

  if (path.ends_with (".json") || path.ends_with (".scene")
      || path.ends_with (".prefab")) {
    return serializer.save_json (path);
  }

  return serializer.save_binary (path);
}

} // namespace wsl
