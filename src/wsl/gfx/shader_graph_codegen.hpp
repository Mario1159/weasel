#pragma once

#include "shader_graph.hpp"
#include "shader_program.hpp"
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wsl
{

namespace gfx
{

/*!
 * \brief Generates Slang fragment shader source from a shader_graph.
 *
 * Emits a complete fragment shader that conforms to the engine's binding
 * conventions:
 *   - Material UBO at b0, space3
 *   - Textures at tN, space2
 *   - Samplers at sN, space2
 */
class shader_graph_codegen
{
public:
  explicit shader_graph_codegen (const shader_graph &graph);

  /*! \brief Generate the complete fragment shader source. */
  std::string generate () const;

  /*! \brief Extract reflection metadata from the generated bindings. */
  shader_reflection build_reflection () const;

  /*! \brief Supply the shared PBR module source (pbr_common.slang).
   *
   *  When set, the generated fragment shader includes the engine's full PBR
   *  lighting (directional / clustered point / spot lights, IBL, shadows,
   *  SSAO, bloom) so shader-graph materials are lit identically to the
   *  standard cube.frag material. The source is inlined at the top of the
   *  generated shader so no runtime include path is required.
   */
  void
  set_pbr_common_source (const std::string &source)
  {
    m_pbr_common_source = source;
  }

private:
  const shader_graph &m_graph;

  std::string m_pbr_common_source;

  mutable std::unordered_map<uint64_t, std::string>
      m_pin_expr; // pin id -> Slang expr
  mutable std::unordered_set<uint64_t> m_visited;
  mutable std::ostringstream m_globals;
  mutable std::ostringstream m_main;
  mutable int m_temp_counter = 0;
  mutable int m_tex_binding = 0;
  mutable std::unordered_map<std::string, int> m_texture_sampler_index;

  std::string eval_pin (uint64_t node_id, uint64_t pin_id) const;
  std::string eval_node (const graph_node &node) const;

  std::string type_name (graph_pin_type t) const;
  std::string swizzle_for (graph_pin_type from, graph_pin_type to) const;
  std::string next_temp () const;
  bool uses_model_pbr_inputs () const;
  std::vector<std::string> collect_texture_names () const;
  int texture_sampler_slot (const std::string &name) const;

  void emit_uniforms () const;
  void emit_textures () const;
  void emit_helpers () const;
};

} // namespace gfx

} // namespace wsl
