#include "animation_system.hpp"

#include "../comp/model_instance_3d.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "../gfx/model_3d.hpp"
#include "../rsc/animation_importer.hpp"
#include "../rsc/resource_manager.hpp"
#include "wsl/log/log.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <entt/entity/entity.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_float4x4.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

#include <tracy/Tracy.hpp>

namespace wsl
{

namespace sys
{

struct animation_system::runtime_state
{
  std::string clip_path;
  std::string skeleton_path;
  rsc::animation_id current_animation{};
  rsc::animation_id previous_animation{};
  rsc::skeleton_id skeleton_id{};
  entt::id_type model_id = entt::null;
  int skin_index = -1;

  std::vector<int> ozz_for_gltf;
  bool remap_ready = false;
  bool remap_failed = false;

  std::unique_ptr<ozz::animation::SamplingJob::Context> current_context;
  std::unique_ptr<ozz::animation::SamplingJob::Context> previous_context;
  std::vector<ozz::math::SoaTransform> current_local;
  std::vector<ozz::math::SoaTransform> previous_local;
  std::vector<ozz::math::SoaTransform> blended_local;
  std::vector<ozz::math::Float4x4> model_matrices;

  const ozz::animation::Skeleton *cached_skeleton = nullptr;
  bool configured = false;
  bool assets_loaded = false;
  entt::id_type clips_source = entt::null;
  double asset_retry_timer = 0.0;
  bool asset_failure_logged = false;
  bool job_failure_logged = false;
};

animation_system::animation_system (const std::string &name)
    : ecs_system_t (name)
{
  set_relationships ({}, {});
}

animation_system::~animation_system () = default;

namespace animation
{

std::vector<int>
build_joint_remap (const std::vector<std::string> &source_names,
                   const std::vector<std::string> &target_names)
{
  std::unordered_map<std::string, int> target_indices;
  std::unordered_set<std::string> duplicate_names;
  target_indices.reserve (target_names.size ());
  for (std::size_t index = 0; index < target_names.size (); ++index) {
    if (!target_names[index].empty ()) {
      if (!target_indices
               .emplace (target_names[index], static_cast<int> (index))
               .second) {
        duplicate_names.insert (target_names[index]);
      }
    }
  }

  std::vector<int> remap (source_names.size (), -1);
  for (std::size_t index = 0; index < source_names.size (); ++index) {
    if (source_names[index].empty ()
        || duplicate_names.contains (source_names[index])) {
      continue;
    }

    if (auto it = target_indices.find (source_names[index]);
        it != target_indices.end ()) {
      remap[index] = it->second;
    }
  }

  return remap;
}

animation::playback_step
advance_playback (float time, float duration, float delta_seconds, float speed,
                  bool loop)
{
  if (!(duration > 0.0F) || !std::isfinite (duration)) {
    return { 0.0F, true };
  }

  float const step = std::max (0.0F, delta_seconds) * speed;
  float next = time + step;
  if (!std::isfinite (next)) {
    return { loop ? 0.0F : duration, !loop };
  }

  if (loop) {
    next = std::fmod (next, duration);
    if (next < 0.0F) {
      next += duration;
    }
    return { next, false };
  }

  if (next <= 0.0F) {
    return { 0.0F, true };
  }
  if (next >= duration) {
    return { duration, true };
  }
  return { next, false };
}

float
advance_blend (float blend_weight, float duration, float delta_seconds)
{
  if (!(duration > 0.0F)) {
    return 1.0F;
  }

  return std::clamp (blend_weight + std::max (0.0F, delta_seconds) / duration,
                     0.0F, 1.0F);
}

bool
compose_skin_palette (const std::vector<glm::mat4> &model_matrices,
                      const std::vector<int> &ozz_for_gltf,
                      const std::vector<glm::mat4> &inverse_binds,
                      std::vector<glm::mat4> &palette)
{
  if (ozz_for_gltf.size () != inverse_binds.size ()) {
    return false;
  }

  palette.assign (ozz_for_gltf.size (), glm::mat4 (1.0F));
  for (std::size_t index = 0; index < ozz_for_gltf.size (); ++index) {
    int const ozz_index = ozz_for_gltf[index];
    if (ozz_index < 0
        || static_cast<std::size_t> (ozz_index) >= model_matrices.size ()) {
      return false;
    }

    palette[index] = model_matrices[static_cast<std::size_t> (ozz_index)]
                     * inverse_binds[index];
  }

  return true;
}

std::string
derive_skeleton_path (std::string_view clip_path)
{
  constexpr std::string_view animation_suffix = ".anim.ozz";
  if (clip_path.size () <= animation_suffix.size ()
      || clip_path.compare (clip_path.size () - animation_suffix.size (),
                            animation_suffix.size (), animation_suffix)
             != 0) {
    return {};
  }

  std::string base = std::string (
      clip_path.substr (0, clip_path.size () - animation_suffix.size ()));
  std::size_t const filename_start = base.find_last_of ("/\\");
  std::size_t const name_start
      = filename_start == std::string::npos ? 0 : filename_start + 1;
  std::size_t const separator = base.rfind ('_');
  if (separator != std::string::npos && separator >= name_start
      && separator + 1 < base.size ()) {
    base.resize (separator);
  }
  return base + ".skel.ozz";
}

} // namespace animation

namespace
{

bool
is_unset_path (const std::string &path)
{
  return path.empty () || path == "None";
}

int
find_skin_index (const gfx::model_3d &model, int scene_index, int requested)
{
  if (requested >= 0
      && static_cast<std::size_t> (requested) < model.skins.size ()) {
    return requested;
  }

  if (scene_index >= 0
      && static_cast<std::size_t> (scene_index) < model.scenes.size ()) {
    const gfx::scene &scene
        = model.scenes[static_cast<std::size_t> (scene_index)];
    std::vector<const gfx::node *> pending;
    for (const gfx::node &root : scene.roots) {
      pending.push_back (&root);
    }

    while (!pending.empty ()) {
      const gfx::node *node = pending.back ();
      pending.pop_back ();
      if (node == nullptr) {
        continue;
      }
      if (node->skin_index >= 0
          && static_cast<std::size_t> (node->skin_index)
                 < model.skins.size ()) {
        return node->skin_index;
      }
      for (const gfx::node &child : node->children) {
        pending.push_back (&child);
      }
    }
  }

  if (!model.skins.empty ()) {
    return 0;
  }
  return -1;
}

glm::mat4
ozz_matrix_to_glm (const ozz::math::Float4x4 &matrix)
{
  glm::mat4 result (1.0F);
  for (int column = 0; column < 4; ++column) {
    result[column][0] = ozz::math::GetX (matrix.cols[column]);
    result[column][1] = ozz::math::GetY (matrix.cols[column]);
    result[column][2] = ozz::math::GetZ (matrix.cols[column]);
    result[column][3] = ozz::math::GetW (matrix.cols[column]);
  }
  return result;
}

} // namespace

void
animation_system::register_iterations (event::event_hub &hub)
{
  clear_registered_iterations ();
  register_iteration<comp::animator, comp::skeleton_pose> (
      hub, "update_animators", [this] (entt::registry &registry, double dt) {
        update_animators (registry, dt);
      });
}

void
animation_system::on_update (entt::registry &registry, double dt)
{
  run_registered_iterations (registry, dt);
}

void
animation_system::on_editor_update (entt::registry &registry, double /*dt*/)
{
  // Keep the rest pose (or the current authored time) visible in the editor,
  // but never advance playback while the simulation is stopped.
  run_registered_iterations (registry, 0.0);
}

void
animation_system::on_inactive (entt::registry &registry)
{
  for (const auto &entry : m_states) {
    clear_pose (registry, entry.first);
  }
  m_states.clear ();
}

void
animation_system::clear_pose (entt::registry &registry, entt::entity entity)
{
  if (registry.valid (entity)) {
    static_cast<void> (registry.remove<comp::skeleton_pose> (entity));
  }
}

bool
animation_system::configure_instance (entt::registry &registry,
                                      entt::entity /*entity*/,
                                      comp::animator &animator_value,
                                      runtime_state &state)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return false;
  }

