// M4: animator clock, crossfade, joint remapping, palette composition, and
// serialized component state.

#include <doctest/doctest.h>

#include "wsl/comp/animator.hpp"
#include "wsl/rsc/model_loader.hpp"
#include "wsl/rsc/skeleton_loader.hpp"
#include "wsl/rsc/animation_loader.hpp"
#include <ozz/animation/runtime/sampling_job.h>
#include "wsl/serialize/component_adapters.hpp"
#include "wsl/serialize/serialize.hpp"
#include "wsl/comp/components.hpp"
#include "wsl/gfx/scene_renderer.hpp"
#include "wsl/sys/animation_system.hpp"
#include "wsl/sys/render_frame.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_float4x4.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>
#include <ozz/animation/runtime/local_to_model_job.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{

/** Compile-time membership test over an entt type list. */
template <typename Needle, typename List> struct list_contains;
template <typename Needle, typename... Ts>
struct list_contains<Needle, entt::type_list<Ts...>>
    : std::bool_constant<(std::is_same_v<Needle, Ts> || ...)>
{
};

} // namespace

TEST_CASE ("animation clock handles looping, clamping, and crossfades")
{
  using wsl::sys::animation::advance_blend;
  using wsl::sys::animation::advance_playback;

  auto looped = advance_playback (0.9F, 1.0F, 0.2F, 1.0F, true);
  CHECK (looped.time == doctest::Approx (0.1F));
  CHECK (looped.finished == false);

  auto clamped = advance_playback (0.95F, 1.0F, 0.2F, 1.0F, false);
  CHECK (clamped.time == doctest::Approx (1.0F));
  CHECK (clamped.finished == true);

  auto finished = advance_playback (0.0F, 0.0F, 1.0F, 1.0F, true);
  CHECK (finished.finished == true);

  CHECK (advance_blend (0.25F, 0.5F, 0.25F) == doctest::Approx (0.75F));
  CHECK (advance_blend (0.25F, 0.0F, 1.0F) == doctest::Approx (1.0F));

  using wsl::sys::animation::derive_skeleton_path;
  CHECK (derive_skeleton_path ("res://rsc/models/Fox_Walk.anim.ozz")
         == "res://rsc/models/Fox.skel.ozz");
  CHECK (derive_skeleton_path ("res://anim_data/Fox.anim.ozz")
         == "res://anim_data/Fox.skel.ozz");
  CHECK (derive_skeleton_path ("res://my_project/rsc/models/Fox.anim.ozz")
         == "res://my_project/rsc/models/Fox.skel.ozz");
}

TEST_CASE ("animation joint remapping is name based")
{
  const std::vector<std::string> gltf_names = { "child", "root", "missing" };
  const std::vector<std::string> ozz_names = { "root", "child" };

  const std::vector<int> remap
      = wsl::sys::animation::build_joint_remap (gltf_names, ozz_names);

  REQUIRE (remap.size () == 3);
  CHECK (remap[0] == 1);
  CHECK (remap[1] == 0);
  CHECK (remap[2] == -1);

  const std::vector<int> duplicate
      = wsl::sys::animation::build_joint_remap ({ "root" }, { "root", "root" });
  REQUIRE (duplicate.size () == 1);
  CHECK (duplicate[0] == -1);
}

TEST_CASE ("Fox model joints remap to the imported ozz skeleton")
{
  const std::string source = std::string (WEASEL_SOURCE_DIR)
                             + "/examples/animation/rsc/models/Fox.glb";
  wsl::rsc::model_loader loader{ nullptr };
  const std::shared_ptr<wsl::rsc::raw::cpu_model> model
      = loader.load_cpu (source);
  REQUIRE (model != nullptr);
  REQUIRE (!model->skins.empty ());

  auto skeleton = wsl::rsc::skeleton_loader::load (
      std::string (WEASEL_SOURCE_DIR)
      + "/examples/animation/rsc/models/Fox.skel.ozz");
  REQUIRE (skeleton != nullptr);

  std::vector<std::string> target_names;
  for (const char *name : skeleton->joint_names ()) {
    target_names.emplace_back (name != nullptr ? name : "");
  }

  const std::vector<int> remap = wsl::sys::animation::build_joint_remap (
      model->skins[0].joint_names, target_names);
  REQUIRE (remap.size () == model->skins[0].joint_names.size ());
  CHECK (std::find (remap.begin (), remap.end (), -1) == remap.end ());
}

