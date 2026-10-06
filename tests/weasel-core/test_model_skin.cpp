// M3: glTF skin metadata and JOINTS_0/WEIGHTS_0 import coverage.

#include <doctest/doctest.h>

#include "wsl/gfx/mesh.hpp"
#include "wsl/rsc/model_loader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#ifndef WEASEL_SOURCE_DIR
#define WEASEL_SOURCE_DIR "."
#endif

namespace
{

struct buffer_view
{
  std::size_t offset = 0;
  std::size_t length = 0;
};

struct temporary_directory
{
  std::filesystem::path path;

  temporary_directory ()
  {
    const auto suffix
        = std::chrono::steady_clock::now ().time_since_epoch ().count ();
    path = std::filesystem::temp_directory_path ()
           / ("weasel_model_skin_" + std::to_string (suffix));
    std::filesystem::create_directories (path);
  }

  temporary_directory (const temporary_directory &) = delete;
  temporary_directory &operator= (const temporary_directory &) = delete;

  ~temporary_directory ()
  {
    std::error_code ec;
    std::filesystem::remove_all (path, ec);
  }
};

template <typename T>
buffer_view
append_values (std::vector<uint8_t> &bytes, const std::vector<T> &values)
{
  const std::size_t aligned_offset
      = (bytes.size () + (alignof (T) - 1)) & ~(alignof (T) - 1);
  bytes.resize (aligned_offset + (values.size () * sizeof (T)));

  if (!values.empty ()) {
    std::memcpy (bytes.data () + aligned_offset, values.data (),
                 values.size () * sizeof (T));
  }

  return { aligned_offset, values.size () * sizeof (T) };
}

void
write_buffer_view (std::ofstream &json, const buffer_view &view)
{
  json << "{\"buffer\":0,\"byteOffset\":" << view.offset
       << ",\"byteLength\":" << view.length << "}";
}

const wsl::rsc::raw::cpu_node *
find_skinned_node (const wsl::rsc::raw::cpu_node &node)
{
  if (node.skin_index >= 0) {
    return &node;
  }

  for (const wsl::rsc::raw::cpu_node &child : node.children) {
    if (const auto *found = find_skinned_node (child)) {
      return found;
    }
  }

  return nullptr;
}

const wsl::gfx::node *
find_skinned_node (const wsl::gfx::node &node)
{
  if (node.skin_index >= 0) {
    return &node;
  }

  for (const wsl::gfx::node &child : node.children) {
    if (const auto *found = find_skinned_node (child)) {
      return found;
    }
  }

  return nullptr;
}

void
check_joint (const glm::uvec4 &actual, const std::array<uint32_t, 4> &expected)
{
  CHECK (actual.x == expected[0]);
  CHECK (actual.y == expected[1]);
  CHECK (actual.z == expected[2]);
  CHECK (actual.w == expected[3]);
}

void
check_weight (const glm::vec4 &actual, const std::array<float, 4> &expected)
{
  CHECK (actual.x == doctest::Approx (expected[0]).epsilon (1e-6F));
  CHECK (actual.y == doctest::Approx (expected[1]).epsilon (1e-6F));
  CHECK (actual.z == doctest::Approx (expected[2]).epsilon (1e-6F));
  CHECK (actual.w == doctest::Approx (expected[3]).epsilon (1e-6F));
}

std::shared_ptr<wsl::rsc::raw::cpu_model>
load_skin_component_fixture (const temporary_directory &temporary)
{
  std::vector<uint8_t> bytes;

  const std::vector<std::array<float, 3>> positions
      = { { 0.0F, 0.0F, 0.0F }, { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } };
  const std::vector<std::array<uint8_t, 4>> joints8
      = { { 0, 3, 1, 2 }, { 1, 2, 3, 0 }, { 2, 1, 0, 3 } };
  const std::vector<std::array<uint8_t, 4>> weights8
      = { { 255, 0, 0, 0 }, { 0, 127, 128, 0 }, { 0, 64, 191, 0 } };
  const std::vector<std::array<uint16_t, 4>> joints16
      = { { 0, 3, 1, 2 }, { 1, 2, 3, 0 }, { 2, 1, 0, 3 } };
  const std::vector<std::array<uint16_t, 4>> weights16
      = { { 65535, 0, 0, 0 }, { 0, 32767, 32768, 0 }, { 0, 16384, 49151, 0 } };
  const std::vector<std::array<uint32_t, 4>> joints32
      = { { 0, 3, 1, 2 }, { 1, 2, 3, 0 }, { 2, 1, 0, 3 } };
  const std::vector<std::array<float, 4>> float_weights
      = { { 1.0F, 0.0F, 0.0F, 0.0F },
          { 0.0F, 0.75F, 0.25F, 0.0F },
          { 0.0F, 0.2F, 0.3F, 0.5F } };

  std::vector<std::array<float, 16>> inverse_binds (4);
  for (std::array<float, 16> &matrix : inverse_binds) {
    matrix.fill (0.0F);
    matrix[0] = 1.0F;
    matrix[5] = 1.0F;
    matrix[10] = 1.0F;
    matrix[15] = 1.0F;
  }
  inverse_binds[1][12] = 5.0F;

  const buffer_view position_view = append_values (bytes, positions);
  const buffer_view joints8_view = append_values (bytes, joints8);
  const buffer_view weights8_view = append_values (bytes, weights8);
  const buffer_view joints16_view = append_values (bytes, joints16);
  const buffer_view weights16_view = append_values (bytes, weights16);
  const buffer_view joints32_view = append_values (bytes, joints32);
  const buffer_view float_weights_view = append_values (bytes, float_weights);
  const buffer_view inverse_binds_view = append_values (bytes, inverse_binds);

  const std::filesystem::path buffer_path = temporary.path / "skin-fixture.bin";
  {
    std::ofstream buffer (buffer_path, std::ios::binary);
    REQUIRE (buffer.good ());
    buffer.write (reinterpret_cast<const char *> (bytes.data ()),
                  static_cast<std::streamsize> (bytes.size ()));
    REQUIRE (buffer.good ());
  }

  const std::filesystem::path gltf_path
      = temporary.path / "skin-component-types.gltf";
  {
    std::ofstream json (gltf_path);
    REQUIRE (json.good ());
    json << "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"skin-"
            "fixture.bin\",\"byteLength\":"
         << bytes.size () << "}],\"bufferViews\":[";
    write_buffer_view (json, position_view);
    json << ',';
    write_buffer_view (json, joints8_view);
    json << ',';
    write_buffer_view (json, weights8_view);
    json << ',';
    write_buffer_view (json, joints16_view);
    json << ',';
    write_buffer_view (json, weights16_view);
    json << ',';
    write_buffer_view (json, joints32_view);
    json << ',';
    write_buffer_view (json, float_weights_view);
    json << ',';
    write_buffer_view (json, inverse_binds_view);
    json << "],\"accessors\":["
         << "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":"
            "\"VEC3\"},"
         << "{\"bufferView\":1,\"componentType\":5121,\"count\":3,\"type\":"
            "\"VEC4\"},"
         << "{\"bufferView\":2,\"componentType\":5121,\"normalized\":true,"
            "\"count\":3,\"type\":\"VEC4\"},"
         << "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":"
            "\"VEC4\"},"
         << "{\"bufferView\":4,\"componentType\":5123,\"normalized\":true,"
            "\"count\":3,\"type\":\"VEC4\"},"
         << "{\"bufferView\":5,\"componentType\":5125,\"count\":3,\"type\":"
            "\"VEC4\"},"
         << "{\"bufferView\":6,\"componentType\":5126,\"count\":3,\"type\":"
            "\"VEC4\"},"
         << "{\"bufferView\":7,\"componentType\":5126,\"count\":4,\"type\":"
            "\"MAT4\"}"
         << "],\"meshes\":[{\"primitives\":["
         << "{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2},"
            "\"mode\":4},"
         << "{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":3,\"WEIGHTS_0\":4},"
            "\"mode\":4},"
         << "{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":5,\"WEIGHTS_0\":6},"
            "\"mode\":4},"
         << "{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":6},"
            "\"mode\":4}"
         << "]}],\"skins\":["
         << "{\"joints\":[1,2,3,4],\"inverseBindMatrices\":7,\"name\":"
            "\"component_skin\"},"
         << "{\"joints\":[1,2,3,4],\"name\":\"identity_skin\"}"
         << "],\"nodes\":["
         << "{\"mesh\":0,\"skin\":0,\"name\":\"skinned_mesh_node\"},"
         << "{\"name\":\"joint_0\"},{\"name\":\"joint_1\"},"
         << "{\"name\":\"joint_2\"},{\"name\":\"joint_3\"},"
         << "{\"name\":\"skeleton_root\",\"children\":[1,2,3,4]}"
         << "],\"scenes\":[{\"nodes\":[5,0]}],\"scene\":0}";
    REQUIRE (json.good ());
  }