  std::string const clip_path = is_unset_path (animator_value.clip_path)
                                    ? "None"
                                    : animator_value.clip_path;
  std::string skeleton_path = is_unset_path (animator_value.skeleton_path)
                                  ? animation::derive_skeleton_path (clip_path)
                                  : animator_value.skeleton_path;
  if (is_unset_path (skeleton_path)) {
    skeleton_path = "None";
  }

  auto &resources
      = ctx.get<comp::singl::runtime_context *> ()->resource_manager ();
  bool const resources_missing
      = state.configured
        && ((state.current_animation.value != entt::null
             && !resources.contains (state.current_animation))
            || (state.skeleton_id.value != entt::null
                && !resources.contains (state.skeleton_id)));
  bool const configuration_changed
      = !state.configured || state.clip_path != clip_path
        || state.skeleton_path != skeleton_path || resources_missing;
  if (!configuration_changed) {
    return clip_path != "None";
  }

  bool const clip_changed = state.configured && state.clip_path != clip_path;
  if (clip_changed && state.current_animation.value != entt::null
      && animator_value.crossfade_duration > 0.0F) {
    state.previous_animation = state.current_animation;
    animator_value.previous_animation = state.previous_animation;
    animator_value.previous_time = animator_value.time;
    animator_value.previous_loop = animator_value.loop;
    animator_value.blend_weight = 0.0F;
  } else {
    state.previous_animation = {};
    animator_value.previous_animation = {};
    animator_value.previous_time = 0.0F;
    animator_value.previous_loop = true;
    animator_value.blend_weight = 1.0F;
  }

