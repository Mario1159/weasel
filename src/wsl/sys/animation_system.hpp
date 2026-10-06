#pragma once

#include "system.hpp"

#include "../comp/animator.hpp"
#include "../comp/skeleton_pose.hpp"

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <glm/mat4x4.hpp>
#endif
#ifndef IN_MODULE_INTERFACE
#include <memory>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string_view>
#endif
#ifndef IN_MODULE_INTERFACE
#include <unordered_map>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace ozz::animation
{
class Animation;
class Skeleton;
}

namespace wsl
{

namespace comp
{
struct model_instance_3d;
}

namespace gfx
{
class model_3d;
}

namespace sys
{

namespace animation
{

/** Result of advancing a clip's playback clock. */
struct playback_step
{
  float time = 0.0F;
  bool finished = false;
};

/**
 * Builds a glTF-joint -> ozz-joint index table using joint names.
 *
 * The returned vector has one entry per source (glTF) joint. A value of -1
 * means that no matching ozz joint name was found.
 */
[[nodiscard]] std::vector<int>
build_joint_remap (const std::vector<std::string> &source_names,
                   const std::vector<std::string> &target_names);

/** Derives the conventional `<model>.skel.ozz` path for a clip. */
[[nodiscard]] std::string derive_skeleton_path (std::string_view clip_path);

/** Advances a clip clock, honoring looping and non-looping end behavior. */
[[nodiscard]] playback_step advance_playback (float time, float duration,
                                              float delta_seconds, float speed,
                                              bool loop);

/** Advances a normalized crossfade weight from 0 toward 1. */
[[nodiscard]] float advance_blend (float blend_weight, float duration,
                                   float delta_seconds);

/**
 * Converts model-space joint matrices into a glTF skin palette.
 *
 * The output is written in glTF joint order using `ozz_for_gltf` and each
 * matrix is multiplied by its corresponding inverse-bind matrix.
 */
[[nodiscard]] bool
compose_skin_palette (const std::vector<glm::mat4> &model_matrices,
                      const std::vector<int> &ozz_for_gltf,
                      const std::vector<glm::mat4> &inverse_binds,
                      std::vector<glm::mat4> &palette);

} // namespace animation

/**
 * Advances ozz clips and writes per-entity skeleton palettes.
 *
 * The system consumes `comp::animator` and `comp::model_instance_3d`, and
 * writes the transient `comp::skeleton_pose` component. GPU consumption of the
 * palette is added in the skinning milestone.
 */
class animation_system : public sys::ecs_system_t<animation_system>
{
public:
  explicit animation_system (const std::string &name);

  ~animation_system () override;

  void register_iterations (event::event_hub &hub) override;
  void on_update (entt::registry &registry, double dt) override;
  void on_editor_update (entt::registry &registry, double dt) override;
  void on_inactive (entt::registry &registry) override;

private:
  struct runtime_state;

  void update_animators (entt::registry &registry, double delta_seconds);
  void clear_pose (entt::registry &registry, entt::entity entity);
  bool configure_instance (entt::registry &registry, entt::entity entity,
                           comp::animator &animator_value,
                           runtime_state &state);
  bool sample_instance (entt::registry &registry, entt::entity entity,
                        comp::animator &animator_value, runtime_state &state,
                        ozz::animation::Skeleton *skeleton,
                        ozz::animation::Animation *animation,
                        double delta_seconds);
  void write_palette (entt::registry &registry, entt::entity entity,
                      const comp::model_instance_3d &model_instance,
                      const gfx::model_3d &model, runtime_state &state);

  std::unordered_map<entt::entity, std::unique_ptr<runtime_state>> m_states;
};

} // namespace sys

} // namespace wsl
