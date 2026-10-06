// M1 smoke test: the ozz-animation package links and its offline -> runtime
// pipeline works — build a 2-joint skeleton, build a clip, sample it, and
// assert the interpolated joint transforms. See OZZ_ANIMATION_PLAN.md (M1).

#include <doctest/doctest.h>

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/quaternion.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/base/memory/unique_ptr.h>
#include <ozz/base/span.h>

#include <cmath>
#include <string>

TEST_CASE ("ozz builds a 2-joint runtime skeleton")
{
  ozz::animation::offline::RawSkeleton raw;
  raw.roots.resize (1);
  raw.roots[0].name = "root";
  raw.roots[0].children.resize (1);
  raw.roots[0].children[0].name = "child";
  raw.roots[0].children[0].transform.translation
      = ozz::math::Float3 (0.f, 1.f, 0.f);

  REQUIRE (raw.Validate ());
  CHECK (raw.num_joints () == 2);

  ozz::animation::offline::SkeletonBuilder builder;
  ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder (raw);
  REQUIRE (skeleton != nullptr);
  CHECK (skeleton->num_joints () == 2);
  CHECK (skeleton->num_soa_joints () == 1);
  // Depth-first order: root first, then child.
  CHECK (std::string (skeleton->joint_names ()[0]) == "root");
  CHECK (std::string (skeleton->joint_names ()[1]) == "child");
}

TEST_CASE ("ozz samples an animation clip at a time ratio")
{
  // Raw clip: 2 tracks (must cover the skeleton's joints), 1s duration.
  // Root stays at the origin; the child slides from y=1 to y=2.
  ozz::animation::offline::RawAnimation raw;
  raw.duration = 1.f;
  raw.name = "smoke_clip";
  raw.tracks.resize (2);

  auto add_translation = [] (ozz::animation::offline::RawAnimation::JointTrack &track,
                             float time, const ozz::math::Float3 &value) {
    ozz::animation::offline::RawAnimation::TranslationKey key;
    key.time = time;
    key.value = value;
    track.translations.push_back (key);
  };
  auto add_rotation = [] (ozz::animation::offline::RawAnimation::JointTrack &track,
                          float time, const ozz::math::Quaternion &value) {
    ozz::animation::offline::RawAnimation::RotationKey key;
    key.time = time;
    key.value = value;
    track.rotations.push_back (key);
  };
  auto add_scale = [] (ozz::animation::offline::RawAnimation::JointTrack &track,
                       float time, const ozz::math::Float3 &value) {
    ozz::animation::offline::RawAnimation::ScaleKey key;
    key.time = time;
    key.value = value;
    track.scales.push_back (key);
  };

  // Rotation and scale are identity for the whole clip, for both joints.
  for (auto &track : raw.tracks) {
    add_rotation (track, 0.f, ozz::math::Quaternion::identity ());
    add_rotation (track, 1.f, ozz::math::Quaternion::identity ());
    add_scale (track, 0.f, ozz::math::Float3 (1.f, 1.f, 1.f));
    add_scale (track, 1.f, ozz::math::Float3 (1.f, 1.f, 1.f));
  }
  add_translation (raw.tracks[0], 0.f, ozz::math::Float3 (0.f, 0.f, 0.f));
  add_translation (raw.tracks[0], 1.f, ozz::math::Float3 (0.f, 0.f, 0.f));
  add_translation (raw.tracks[1], 0.f, ozz::math::Float3 (0.f, 1.f, 0.f));
  add_translation (raw.tracks[1], 1.f, ozz::math::Float3 (0.f, 2.f, 0.f));

  REQUIRE (raw.Validate ());

  ozz::animation::offline::AnimationBuilder builder;
  ozz::unique_ptr<ozz::animation::Animation> animation = builder (raw);
  REQUIRE (animation != nullptr);
  CHECK (animation->duration () == doctest::Approx (1.f));
  CHECK (animation->num_tracks () == 2);
  CHECK (std::string (animation->name ()) == "smoke_clip");

  // Sample at ratio 0.5 (t = 0.5s of a 1s clip).
  ozz::animation::SamplingJob::Context context (animation->num_tracks ());
  ozz::math::SoaTransform output[1]; // 2 tracks -> 1 SoA row (4 lanes)
  ozz::animation::SamplingJob job;
  job.animation = animation.get ();
  job.ratio = 0.5f;
  job.context = &context;
  job.output = ozz::make_span (output);
  REQUIRE (job.Run ());

  // SoA layout: lane 0 = root, lane 1 = child.
  CHECK (ozz::math::GetX (output[0].translation.x) == doctest::Approx (0.f));
  CHECK (ozz::math::GetX (output[0].translation.y) == doctest::Approx (0.f));
  // Child y interpolates linearly from 1 to 2: ~1.5 at ratio 0.5.
  // Tolerance accounts for ozz's keyframe quantization (~1e-4).
  CHECK (ozz::math::GetY (output[0].translation.y)
         == doctest::Approx (1.5f).epsilon (1e-3));
  // Rotation stays identity: y component of the child quaternion lane.
  // Near-zero, so compare with an absolute bound (relative epsilon is
  // meaningless against an exact-zero expectation).
  CHECK (std::fabs (ozz::math::GetY (output[0].rotation.y)) < 1e-3f);
  CHECK (ozz::math::GetY (output[0].rotation.w)
         == doctest::Approx (1.f).epsilon (1e-3));
  // Scale stays (1, 1, 1).
  CHECK (ozz::math::GetY (output[0].scale.x)
         == doctest::Approx (1.f).epsilon (1e-3));
}
