#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "../rsc/scene.hpp"

namespace wsl
{

namespace comp::singl
{
class runtime_context;
class editor_context;
} // namespace comp::singl

namespace rsc
{

struct scene_loader final : entt::resource_loader<scene>
{
  scene_loader () = default;

  std::shared_ptr<scene> operator() (comp::singl::runtime_context *runtime_ctx,
                                     comp::singl::editor_context *editor_ctx,
                                     const std::string &path) const;
  std::shared_ptr<scene> operator() (scene &&ready_scene) const;
  static bool save (comp::singl::runtime_context *runtime_ctx, const scene &scene,
             const std::string &path, bool is_prefab = false) ;
};

} // namespace rsc

} // namespace wsl