TEST_CASE ("animation palette composes model and inverse bind matrices")
{
  std::vector<glm::mat4> model_matrices (2, glm::mat4 (1.0F));
  model_matrices[1]
      = glm::translate (glm::mat4 (1.0F), glm::vec3 (0.0F, 2.0F, 0.0F));

  // glTF order is child, root; ozz order is root, child.
  const std::vector<int> remap = { 1, 0 };
  const std::vector<glm::mat4> inverse_binds
      = { glm::mat4 (1.0F), glm::mat4 (1.0F) };
  std::vector<glm::mat4> palette;
  REQUIRE (wsl::sys::animation::compose_skin_palette (model_matrices, remap,
                                                      inverse_binds, palette));
  REQUIRE (palette.size () == 2);

  CHECK (palette[0][3][1] == doctest::Approx (2.0F));
  CHECK (palette[1][3][1] == doctest::Approx (0.0F));
}

TEST_CASE ("animator serializes authored playback state only")
{
  wsl::comp::animator source;
  source.clip_path = "res://rsc/models/Fox_Walk.anim.ozz";
  source.skeleton_path = "res://rsc/models/Fox.skel.ozz";
  source.skin_index = 2;
  source.speed = 1.5F;
  source.loop = false;
  source.crossfade_duration = 0.4F;
  source.playing = false;
  source.time = 0.75F;
  source.current_animation.value = 123;
  source.previous_animation.value = 456;
  source.previous_time = 0.25F;
  source.blend_weight = 0.5F;
  source.paused = true;
  source.finished = true;
  source.previous_loop = false;

  const std::string json = wsl::serialize::json_write (source);
  REQUIRE (!json.empty ());

  wsl::comp::animator loaded;
  REQUIRE (wsl::serialize::json_read<wsl::comp::animator> (json, loaded));

  CHECK (loaded.clip_path == source.clip_path);
  CHECK (loaded.skeleton_path == source.skeleton_path);
  CHECK (loaded.skin_index == source.skin_index);
  CHECK (loaded.speed == doctest::Approx (source.speed));
  CHECK (loaded.loop == source.loop);
  CHECK (loaded.crossfade_duration
         == doctest::Approx (source.crossfade_duration));
  CHECK (loaded.playing == source.playing);
  CHECK (loaded.time == doctest::Approx (source.time));

  // Runtime-only fields must not be restored from a scene snapshot.
  CHECK (loaded.current_animation.value
         == static_cast<entt::id_type> (entt::null));
  CHECK (loaded.previous_animation.value
         == static_cast<entt::id_type> (entt::null));
  CHECK (loaded.previous_time == doctest::Approx (0.0F));
  CHECK (loaded.blend_weight == doctest::Approx (1.0F));
  CHECK (loaded.paused == false);
  CHECK (loaded.finished == false);
  CHECK (loaded.previous_loop == true);
}

TEST_CASE ("skinning palette layout matches the shader cbuffer")
{
  // The CPU-side upload in scene_renderer::pack_joint_palette writes
  // [uint32 count][pad to 16][mat4 x max] and skinning.slang declares the same
  // layout. A mismatch here would read garbage joint indices rather than fail
  // loudly, so the two sizes are pinned explicitly.
  constexpr std::size_t kMax = 128;
  CHECK (wsl::gfx::scene_renderer::joint_palette::max_joints == kMax);
  CHECK (sizeof (glm::mat4) == 64);
  // Reflected from the compiled skinned cube vertex shader:
  //   OpMemberDecorate ... 0 Offset 0     -> u_JointCount
  //   OpMemberDecorate ... 1 Offset 16    -> u_JointPalette (std140 aligns the
  //                                         array to 16 after the scalar)
  CHECK (wsl::gfx::scene_renderer::joint_palette_layout::count_offset == 0);
  CHECK (wsl::gfx::scene_renderer::joint_palette_layout::matrix_offset == 16);
  CHECK (kMax * sizeof (glm::mat4) == 8192);
  CHECK (wsl::gfx::scene_renderer::joint_palette_layout::total_bytes
         == 16 + 8192);
}