  wsl::rsc::model_loader loader{ nullptr };
  return loader.load_cpu (gltf_path.string ());
}

} // namespace

static_assert (std::is_standard_layout_v<wsl::rsc::raw::cpu_vertex>);
static_assert (std::is_trivially_copyable_v<wsl::rsc::raw::cpu_vertex>);
static_assert (std::is_standard_layout_v<wsl::gfx::vertex>);
static_assert (std::is_trivially_copyable_v<wsl::gfx::vertex>);

TEST_CASE ("mesh vertices default to unskinned data")
{
  const wsl::rsc::raw::cpu_vertex cpu_vertex{};
  CHECK (cpu_vertex.joints == glm::uvec4 (0U));
  CHECK (cpu_vertex.weights == glm::vec4 (0.0F));

  const wsl::gfx::vertex gpu_vertex{};
  CHECK (gpu_vertex.joints == glm::uvec4 (0U));
  CHECK (gpu_vertex.weights == glm::vec4 (0.0F));
}

TEST_CASE ("model loader imports skin component widths and metadata")
{
  temporary_directory temporary;
  const std::shared_ptr<wsl::rsc::raw::cpu_model> cpu
      = load_skin_component_fixture (temporary);
  REQUIRE (cpu != nullptr);
  REQUIRE (cpu->skins.size () == 2);
  REQUIRE (cpu->meshes.size () == 1);
  REQUIRE (cpu->meshes[0].primitives.size () == 4);

  const auto &skin = cpu->skins[0];
  CHECK (skin.joint_nodes == std::vector<std::size_t> ({ 1, 2, 3, 4 }));
  CHECK (skin.joint_names
         == std::vector<std::string> (
             { "joint_0", "joint_1", "joint_2", "joint_3" }));
  REQUIRE (skin.inverse_binds.size () == 4);
  CHECK (skin.inverse_binds[1][3][0] == doctest::Approx (5.0F));
  CHECK (cpu->skins[1].inverse_binds[2] == glm::mat4 (1.0F));

  const auto *skinned_node = find_skinned_node (cpu->scenes[0].roots[0]);
  REQUIRE (skinned_node
           == nullptr); // The first root is the skeleton, not a mesh.
  skinned_node = find_skinned_node (cpu->scenes[0].roots[1]);
  REQUIRE (skinned_node != nullptr);
  CHECK (skinned_node->skin_index == 0);
  REQUIRE (skinned_node->mesh_lods.size () == 1);

  const auto &primitives = cpu->meshes[0].primitives;
  for (std::size_t primitive_index = 0; primitive_index < primitives.size ();
       ++primitive_index) {
    const auto &vertices = primitives[primitive_index].vertices;
    REQUIRE (vertices.size () == 3);
    check_joint (vertices[0].joints, { 0, 3, 1, 2 });
    check_joint (vertices[1].joints, { 1, 2, 3, 0 });
    check_joint (vertices[2].joints, { 2, 1, 0, 3 });
  }

  check_weight (primitives[0].vertices[0].weights, { 1.0F, 0.0F, 0.0F, 0.0F });
  check_weight (primitives[0].vertices[1].weights,
                { 0.0F, 127.0F / 255.0F, 128.0F / 255.0F, 0.0F });
  check_weight (primitives[1].vertices[1].weights,
                { 0.0F, 32767.0F / 65535.0F, 32768.0F / 65535.0F, 0.0F });
  check_weight (primitives[2].vertices[1].weights,
                { 0.0F, 0.75F, 0.25F, 0.0F });
}

