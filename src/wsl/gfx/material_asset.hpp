#pragma once

#include "shader_program.hpp"
#include "wsl/rsc/resource_ids.hpp"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>

namespace wsl
{

namespace gfx
{

/**
 * Strongly-typed material parameter value.
 *
 * Supports scalars, vectors, and texture references.
 */
struct material_parameter
{
  using value_type = std::variant<float, glm::vec2, glm::vec3, glm::vec4, int,
                                  bool, rsc::image_id, rsc::cubemap_id>;

  value_type value;
  std::string name;

  material_parameter () = default;
  explicit material_parameter (const std::string &n, value_type v)
      : value (v), name (n)
  {
  }

};

/**
 * A shared material template defining a shader program and default
 * parameters.
 *
 * Multiple material_instances can reference one material_asset with different
 * overrides.
 */
struct material_asset
{
  rsc::material_id id{};
  std::string name;
  std::string path; // Path to the .wslmat file, if persisted.

  rsc::shader_program_id shader_program{};

  /**
   * Path to the vertex shader variant used with this material.
   * Default: "engine://compiled_shaders/cube.vert.slang.spv"
   */
  std::string vertex_shader_path
      = "engine://compiled_shaders/cube.vert.slang.spv";

  /** Default parameters defined by the asset (populated from the shader graph). */
  std::unordered_map<std::string, material_parameter> default_parameters;

  /** Metadata: is this material double-sided? */
  bool double_sided = false;

  /** Metadata: does this material use alpha test / opacity mask? */
  bool alpha_test = false;

};

/**
 * A lightweight per-mesh override layer on top of a material_asset.
 *
 * This is what `mesh::primitive` holds at runtime.
 */
struct material_instance
{
  rsc::material_id asset_id{};

  /**
   * Per-instance parameter overrides (sparse — missing keys fall back to asset
   * defaults).
   */
  std::unordered_map<std::string, material_parameter> overrides;

  /**
 * Build a uniform buffer blob matching the shader reflection layout.
 *
 *  Looks up parameter values from instance overrides then asset defaults.
 */
  std::vector<uint8_t> build_uniform_blob (const shader_reflection &reflection,
                                           const material_asset &asset) const;

  /** Get effective parameter value (instance override or asset default). */
  material_parameter::value_type
  get_parameter (const std::string &name, const material_asset &asset) const;
};

} // namespace gfx

} // namespace wsl