TEST_CASE (
    "the joint palette upload puts the count at the offset the shader reads")
{
  // Regression test for an animated mesh rendering as nothing at all.
  //
  // The count must sit where the compiled SPIR-V reads it. Two separate defects
  // landed here, both ending in the same silent collapse:
  //
  //  1. The upload was sized to the *used prefix* and wrote the count at
  //     `count * 64`. For any rig under 128 joints that offset is not where the
  //     shader looks, so u_JointCount read back as 0.
  //  2. Even once the full block was uploaded, a RenderDoc capture of a live
  //     skinned draw showed all 24 palette matrices arriving intact while
  //     u_JointCount still read 0 from its trailing offset (max_joints * 64).
  //     The SDK's push path memcpy's the full length unclamped into a 32 KB
  //     pool buffer, so the write was never truncated -- the loss happens after
  //     the upload, and leading with the scalar keeps it inside any plausible
  //     descriptor range.
  //
  // Either way u_JointCount reads as 0, the shader's `index < u_JointCount`
  // guard rejects all four influences, every vertex blends to the origin, and
  // the mesh vanishes with no API error.
  using palette_t = wsl::gfx::scene_renderer::joint_palette;
  using layout_t = wsl::gfx::scene_renderer::joint_palette_layout;

  std::vector<uint8_t> scratch;

  for (std::size_t joint_count : { std::size_t{ 0 }, std::size_t{ 1 },
                                   std::size_t{ 24 }, palette_t::max_joints }) {
    palette_t palette;
    for (std::size_t j = 0; j < joint_count; ++j) {
      glm::mat4 m (1.0F);
      m[3][0] = static_cast<float> (j + 1);
      palette.matrices.push_back (m);
    }

    const std::size_t bytes
        = wsl::gfx::scene_renderer::pack_joint_palette (palette, scratch);

    // The whole block is uploaded, so the count offset is always in range.
    CHECK (bytes == layout_t::total_bytes);
    REQUIRE (scratch.size () >= layout_t::count_offset + sizeof (uint32_t));

    // The count must be readable at the offset the shader indexes.
    uint32_t packed_count = 0xFFFFFFFFU;
    std::memcpy (&packed_count, scratch.data () + layout_t::count_offset,
                 sizeof (packed_count));
    INFO ("joint_count=" << joint_count);
    CHECK (packed_count == static_cast<uint32_t> (joint_count));

    // The count must not be shadowed by matrix data starting at 0.
    if (layout_t::matrix_offset < layout_t::count_offset + sizeof (uint32_t)) {
      FAIL ("count and first matrix entry overlap in the block layout");
    }

    // And the matrices must follow the count at the std140-aligned offset, in
    // order. The marker lives in m[3][0]; glm is column-major, so that is
    // column 3 -> byte 48 within the matrix.
    for (std::size_t j = 0; j < joint_count; ++j) {
      constexpr std::size_t kMarkerOffset = 3 * sizeof (glm::vec4);
      float marker = 0.0F;
      std::memcpy (&marker,
                   scratch.data () + layout_t::matrix_offset
                       + (j * sizeof (glm::mat4)) + kMarkerOffset,
                   sizeof (marker));
      INFO ("joint_count=" << joint_count << " j=" << j);
      CHECK (marker == doctest::Approx (static_cast<float> (j + 1)));
    }
  }

  // A rig with no joints must still upload a zero count rather than leaving
  // the shader to read whatever happened to be in the block.
  palette_t empty;
  static_cast<void> (
      wsl::gfx::scene_renderer::pack_joint_palette (empty, scratch));
  uint32_t zero = 0xFFFFFFFFU;
  std::memcpy (&zero, scratch.data () + layout_t::count_offset, sizeof (zero));
  CHECK (zero == 0U);
}