  if (clip_changed) {
    animator_value.time = 0.0F;
    animator_value.finished = false;
  }

  state.clip_path = clip_path;
  state.skeleton_path = skeleton_path;
  state.configured = true;
  state.remap_ready = false;
  state.remap_failed = false;
  state.model_id = entt::null;
  state.skin_index = -1;
  state.ozz_for_gltf.clear ();
  state.current_context.reset ();
  state.previous_context.reset ();
  state.current_local.clear ();
  state.previous_local.clear ();
  state.blended_local.clear ();
  state.model_matrices.clear ();
  state.cached_skeleton = nullptr;
  state.assets_loaded = false;
  state.clips_source = entt::null;
  state.asset_retry_timer = 0.0;
  state.asset_failure_logged = false;
  state.job_failure_logged = false;
  if (clip_path == "None") {
    state.current_animation = {};
    state.skeleton_id = {};
    animator_value.current_animation = {};
    animator_value.current_skeleton = {};
    return false;
  }

  state.current_animation = resources.register_animation (clip_path);
  animator_value.current_animation = state.current_animation;
  if (skeleton_path == "None") {
    state.skeleton_id = {};
  } else {
    state.skeleton_id = resources.register_skeleton (skeleton_path);
  }
  animator_value.current_skeleton = state.skeleton_id;

  return true;
}

