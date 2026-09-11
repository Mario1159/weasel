#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace gfx
{

/** Supported pin data types in the shader graph. */
enum class graph_pin_type
{
  none,
  float_scalar,
  float2,
  float3,
  float4,
  int_scalar,
  bool_scalar,
  texture2d,
  texturecube,
  mat4
};

/** A typed input or output on a graph node. */
struct graph_pin
{
  uint64_t id = 0;
  std::string name;
  graph_pin_type type = graph_pin_type::none;
  bool is_input = true;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("id", id), cereal::make_nvp ("name", name),
        cereal::make_nvp ("type", type),
        cereal::make_nvp ("is_input", is_input));
  }
};

/** A connection between two pins. */
struct graph_link
{
  uint64_t from_node = 0;
  uint64_t from_pin = 0;
  uint64_t to_node = 0;
  uint64_t to_pin = 0;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("from_node", from_node),
        cereal::make_nvp ("from_pin", from_pin),
        cereal::make_nvp ("to_node", to_node),
        cereal::make_nvp ("to_pin", to_pin));
  }
};

/** Kinds of built-in shader graph nodes. */
enum class graph_node_kind
{
  unknown,
  // Inputs
  vertex_color,
  uv0,
  normal,
  world_position,
  view_direction,
  // Textures
  texture_sample_2d,
  texture_sample_cube,
  // Math
  add,
  multiply,
  subtract,
  divide,
  lerp,
  clamp,
  one_minus,
  normalize,
  dot,
  cross,
  // Vector
  split_vector,
  combine_vector,
  swizzle,
  // PBR output
  material_output,
  // Uniforms
  uniform_float,
  uniform_float3,
  uniform_float4,
  uniform_texture2d,
  uniform_texturecube,
  // Utility
  time,
  camera_position,
  // Model material PBR inputs
  model_pbr_albedo,
  model_pbr_metallic,
  model_pbr_roughness,
  model_pbr_normal,
  model_pbr_emissive
};

/** A single node in the shader graph. */
struct graph_node
{
  uint64_t id = 0;
  std::string name;
  graph_node_kind kind = graph_node_kind::unknown;
  float pos_x = 0.0F;
  float pos_y = 0.0F;

  /** Node-specific properties (e.g., default value for a uniform). */
  std::unordered_map<std::string, std::string> properties;

  /** Pins exposed by this node. */
  std::vector<graph_pin> pins;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("id", id), cereal::make_nvp ("name", name),
        cereal::make_nvp ("kind", kind), cereal::make_nvp ("pos_x", pos_x),
        cereal::make_nvp ("pos_y", pos_y),
        cereal::make_nvp ("properties", properties),
        cereal::make_nvp ("pins", pins));
  }
};

/**
 * A node-based shader graph that can be serialized and compiled to
 * Slang.
 */
struct shader_graph
{
  std::string name;
  std::vector<graph_node> nodes;
  std::vector<graph_link> links;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("name", name), cereal::make_nvp ("nodes", nodes),
        cereal::make_nvp ("links", links));
  }

  /** Find a node by id, or nullptr. */
  const graph_node *find_node (uint64_t id) const;
  graph_node *find_node (uint64_t id);

  /** Find the unique material output node, or nullptr. */
  const graph_node *find_output_node () const;

  /** Find the link that drives a given target pin, or nullptr. */
  const graph_link *find_input_link (uint64_t to_node, uint64_t to_pin) const;

  /** List all nodes connected to an output pin (used for traversal). */
  std::vector<const graph_link *> find_output_links (uint64_t from_node,
                                                     uint64_t from_pin) const;
};

} // namespace gfx

} // namespace wsl