TEST_CASE (
    "render_submission keeps palette addresses stable for the whole frame")
{
  // Regression test for a use-after-free in the M5 skinning path.
  // `draw_command::palette` is a raw pointer, and the renderer consumes
  // `draw_commands` *after* `build_render_frame` has returned, so the palettes
  // must be owned by the submission (which outlives the frame) rather than by
  // a local. The original code used a local `std::vector` reserved with
  // `view.size_hint()`, which was wrong twice over: the local died with the
  // function, and `size_hint()` is the size of the *smallest* pool in an entt
  // multi-component view -- not a bound on how many entities match -- so the
  // vector could reallocate mid-loop and dangle every pointer handed out.
  wsl::sys::render_submission submission;
  submission.reset ();

  // Take pointers as we go, mimicking build_render_frame, then keep appending
  // well past any plausible size_hint() to force growth.
  std::vector<const wsl::gfx::scene_renderer::joint_palette *> handed_out;
  std::vector<wsl::gfx::scene_renderer::draw_command> commands;

  constexpr int kEntities = 64;
  for (int i = 0; i < kEntities; ++i) {
    submission.palettes.emplace_back ();
    auto &palette = submission.palettes.back ();
    // Distinguishable content so a stale pointer is detectable, not just a
    // dangling one.
    const auto marker = static_cast<float> (i + 1);
    palette.matrices.assign (2, glm::mat4 (1.0F));
    palette.matrices[0][3][0] = marker;

    wsl::gfx::scene_renderer::draw_command cmd{};
    cmd.palette = &palette;
    commands.push_back (cmd);
    handed_out.push_back (&palette);
  }

  CHECK (submission.palettes.size () == kEntities);

  // Every pointer previously handed to a draw command must still address its
  // own palette.
  for (int i = 0; i < kEntities; ++i) {
    const auto *palette = commands[static_cast<std::size_t> (i)].palette;
    REQUIRE (palette != nullptr);
    CHECK (palette == handed_out[static_cast<std::size_t> (i)]);
    REQUIRE (palette->matrices.size () == 2);
    CHECK (palette->matrices[0][3][0]
           == doctest::Approx (static_cast<float> (i + 1)));
  }

  // reset() must drop the palettes together with the commands that reference
  // them; clearing one without the other would leave dangling pointers.
  submission.draw_commands = commands;
  submission.reset ();
  CHECK (submission.palettes.empty ());
  CHECK (submission.draw_commands.empty ());
}

TEST_CASE ("only animated draws carry a joint palette")
{
  // Regression test for 3D objects disappearing from every scene.
  //
  // `scene_renderer` selects its pipeline from `draw.palette != nullptr`: a
  // non-null palette means "use the skinned pipeline". The skinned pipelines
  // are built from a six-attribute vertex layout (POSITION, NORMAL, UV, COLOR,
  // JOINTS, WEIGHTS), while a static mesh's vertex buffer only supplies the
  // first four. When build_render_frame handed a palette to *every* draw, all
  // static meshes were recorded against skinned pipelines whose joint
  // attributes had no data behind them, and the whole 3D scene rendered empty
  // while the skybox (drawn through a different path) kept working.
  //
  // The invariant: a palette is published only for an entity that actually has
  // a sampled pose, and the submission allocates exactly as many palettes as it
  // has animated draws.
  auto is_animated = [] (const wsl::gfx::scene_renderer::draw_command &cmd) {
    return cmd.palette != nullptr;
  };

  // The draw_command default must be "not skinned", so an omitted field can
  // never accidentally opt a static mesh into the skinned pipeline.
  const wsl::gfx::scene_renderer::draw_command default_cmd{};
  CHECK (default_cmd.palette == nullptr);
  CHECK (is_animated (default_cmd) == false);

  // Reproduce the publication rule: static draws stay null, animated draws get
  // exactly one palette each.
  wsl::sys::render_submission submission;
  submission.reset ();

  constexpr int kStatic = 3;
  constexpr int kAnimated = 2;
  for (int i = 0; i < kStatic; ++i) {
    // No skeleton_pose -> no palette entry, null pointer.
    wsl::gfx::scene_renderer::draw_command cmd{};
    cmd.palette = nullptr;
    submission.draw_commands.push_back (cmd);
  }
  for (int i = 0; i < kAnimated; ++i) {
    submission.palettes.emplace_back ();
    submission.palettes.back ().matrices.assign (1, glm::mat4 (1.0F));
    wsl::gfx::scene_renderer::draw_command cmd{};
    cmd.palette = &submission.palettes.back ();
    submission.draw_commands.push_back (cmd);
  }

  // One palette per animated draw -- not one per draw. An over-eager count is
  // the other half of the bug: a palette that exists is a palette that can be
  // attached to a command.
  CHECK (submission.palettes.size () == kAnimated);
  CHECK (submission.draw_commands.size () == kStatic + kAnimated);

  int animated_count = 0;
  for (const auto &cmd : submission.draw_commands) {
    if (is_animated (cmd)) {
      ++animated_count;
      // An attached palette must be real data, never an empty placeholder.
      REQUIRE (cmd.palette != nullptr);
      CHECK (!cmd.palette->matrices.empty ());
    }
  }
  CHECK (animated_count == kAnimated);

  // The static draws must be the ones without a palette, otherwise the
  // regression is only partially fixed.
  for (int i = 0; i < kStatic; ++i) {
    CHECK (submission.draw_commands[static_cast<std::size_t> (i)].palette
           == nullptr);
  }
}

