#pragma once

#include "shader_program.hpp"
#include "wsl/rsc/resource_ids.hpp"
#include "wsl/rsc/cereal_glm.hpp"

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/variant.hpp>
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

/*!
 * \brief Strongly-typed material parameter value.
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

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("name", name), cereal::make_nvp ("value", value));
  }
};

/*!
 * \brief A shared material template defining a shader program and default
 * parameters.
 *
 * Multiple material_instances can reference one material_asset with different
 * overrides.
 */
struct material_asset
{
  rsc::material_id id{};
  std::string name;
  std::string path; //!< Path to the .wslmat file, if persisted.

  rsc::shader_program_id shader_program{};

  //! Path to the vertex shader variant used with this material.
  //! Default: "engine://compiled_shaders/cube.vert.slang.spv"
  std::string vertex_shader_path
      = "engine://compiled_shaders/cube.vert.slang.spv";

  //! Default parameters defined by the asset (populated from the shader graph).
  std::unordered_map<std::string, material_parameter> default_parameters;

  //! Metadata: is this material double-sided?
  bool double_sided = false;

  //! Metadata: does this material use alpha test / opacity mask?
  bool alpha_test = false;

  template <class Archive>
  void
  serialize (Archive &ar)
  {
    ar (cereal::make_nvp ("name", name), cereal::make_nvp ("path", path),
        cereal::make_nvp ("shader_program", shader_program.value),
        cereal::make_nvp ("vertex_shader_path", vertex_shader_path),
        cereal::make_nvp ("double_sided", double_sided),
        cereal::make_nvp ("alpha_test", alpha_test),
        cereal::make_nvp ("default_parameters", default_parameters));
  }
};

/*!
 * \brief A lightweight per-mesh override layer on top of a material_asset.
 *
 * This is what `mesh::primitive` holds at runtime.
 */
struct material_instance
{
  rsc::material_id asset_id{};

  //! Per-instance parameter overrides (sparse — missing keys fall back to asset
  //! defaults).
  std::unordered_map<std::string, material_parameter> overrides;

  /*! \brief Build a uniform buffer blob matching the shader reflection layout.
   *
   *  Looks up parameter values from instance overrides then asset defaults.
   */
  std::vector<uint8_t> build_uniform_blob (const shader_reflection &reflection,
                                           const material_asset &asset) const;

  /*! \brief Get effective parameter value (instance override or asset default).
   */
  material_parameter::value_type
  get_parameter (const std::string &name, const material_asset &asset) const;
};

} // namespace gfx

} // namespace wsl