bool
animation_system::sample_instance (entt::registry &registry,
                                   entt::entity /*entity*/,
                                   comp::animator &animator_value,
                                   runtime_state &state,
                                   ozz::animation::Skeleton *skeleton,
                                   ozz::animation::Animation *animation,
                                   double delta_seconds)
{
  if (skeleton == nullptr || animation == nullptr
      || skeleton->num_joints () <= 0) {
    return false;
  }

  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return false;
  }
  auto &resources
      = ctx.get<comp::singl::runtime_context *> ()->resource_manager ();

  if (state.cached_skeleton != skeleton) {
    state.current_context
        = std::make_unique<ozz::animation::SamplingJob::Context> (
            skeleton->num_joints ());
    state.previous_context
        = std::make_unique<ozz::animation::SamplingJob::Context> (
            skeleton->num_joints ());
    state.current_local.assign (skeleton->joint_rest_poses ().begin (),
                                skeleton->joint_rest_poses ().end ());
    state.previous_local.assign (skeleton->joint_rest_poses ().begin (),
                                 skeleton->joint_rest_poses ().end ());
    state.blended_local.assign (skeleton->joint_rest_poses ().begin (),
                                skeleton->joint_rest_poses ().end ());
    state.model_matrices.resize (
        static_cast<std::size_t> (skeleton->num_joints ()));
    state.cached_skeleton = skeleton;
  }

  ozz::animation::Animation *previous_animation = nullptr;
  if (state.previous_animation.value != entt::null) {
    previous_animation = resources.get (state.previous_animation);
    if (previous_animation == nullptr && state.asset_retry_timer <= 0.0) {
      previous_animation = resources.load (state.previous_animation);
      if (previous_animation == nullptr) {
        state.asset_retry_timer = 1.0;
      }
    }
  }
  if (previous_animation == nullptr) {
    state.previous_animation = {};
    animator_value.previous_animation = {};
    animator_value.blend_weight = 1.0F;
  }

  if (animator_value.loop) {
    animator_value.finished = false;
  } else if (animator_value.time >= animation->duration ()) {
    animator_value.finished = true;
  } else {
    animator_value.finished = false;
  }

  if (animator_value.playing && !animator_value.paused
      && !animator_value.finished && delta_seconds > 0.0) {
    animation::playback_step const step = animation::advance_playback (
        animator_value.time, animation->duration (),
        static_cast<float> (delta_seconds), animator_value.speed,
        animator_value.loop);
    animator_value.time = step.time;
    animator_value.finished = step.finished;

    if (previous_animation != nullptr) {
      animation::playback_step const previous_step
          = animation::advance_playback (
              animator_value.previous_time, previous_animation->duration (),
              static_cast<float> (delta_seconds), animator_value.speed,
              animator_value.previous_loop);
      animator_value.previous_time = previous_step.time;
    }
  }

  // A crossfade is a transition state, not part of the current clip's clock.
  // It must finish even when the new clip has already reached its end or the
  // authored playing flag is false.
  if (previous_animation != nullptr && !animator_value.paused
      && delta_seconds > 0.0) {
    animator_value.blend_weight = animation::advance_blend (
        animator_value.blend_weight, animator_value.crossfade_duration,
        static_cast<float> (delta_seconds));
  }

  if (animator_value.blend_weight >= 1.0F) {
    state.previous_animation = {};
    animator_value.previous_animation = {};
  }

  auto report_job_failure = [&] (const char *stage) {
    if (!state.job_failure_logged) {
      wsl::log::sys ()->warn (
          "Animator sampling failed during {}; pose will be skipped", stage);
      state.job_failure_logged = true;
    }
    return false;
  };

  auto sample = [] (ozz::animation::Animation *clip, float time,
                    ozz::animation::SamplingJob::Context *context,
                    std::vector<ozz::math::SoaTransform> &output,
                    ozz::span<const ozz::math::SoaTransform> rest_pose) {
    output.assign (rest_pose.begin (), rest_pose.end ());
    if (clip == nullptr || context == nullptr || clip->duration () <= 0.0F) {
      return true;
    }

    ozz::animation::SamplingJob job;
    job.animation = clip;
    job.context = context;
    job.ratio = std::clamp (time / clip->duration (), 0.0F, 1.0F);
    job.output = ozz::make_span (output);
    return job.Run ();
  };

  if (!sample (animation, animator_value.time, state.current_context.get (),
               state.current_local, skeleton->joint_rest_poses ())) {
    return report_job_failure ("current clip sampling");
  }

  if (previous_animation != nullptr) {
    if (!sample (previous_animation, animator_value.previous_time,
                 state.previous_context.get (), state.previous_local,
                 skeleton->joint_rest_poses ())) {
      return report_job_failure ("previous clip sampling");
    }
  }

  if (previous_animation != nullptr && animator_value.blend_weight < 1.0F) {
    std::array<ozz::animation::BlendingJob::Layer, 2> layers{};
    layers[0].weight = animator_value.blend_weight;
    layers[0].transform = ozz::make_span (state.current_local);
    layers[1].weight = 1.0F - animator_value.blend_weight;
    layers[1].transform = ozz::make_span (state.previous_local);

    ozz::animation::BlendingJob blend_job;
    blend_job.layers
        = ozz::span<ozz::animation::BlendingJob::Layer> (layers.data (), 2);
    blend_job.rest_pose = skeleton->joint_rest_poses ();
    blend_job.output = ozz::make_span (state.blended_local);
    if (!blend_job.Run ()) {
      return report_job_failure ("clip blending");
    }
  } else {
    state.blended_local = state.current_local;
  }

  ozz::animation::LocalToModelJob local_to_model;
  local_to_model.skeleton = skeleton;
  local_to_model.input = ozz::make_span (state.blended_local);
  local_to_model.output = ozz::make_span (state.model_matrices);
  if (!local_to_model.Run ()) {
    return report_job_failure ("local-to-model conversion");
  }
  state.job_failure_logged = false;
  return true;
}