TEST_CASE ("model upload carries skin data without a GPU context")
{
  temporary_directory temporary;
  const std::shared_ptr<wsl::rsc::raw::cpu_model> cpu
      = load_skin_component_fixture (temporary);
  REQUIRE (cpu != nullptr);

  wsl::rsc::model_loader loader{ nullptr };
  auto session = loader.begin_upload (*cpu);
  while (!wsl::rsc::model_loader::is_upload_complete (session)) {
    loader.upload_next_batch (session, *cpu, session.tasks.size ());
  }
  wsl::gfx::model_3d model
      = wsl::rsc::model_loader::finish_upload (session, *cpu);

  REQUIRE (model.skins.size () == 2);
  CHECK (model.skins[0].joint_count () == 4);
  CHECK (model.skins[0].joint_names[3] == "joint_3");
  CHECK (model.skins[0].inverse_binds[1][3][0] == doctest::Approx (5.0F));
  REQUIRE (model.meshes[0].primitives.size () == 4);

  const wsl::gfx::vertex &vertex = model.meshes[0].primitives[2].vertices[1];
  check_joint (vertex.joints, { 1, 2, 3, 0 });
  check_weight (vertex.weights, { 0.0F, 0.75F, 0.25F, 0.0F });

  const wsl::gfx::node *skinned_node
      = find_skinned_node (model.scenes[0].roots[0]);
  REQUIRE (skinned_node == nullptr);
  skinned_node = find_skinned_node (model.scenes[0].roots[1]);
  REQUIRE (skinned_node != nullptr);
  CHECK (skinned_node->skin_index == 0);
}