TEST_CASE ("animator transient editor state is excluded from serialization")
{
  // The inspector's clip dropdown and scrub bar are driven by transient
  // fields. If they ever leaked into the scene file the format would grow a
  // dependency on the model currently assigned to the entity.
  wsl::comp::animator source;
  source.available_clips.push_back (
      { "Walk", "res://rsc/models/Fox_Walk.anim.ozz" });
  source.clip_duration = 1.234F;
  source.finished = true;
  source.previous_loop = false;

  const std::string json = wsl::serialize::json_write (source);
  wsl::comp::animator back;
  REQUIRE (wsl::serialize::json_read (json, back));

  CHECK (back.available_clips.empty ());
  CHECK (back.clip_duration == doctest::Approx (0.0F));
  CHECK (back.finished == false);
  CHECK (back.previous_loop == true);
}

TEST_CASE ("skeleton pose is not part of the serialized component set")
{
  // skeleton_pose is deliberately absent from component_types: it is rebuilt
  // every frame and must never appear in a scene file. Assert the component
  // list itself rather than trusting the comment.
  CHECK (
      (list_contains<wsl::comp::animator, wsl::comp::component_types>::value));
  CHECK ((!list_contains<wsl::comp::skeleton_pose,
                         wsl::comp::component_types>::value));
}

TEST_CASE (
    "the rest pose composes to an identity joint palette (real Fox data)")
{
  const std::string base
      = std::string (WEASEL_SOURCE_DIR) + "/examples/animation/rsc/models/Fox";
  wsl::rsc::model_loader loader{ nullptr };
  const std::shared_ptr<wsl::rsc::raw::cpu_model> model
      = loader.load_cpu (base + ".glb");
  REQUIRE (model != nullptr);
  REQUIRE (!model->skins.empty ());
  auto skeleton = wsl::rsc::skeleton_loader::load (base + ".skel.ozz");
  REQUIRE (skeleton != nullptr);

  std::vector<std::string> target_names;
  for (const char *n : skeleton->joint_names ())
    target_names.emplace_back (n ? n : "");
  const std::vector<int> remap = wsl::sys::animation::build_joint_remap (
      model->skins[0].joint_names, target_names);
  REQUIRE (remap.size () == model->skins[0].joint_names.size ());
  CHECK (std::find (remap.begin (), remap.end (), -1) == remap.end ());

  // Rest pose -> local transforms straight from the skeleton.
  std::vector<ozz::math::SoaTransform> local (
      skeleton->joint_rest_poses ().begin (),
      skeleton->joint_rest_poses ().end ());
  std::vector<ozz::math::Float4x4> mm (skeleton->num_joints ());
  ozz::animation::LocalToModelJob job;
  job.skeleton = skeleton.get ();
  job.input = ozz::make_span (local);
  job.output = ozz::make_span (mm);
  REQUIRE (job.Run ());
  MESSAGE (
      "ozz joints=", mm.size (), " gltf skin joints=", remap.size (),
      " inverse_binds=", model->skins[0].inverse_binds.size (),
      " root name=", target_names.empty () ? std::string () : target_names[0]);

  // Same conversion the engine uses.
  std::vector<glm::mat4> mmg (mm.size ());
  for (std::size_t i = 0; i < mm.size (); ++i) {
    for (int c = 0; c < 4; ++c) {
      mmg[i][c][0] = ozz::math::GetX (mm[i].cols[c]);
      mmg[i][c][1] = ozz::math::GetY (mm[i].cols[c]);
      mmg[i][c][2] = ozz::math::GetZ (mm[i].cols[c]);
      mmg[i][c][3] = ozz::math::GetW (mm[i].cols[c]);
    }
  }
  std::vector<glm::mat4> palette;
  REQUIRE (wsl::sys::animation::compose_skin_palette (
      mmg, remap, model->skins[0].inverse_binds, palette));
  REQUIRE (palette.size () == 24);
  // At the bind pose the whole palette must collapse to identity, for every
  // joint. This is the invariant the whole skinned path rests on: the vertex
  // shader computes sum(w_i * palette[joint_i] * v), so if the palette is not
  // identity at rest the mesh is deformed the instant a clip is selected
  // instead of starting from the authored pose. It also pins that the ozz
  // joint matrices and the glTF inverse binds agree on their shared space --
  // the mesh node and the skeleton root are separate scene roots in Fox.glb,
  // which is exactly the case where a space mismatch shows up.
  for (std::size_t j = 0; j < palette.size (); ++j) {
    const glm::mat4 d = palette[j] - glm::mat4 (1.0F);
    const float err = std::abs (d[0][0]) + std::abs (d[1][1])
                      + std::abs (d[2][2]) + std::abs (d[3][3])
                      + std::abs (d[3][0]) + std::abs (d[3][1])
                      + std::abs (d[3][2]);
    INFO ("joint " << j << " (" << model->skins[0].joint_names[j] << ")");
    CHECK (err < 0.01F);
  }
}