void
animation_system::write_palette (
    entt::registry &registry, entt::entity entity,
    const comp::model_instance_3d & /*model_instance*/,
    const gfx::model_3d &model, runtime_state &state)
{
  if (state.skin_index < 0
      || static_cast<std::size_t> (state.skin_index) >= model.skins.size ()) {
    return;
  }

  const gfx::skin &skin
      = model.skins[static_cast<std::size_t> (state.skin_index)];
  if (skin.inverse_binds.size () != skin.joint_names.size ()) {
    clear_pose (registry, entity);
    return;
  }

  if (!state.remap_ready && !state.remap_failed) {
    std::vector<std::string> target_names;
    auto const &ozz_names = state.cached_skeleton->joint_names ();
    target_names.reserve (ozz_names.size ());
    for (const char *name : ozz_names) {
      target_names.emplace_back (name != nullptr ? name : "");
    }

    state.ozz_for_gltf
        = animation::build_joint_remap (skin.joint_names, target_names);
    if (std::find (state.ozz_for_gltf.begin (), state.ozz_for_gltf.end (), -1)
        != state.ozz_for_gltf.end ()) {
      wsl::log::sys ()->warn (
          "Animator on entity {} could not remap every model joint by name",
          static_cast<std::uint32_t> (entity));
      state.remap_failed = true;
      clear_pose (registry, entity);
      return;
    }
    state.remap_ready = true;
  }

  if (!state.remap_ready
      || state.model_matrices.size () < state.ozz_for_gltf.size ()) {
    return;
  }

  std::vector<glm::mat4> model_matrices;
  model_matrices.reserve (state.model_matrices.size ());
  for (const ozz::math::Float4x4 &matrix : state.model_matrices) {
    model_matrices.push_back (ozz_matrix_to_glm (matrix));
  }

  comp::skeleton_pose *pose = registry.try_get<comp::skeleton_pose> (entity);
  if (pose == nullptr) {
    pose = &registry.emplace<comp::skeleton_pose> (entity);
  }
  if (!animation::compose_skin_palette (model_matrices, state.ozz_for_gltf,
                                        skin.inverse_binds, pose->palette)) {
    pose->clear ();
  }
}