TEST_CASE ("model loader imports the rigged Fox skin")
{
  const std::string path = std::string (WEASEL_SOURCE_DIR)
                           + "/examples/animation/rsc/models/Fox.glb";
  wsl::rsc::model_loader loader{ nullptr };
  const std::shared_ptr<wsl::rsc::raw::cpu_model> cpu = loader.load_cpu (path);
  REQUIRE (cpu != nullptr);
  REQUIRE (!cpu->skins.empty ());

  const wsl::rsc::raw::cpu_skin &skin = cpu->skins[0];
  CHECK (!skin.joint_nodes.empty ());
  CHECK (skin.joint_nodes.size () == skin.joint_names.size ());
  CHECK (skin.joint_nodes.size () == skin.inverse_binds.size ());
  CHECK (std::any_of (skin.joint_names.begin (), skin.joint_names.end (),
                      [] (const std::string &name) { return !name.empty (); }));

  const wsl::rsc::raw::cpu_node *skinned_node = nullptr;
  for (const wsl::rsc::raw::cpu_scene &scene : cpu->scenes) {
    for (const wsl::rsc::raw::cpu_node &root : scene.roots) {
      skinned_node = find_skinned_node (root);
      if (skinned_node != nullptr) {
        break;
      }
    }
    if (skinned_node != nullptr) {
      break;
    }
  }
  REQUIRE (skinned_node != nullptr);
  REQUIRE (skinned_node->skin_index >= 0);
  REQUIRE (!skinned_node->mesh_lods.empty ());

  const std::size_t joint_count
      = cpu->skins[static_cast<std::size_t> (skinned_node->skin_index)]
            .joint_nodes.size ();
  bool found_influence = false;
  for (const int mesh_index : skinned_node->mesh_lods) {
    REQUIRE (mesh_index >= 0);
    for (const wsl::rsc::raw::cpu_primitive &primitive :
         cpu->meshes[static_cast<std::size_t> (mesh_index)].primitives) {
      for (const wsl::rsc::raw::cpu_vertex &vertex : primitive.vertices) {
        CHECK (vertex.joints.x < joint_count);
        CHECK (vertex.joints.y < joint_count);
        CHECK (vertex.joints.z < joint_count);
        CHECK (vertex.joints.w < joint_count);
        found_influence = found_influence || (vertex.weights.x > 0.0F);
      }
    }
  }
  CHECK (found_influence);
}