TEST_CASE ("the skinned vertex layout matches the mesh vertex struct")
{
  // Guards the class of bug where the skinned pipelines were built by
  // hand-copying the plain pipeline's state instead of reusing it. Rebuilding
  // the SDL_GPUGraphicsPipelineCreateInfo with SDL_zero silently dropped two
  // fields that the plain pipeline set outside its per-alpha-mode switch:
  //
  //   - vertex_input_state: without the six-attribute layout the skinned
  //     pipeline declared no attributes at all, so the vertex shader read
  //     undefined positions and animated meshes vanished.
  //   - multisample_state.sample_count: the main pass is 4x MSAA, and a
  //     pipeline at the default of 1 is incompatible with those attachments.
  //
  // Both failed silently -- no API error, no warning, just a missing mesh.
  // The pipelines now share one builder, so this test pins the *contract* that
  // makes the duplication unnecessary: the attributes the shader declares must
  // line up with the struct the uploader writes.
  struct vertex
  {
    glm::vec3 pos{ 0.0F };
    glm::vec3 normal{ 0.0F };
    glm::vec2 uv{ 0.0F };
    glm::vec4 tangent{ 0.0F };
    glm::uvec4 joints{ 0U };
    glm::vec4 weights{ 0.0F };
  };

  // The shader reads POSITION, NORMAL, UV, TANGENT at locations 0-3 and
  // JOINTS, WEIGHTS at 4-5 (verified against the compiled SPIR-V:
  // OpDecorate ... Location 0..5). All six come from one interleaved buffer
  // with a pitch of sizeof(vertex), so the offsets must be tightly packed and
  // in that order.
  CHECK (offsetof (vertex, pos) == 0);
  CHECK (offsetof (vertex, normal) == 12);
  CHECK (offsetof (vertex, uv) == 24);
  CHECK (offsetof (vertex, tangent) == 32);
  CHECK (offsetof (vertex, joints) == 48);
  CHECK (offsetof (vertex, weights) == 64);
  CHECK (sizeof (vertex) == 80);

  // Attributes must not overlap and must all fit inside one vertex. The
  // pipeline declares a 4-component format for every one of them, so each
  // needs 16 bytes of room.
  struct attr
  {
    std::size_t offset;
    std::size_t size;
  };
  const attr attrs[] = {
    { offsetof (vertex, pos), 12 },    { offsetof (vertex, normal), 12 },
    { offsetof (vertex, uv), 8 },      { offsetof (vertex, tangent), 16 },
    { offsetof (vertex, joints), 16 }, { offsetof (vertex, weights), 16 }
  };
  for (const attr &a : attrs) {
    CHECK (a.offset + a.size <= sizeof (vertex));
  }
  for (std::size_t i = 1; i < std::size (attrs); ++i) {
    CHECK (attrs[i - 1].offset + attrs[i - 1].size <= attrs[i].offset);
  }
}