void
animation_system::update_animators (entt::registry &registry,
                                    double delta_seconds)
{
  ZoneScopedN ("animation_system::update_animators");

  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime = *ctx.get<comp::singl::runtime_context *> ();
  auto &resources = runtime.resource_manager ();

  std::unordered_set<entt::entity> live_entities;
  auto view = registry.view<comp::animator> ();

  for (const entt::entity entity : view) {
    live_entities.insert (entity);
    comp::animator &animator_value = view.get<comp::animator> (entity);
    std::unique_ptr<runtime_state> &state_ptr = m_states[entity];
    if (!state_ptr) {
      state_ptr = std::make_unique<runtime_state> ();
    }
    runtime_state &state = *state_ptr;
    state.asset_retry_timer = std::max (
        0.0, state.asset_retry_timer - std::max (0.0, delta_seconds));

    const comp::model_instance_3d *model_instance
        = registry.try_get<comp::model_instance_3d> (entity);
    if (model_instance == nullptr || model_instance->id.value == entt::null) {
      clear_pose (registry, entity);
      continue;
    }

    static_cast<void> (resources.load (model_instance->id));
    auto model = resources.get (model_instance->id);
    if (!model) {
      clear_pose (registry, entity);
      continue;
    }

    // Refresh the editor's clip dropdown from the model's import sidecar.
    //
    // This runs *before* configure_instance() and is independent of playback
    // state. It used to sit further down, behind the configure_instance()
    // gate, but a freshly added animator has no clip selected, so
    // configure_instance() returns false ("None") and the loop continued
    // before ever reaching the refresh -- leaving the picker permanently
    // empty, and the clip unselectable in the first place.
    //
    // The refresh is keyed on the model id, so it costs one comparison rather
    // than a directory scan every frame.
    if (state.clips_source != model_instance->id.value) {
      state.clips_source = model_instance->id.value;
      animator_value.available_clips.clear ();

      // get_resource_path() hands back a logical "res://rsc/models/Fox.glb"
      // form for anything inside the project root, but read_manifest() stats
      // the filesystem directly and takes no scheme. Passing the logical path
      // straight through made every lookup miss, so the dropdown stayed empty
      // for perfectly well imported models. resolve_path() is the engine's
      // res:// -> absolute translation and is what every loader uses.
      const std::string source_path = resources.resolve_path (
          resources.get_resource_path (model_instance->id));
      if (const std::optional<rsc::animation_import_manifest> manifest
          = rsc::animation_importer::read_manifest (source_path)) {
        animator_value.available_clips.reserve (manifest->clips.size ());
        for (const auto &[name, path] : manifest->clips) {
          animator_value.available_clips.push_back ({ name, path });
        }
      }
    }

    if (!configure_instance (registry, entity, animator_value, state)) {
      clear_pose (registry, entity);
      continue;
    }

    const int skin_index = find_skin_index (*model, model_instance->scene_index,
                                            animator_value.skin_index);
    if (skin_index < 0) {
      clear_pose (registry, entity);
      continue;
    }

    if (state.model_id != model_instance->id.value
        || state.skin_index != skin_index) {
      state.model_id = model_instance->id.value;
      state.skin_index = skin_index;
      state.remap_ready = false;
      state.remap_failed = false;
      state.ozz_for_gltf.clear ();
    }

    if (!state.assets_loaded && state.asset_retry_timer <= 0.0) {
      ozz::animation::Skeleton *loaded_skeleton = nullptr;
      ozz::animation::Animation *loaded_animation = nullptr;
      if (state.skeleton_id.value != entt::null) {
        loaded_skeleton = resources.load (state.skeleton_id);
      }
      if (state.current_animation.value != entt::null) {
        loaded_animation = resources.load (state.current_animation);
      }

      if (loaded_skeleton != nullptr && loaded_animation != nullptr) {
        state.assets_loaded = true;
        state.asset_failure_logged = false;
      } else {
        if (!state.asset_failure_logged) {
          wsl::log::sys ()->warn (
              "Animator on entity {} could not load its skeleton/animation "
              "resources; retrying",
              static_cast<std::uint32_t> (entity));
          state.asset_failure_logged = true;
        }
        state.asset_retry_timer = 1.0;
      }
    }

    ozz::animation::Skeleton *skeleton = nullptr;
    ozz::animation::Animation *animation = nullptr;
    if (state.assets_loaded) {
      if (state.skeleton_id.value != entt::null) {
        skeleton = resources.get (state.skeleton_id);
      }
      if (state.current_animation.value != entt::null) {
        animation = resources.get (state.current_animation);
      }
      if (skeleton == nullptr || animation == nullptr) {
        state.assets_loaded = false;
        state.asset_retry_timer = 1.0;
      }
    }
    if (skeleton == nullptr || animation == nullptr) {
      clear_pose (registry, entity);
      continue;
    }

    if (!sample_instance (registry, entity, animator_value, state, skeleton,
                          animation, delta_seconds)) {
      clear_pose (registry, entity);
      continue;
    }

    animator_value.current_animation = state.current_animation;
    animator_value.previous_animation = state.previous_animation;
    // Drives the inspector scrub bar; 0 when the clip is not loaded yet.
    animator_value.clip_duration = animation->duration ();
    write_palette (registry, entity, *model_instance, *model, state);
  }

  for (auto it = m_states.begin (); it != m_states.end ();) {
    if (live_entities.contains (it->first)) {
      ++it;
      continue;
    }

    clear_pose (registry, it->first);
    it = m_states.erase (it);
  }
}

} // namespace sys

} // namespace wsl
