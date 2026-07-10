#include "shader_graph_codegen.hpp"
#include "wsl/log/log.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_set>
#include <vector>

namespace wsl
{

namespace gfx
{

shader_graph_codegen::shader_graph_codegen (const shader_graph &graph)
    : m_graph (graph)
{
}

std::string
shader_graph_codegen::type_name (graph_pin_type t) const
{
  switch (t) {
  case graph_pin_type::float_scalar:
    return "float";
  case graph_pin_type::float2:
    return "float2";
  case graph_pin_type::float3:
    return "float3";
  case graph_pin_type::float4:
    return "float4";
  case graph_pin_type::int_scalar:
    return "int";
  case graph_pin_type::bool_scalar:
    return "bool";
  case graph_pin_type::mat4:
    return "float4x4";
  default:
    return "float";
  }
}

std::string
shader_graph_codegen::swizzle_for (graph_pin_type from, graph_pin_type to) const
{
  if (from == to) {
    return "";
  }
  if (to == graph_pin_type::float_scalar) {
    if (from == graph_pin_type::float2) {
      return ".x";
    }
    if (from == graph_pin_type::float3) {
      return ".x";
    }
    if (from == graph_pin_type::float4) {
      return ".x";
    }
  }
  if (to == graph_pin_type::float3 && from == graph_pin_type::float4) {
    return ".xyz";
  }
  if (to == graph_pin_type::float2 && from == graph_pin_type::float4) {
    return ".xy";
  }
  if (to == graph_pin_type::float2 && from == graph_pin_type::float3) {
    return ".xy";
  }
  return "";
}

std::string
shader_graph_codegen::next_temp () const
{
  return "_t" + std::to_string (m_temp_counter++);
}

bool
shader_graph_codegen::uses_model_pbr_inputs () const
{
  for (const auto &node : m_graph.nodes) {
    switch (node.kind) {
    case graph_node_kind::model_pbr_albedo:
    case graph_node_kind::model_pbr_metallic:
    case graph_node_kind::model_pbr_roughness:
    case graph_node_kind::model_pbr_normal:
    case graph_node_kind::model_pbr_emissive:
      return true;
    default:
      break;
    }
  }
  return false;
}

std::vector<std::string>
shader_graph_codegen::collect_texture_names () const
{
  std::vector<std::string> names;
  std::unordered_set<std::string> seen;

  auto push_unique = [&] (const std::string &name) {
    if (!name.empty () && seen.insert (name).second) {
      names.push_back (name);
    }
  };

  if (uses_model_pbr_inputs ()) {
    push_unique ("u_BaseColorTex");
    push_unique ("u_MetallicRoughnessTex");
    push_unique ("u_NormalTex");
    push_unique ("u_EmissiveTex");
  }

  for (const auto &node : m_graph.nodes) {
    if (node.kind != graph_node_kind::uniform_texture2d) {
      continue;
    }
    auto it = node.properties.find ("name");
    push_unique ((it != node.properties.end ()) ? it->second : "u_Tex2D");
  }

  return names;
}

int
shader_graph_codegen::texture_sampler_slot (const std::string &name) const
{
  auto it = m_texture_sampler_index.find (name);
  if (it != m_texture_sampler_index.end ()) {
    return it->second;
  }
  return 0;
}

std::string
shader_graph_codegen::eval_pin (uint64_t node_id, uint64_t pin_id) const
{
  auto key = (node_id << 32) | pin_id;
  auto it = m_pin_expr.find (key);
  if (it != m_pin_expr.end ()) {
    return it->second;
  }

  const graph_node *node = m_graph.find_node (node_id);
  if (!node) {
    return "0.0";
  }

  // Evaluate upstream if this is an input pin
  const graph_pin *target_pin = nullptr;
  for (const auto &p : node->pins) {
    if (p.id == pin_id && p.is_input) {
      target_pin = &p;
      break;
    }
  }

  if (target_pin) {
    const graph_link *link = m_graph.find_input_link (node_id, pin_id);
    if (link) {
      return eval_pin (link->from_node, link->from_pin);
    }
  }

  // Split exposes each vector channel as its own output pin, so the expression
  // depends on *which* output pin is being read, not just the node.
  if (node->kind == graph_node_kind::split_vector && !target_pin) {
    const graph_link *src = m_graph.find_input_link (node_id, node->pins[0].id);
    std::string val = src ? eval_pin (src->from_node, src->from_pin)
                          : "float4(0.0,0.0,0.0,0.0)";
    std::string comp = "x";
    for (const auto &p : node->pins) {
      if (!p.is_input && p.id == pin_id) {
        if (p.name == "X")
          comp = "x";
        else if (p.name == "Y")
          comp = "y";
        else if (p.name == "Z")
          comp = "z";
        else if (p.name == "W")
          comp = "w";
      }
    }
    std::string expr = val + "." + comp;
    m_pin_expr[key] = expr;
    return expr;
  }

  // Evaluate the node itself to produce the expression
  std::string expr = eval_node (*node);
  m_pin_expr[key] = expr;
  return expr;
}

std::string
shader_graph_codegen::eval_node (const graph_node &node) const
{
  if (m_visited.count (node.id)) {
    // Already visited, return cached result for first output pin
    for (const auto &p : node.pins) {
      if (!p.is_input) {
        auto key = (node.id << 32) | p.id;
        auto it = m_pin_expr.find (key);
        if (it != m_pin_expr.end ()) {
          return it->second;
        }
      }
    }
    return "0.0";
  }
  m_visited.insert (node.id);

  auto emit_binary = [&] (const char *op) {
    auto *a = m_graph.find_input_link (node.id, node.pins[0].id);
    auto *b = m_graph.find_input_link (node.id, node.pins[1].id);
    std::string lhs = a ? eval_pin (a->from_node, a->from_pin) : "0.0";
    std::string rhs = b ? eval_pin (b->from_node, b->from_pin) : "0.0";
    std::string tmp = next_temp ();
    m_main << "    " << type_name (node.pins.back ().type) << " " << tmp
           << " = " << lhs << " " << op << " " << rhs << ";\n";
    return tmp;
  };

  auto emit_unary = [&] (const char *func) {
    auto *a = m_graph.find_input_link (node.id, node.pins[0].id);
    std::string lhs = a ? eval_pin (a->from_node, a->from_pin) : "0.0";
    std::string tmp = next_temp ();
    m_main << "    " << type_name (node.pins.back ().type) << " " << tmp
           << " = " << func << "(" << lhs << ");\n";
    return tmp;
  };

  switch (node.kind) {
  case graph_node_kind::uv0:
    return "input.uv";
  case graph_node_kind::normal:
    return "normalize(input.normal)";
  case graph_node_kind::world_position:
    return "input.worldPos";
  case graph_node_kind::view_direction:
    return "normalize(u_CameraPos.xyz - input.worldPos)";
  case graph_node_kind::vertex_color:
    return "float4(1.0,1.0,1.0,1.0)"; // placeholder
  case graph_node_kind::time:
    return "u_Time";
  case graph_node_kind::camera_position:
    return "u_CameraPos.xyz";
  case graph_node_kind::model_pbr_albedo:
    return "u_BaseColorTex.SampleBias(u_Samplers["
           + std::to_string (texture_sampler_slot ("u_BaseColorTex"))
           + "], input.uv, u_MipLodBias) * u_BaseColorFactor";
  case graph_node_kind::model_pbr_metallic:
    return "u_MetallicRoughnessTex.SampleBias(u_Samplers["
           + std::to_string (texture_sampler_slot ("u_MetallicRoughnessTex"))
           + "], input.uv, u_MipLodBias).b * u_MetallicFactor";
  case graph_node_kind::model_pbr_roughness:
    return "clamp(u_MetallicRoughnessTex.SampleBias(u_Samplers["
           + std::to_string (texture_sampler_slot ("u_MetallicRoughnessTex"))
           + "], input.uv, u_MipLodBias).g * u_RoughnessFactor, 0.04, 1.0)";
  case graph_node_kind::model_pbr_normal:
    return "ApplyModelMaterialNormal(input.normal, input.tangent, input.uv)";
  case graph_node_kind::model_pbr_emissive:
    return "u_EmissiveTex.SampleBias(u_Samplers["
           + std::to_string (texture_sampler_slot ("u_EmissiveTex"))
           + "], input.uv, u_MipLodBias).rgb * u_EmissiveFactor";

  case graph_node_kind::uniform_float: {
    auto it = node.properties.find ("name");
    std::string name = (it != node.properties.end ()) ? it->second : "u_Float";
    return name;
  }
  case graph_node_kind::uniform_float3: {
    auto it = node.properties.find ("name");
    std::string name = (it != node.properties.end ()) ? it->second : "u_Float3";
    return name;
  }
  case graph_node_kind::uniform_float4: {
    auto it = node.properties.find ("name");
    std::string name = (it != node.properties.end ()) ? it->second : "u_Float4";
    return name;
  }

  case graph_node_kind::texture_sample_2d: {
    auto *uv_link = m_graph.find_input_link (node.id, node.pins[0].id);
    std::string uv = uv_link ? eval_pin (uv_link->from_node, uv_link->from_pin)
                             : "input.uv";
    // Find the connected texture uniform node
    std::string tex_name = "u_DefaultTex";
    auto *tex_link = m_graph.find_input_link (node.id, node.pins[1].id);
    if (tex_link) {
      const graph_node *tex_node = m_graph.find_node (tex_link->from_node);
      if (tex_node) {
        auto it = tex_node->properties.find ("name");
        if (it != tex_node->properties.end ()) {
          tex_name = it->second;
        }
      }
    }
    std::string tmp = next_temp ();
    int sampler_idx = texture_sampler_slot (tex_name);
    m_main << "    float4 " << tmp << " = " << tex_name << ".Sample(u_Samplers["
           << sampler_idx << "], " << uv << ");\n";
    return tmp;
  }

  case graph_node_kind::add:
    return emit_binary ("+");
  case graph_node_kind::multiply:
    return emit_binary ("*");
  case graph_node_kind::subtract:
    return emit_binary ("-");
  case graph_node_kind::divide:
    return emit_binary ("/");
  case graph_node_kind::lerp: {
    auto *a = m_graph.find_input_link (node.id, node.pins[0].id);
    auto *b = m_graph.find_input_link (node.id, node.pins[1].id);
    auto *t = m_graph.find_input_link (node.id, node.pins[2].id);
    std::string av = a ? eval_pin (a->from_node, a->from_pin) : "0.0";
    std::string bv = b ? eval_pin (b->from_node, b->from_pin) : "0.0";
    std::string tv = t ? eval_pin (t->from_node, t->from_pin) : "0.0";
    std::string tmp = next_temp ();
    m_main << "    " << type_name (node.pins.back ().type) << " " << tmp
           << " = lerp(" << av << ", " << bv << ", " << tv << ");\n";
    return tmp;
  }
  case graph_node_kind::clamp:
    return emit_unary ("saturate");
  case graph_node_kind::one_minus: {
    auto *a = m_graph.find_input_link (node.id, node.pins[0].id);
    std::string v = a ? eval_pin (a->from_node, a->from_pin) : "0.0";
    std::string tmp = next_temp ();
    m_main << "    " << type_name (node.pins.back ().type) << " " << tmp
           << " = 1.0 - " << v << ";\n";
    return tmp;
  }
  case graph_node_kind::normalize:
    return emit_unary ("normalize");
  case graph_node_kind::dot:
    return emit_binary ("dot");
  case graph_node_kind::cross:
    return emit_binary ("cross");

  case graph_node_kind::split_vector: {
    // Per-output handling lives in eval_pin(); this fallback is unreachable.
    return "0.0";
  }

  case graph_node_kind::combine_vector: {
    auto *x = m_graph.find_input_link (node.id, node.pins[0].id);
    auto *y = m_graph.find_input_link (node.id, node.pins[1].id);
    auto *z = m_graph.find_input_link (node.id, node.pins[2].id);
    auto *w = m_graph.find_input_link (node.id, node.pins[3].id);
    std::string sx = x ? eval_pin (x->from_node, x->from_pin) : "0.0";
    std::string sy = y ? eval_pin (y->from_node, y->from_pin) : "0.0";
    std::string sz = z ? eval_pin (z->from_node, z->from_pin) : "0.0";
    std::string sw = w ? eval_pin (w->from_node, w->from_pin) : "0.0";
    std::string tmp = next_temp ();
    m_main << "    float4 " << tmp << " = float4(" << sx << ", " << sy << ", "
           << sz << ", " << sw << ");\n";
    return tmp;
  }

  case graph_node_kind::material_output: {
    // Aggregate all connected inputs into PSOut
    std::string albedo = "float4(1.0,0.0,0.0,1.0)";
    std::string emissive = "float3(0.0,0.0,0.0)";
    std::string metallic = "0.0";
    std::string roughness = "0.5";
    std::string normal = "normalize(input.normal)";

    for (const auto &pin : node.pins) {
      if (pin.is_input && pin.name == "Albedo") {
        auto *link = m_graph.find_input_link (node.id, pin.id);
        if (link) {
          albedo = "float4(" + eval_pin (link->from_node, link->from_pin) + ")";
        }
      }
      if (pin.is_input && pin.name == "Emissive") {
        auto *link = m_graph.find_input_link (node.id, pin.id);
        if (link) {
          emissive = "float3(" + eval_pin (link->from_node, link->from_pin)
                     + ".xyz)";
        }
      }
      if (pin.is_input && pin.name == "Metallic") {
        auto *link = m_graph.find_input_link (node.id, pin.id);
        if (link) {
          metallic
              = "float(" + eval_pin (link->from_node, link->from_pin) + ".x)";
        }
      }
      if (pin.is_input && pin.name == "Roughness") {
        auto *link = m_graph.find_input_link (node.id, pin.id);
        if (link) {
          roughness
              = "float(" + eval_pin (link->from_node, link->from_pin) + ".x)";
        }
      }
      if (pin.is_input && pin.name == "Normal") {
        auto *link = m_graph.find_input_link (node.id, pin.id);
        if (link) {
          normal = "float3(" + eval_pin (link->from_node, link->from_pin)
                   + ".xyz)";
        }
      }
    }

    m_main << "    float4 albedo = " << albedo << ";\n";
    m_main << "    float3 emissive = " << emissive << ";\n";
    m_main << "    float metallic = " << metallic << ";\n";
    m_main << "    float roughness = " << roughness << ";\n";
    m_main << "    float3 Ng = normalize(input.normal);\n";
    m_main << "    float3 N = " << normal << ";\n";
    m_main << "    float3 V = normalize(u_CameraPos.xyz - input.worldPos);\n";
    m_main << "    SceneResult _pbr = EvaluateScenePBR(albedo.rgb, metallic, "
              "roughness, emissive, N, Ng, V, input.worldPos, input.uv, "
              "input.pos, input.viewPos);\n";
    m_main << "    PSOut o;\n";
    m_main << "    o.color = float4(_pbr.color, albedo.a);\n";
    m_main << "    o.bloom = float4(_pbr.bloom, 1.0);\n";
    m_main << "    return o;\n";
    return "";
  }

  default:
    return "0.0";
  }
}

void
shader_graph_codegen::emit_uniforms () const
{
  static constexpr std::array<const char *, 6> model_pbr_uniforms
      = { "u_BaseColorFactor", "u_MetallicFactor", "u_RoughnessFactor",
          "u_EmissiveFactor", "u_MipLodBias", "u_Time" };

  auto is_reserved_name = [&] (const std::string &name) {
    for (const char *reserved : model_pbr_uniforms) {
      if (name == reserved) {
        return true;
      }
    }
    return false;
  };

  m_globals << "cbuffer Material : register(b0, space3)\n{\n";
  m_globals << "    float u_Time;\n";
  m_globals << "    float3 _pad_time;\n";
  if (uses_model_pbr_inputs ()) {
    m_globals << "    float4 u_BaseColorFactor;\n";
    m_globals << "    float u_MetallicFactor;\n";
    m_globals << "    float u_RoughnessFactor;\n";
    m_globals << "    float2 _pad_model_material0;\n";
    m_globals << "    float3 u_EmissiveFactor;\n";
    m_globals << "    float u_MipLodBias;\n";
  }

  for (const auto &node : m_graph.nodes) {
    if (node.kind == graph_node_kind::uniform_float) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float";
      if (is_reserved_name (name)) {
        continue;
      }
      m_globals << "    float " << name << ";\n";
    } else if (node.kind == graph_node_kind::uniform_float3) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float3";
      if (is_reserved_name (name)) {
        continue;
      }
      m_globals << "    float3 " << name << "; float _pad_" << name << ";\n";
    } else if (node.kind == graph_node_kind::uniform_float4) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float4";
      if (is_reserved_name (name)) {
        continue;
      }
      m_globals << "    float4 " << name << ";\n";
    }
  }
  m_globals << "};\n\n";
}

void
shader_graph_codegen::emit_textures () const
{
  // Graph-declared Texture2D uniforms (including built-in model PBR inputs)
  // occupy the t0..t3 range, which pbr_common.slang leaves free (it uses
  // t4..t16 for IBL, shadow maps and the clustered lighting storage buffers).
  // The shared `u_Samplers[15]` array is declared by pbr_common.slang, so we
  // must NOT redeclare it here.
  m_texture_sampler_index.clear ();

  for (const auto &name : collect_texture_names ()) {
    m_texture_sampler_index[name] = m_tex_binding;
    m_globals << "Texture2D " << name << " : register(t" << m_tex_binding++
              << ", space2);\n";
  }
}

void
shader_graph_codegen::emit_helpers () const
{
  if (uses_model_pbr_inputs ()) {
    m_globals << "\n";
    m_globals << "float3 ApplyModelMaterialNormal(float3 Ngeom, float4 tangent, "
                 "float2 uv)\n";
    m_globals << "{\n";
    m_globals << "    float3 T = normalize(tangent.xyz);\n";
    m_globals << "    T = normalize(T - Ngeom * dot(Ngeom, T));\n";
    m_globals << "    float3 B = normalize(cross(Ngeom, T) * tangent.w);\n";
    m_globals << "    float3 nTS = UnpackNormal(u_NormalTex.SampleBias("
              << "u_Samplers["
              << texture_sampler_slot ("u_NormalTex")
              << "], uv, u_MipLodBias).xyz);\n";
    m_globals << "    float3x3 TBN = float3x3(T, B, Ngeom);\n";
    m_globals << "    return normalize(mul(nTS, TBN));\n";
    m_globals << "}\n\n";
  }
}

std::string
shader_graph_codegen::generate () const
{
  m_pin_expr.clear ();
  m_visited.clear ();
  m_globals.str ("");
  m_main.str ("");
  m_temp_counter = 0;
  m_tex_binding = 0;
  m_texture_sampler_index.clear ();

  const graph_node *output = m_graph.find_output_node ();
  if (!output) {
    wsl::log::gfx ()->error ("shader_graph_codegen: no output node found");
    return "";
  }

  emit_uniforms ();
  emit_textures ();
  emit_helpers ();

  m_main << "[shader(\"fragment\")]\n";
  m_main << "PSOut fsMain(PSInput input)\n{\n";

  eval_node (*output);

  m_main << "}\n";

  std::ostringstream full;
  full << "// Generated by Weasel Shader Graph\n\n";
  // Inline the shared PBR module so the graph material is lit with the exact
  // same engine PBR as the standard cube.frag material.
  if (!m_pbr_common_source.empty ()) {
    full << m_pbr_common_source << "\n";
  }
  full << "struct PSInput\n{\n";
  full << "    float4 pos      : SV_Position;\n";
  full << "    float3 worldPos : TEXCOORD0;\n";
  full << "    float3 normal   : TEXCOORD1;\n";
  full << "    float2 uv       : TEXCOORD2;\n";
  full << "    float4 tangent  : TEXCOORD3;\n";
  full << "    float3 viewPos  : TEXCOORD4;\n";
  full << "};\n\n";
  // PSOut and SceneResult are provided by pbr_common.slang.
  full << m_globals.str ();
  full << m_main.str ();

  return full.str ();
}

shader_reflection
shader_graph_codegen::build_reflection () const
{
  shader_reflection refl;
  // Populate reflection to match what generate() emits. When the PBR module is
  // inlined, the shader declares five uniform buffers (Material + the four the
  // shared module owns) and a fixed 15-slot sampler array.
  auto add_ubo = [&] (const char *name, uint32_t binding, uint32_t size) {
    shader_uniform_buffer ub{};
    ub.name = name;
    ub.binding = binding;
    ub.set = 3;
    ub.size = size;
    refl.uniform_buffers.push_back (std::move (ub));
  };

  add_ubo ("Material", 0, 64);
  add_ubo ("Lighting", 1, 512);
  add_ubo ("IBL", 2, 16);
  add_ubo ("Post", 3, 32);
  add_ubo ("ClusterParams", 4, 256);

  int tex_count = 0;
  for (const auto &name : collect_texture_names ()) {
    shader_texture_binding tb;
    tb.name = name;
    tb.binding = tex_count++;
    tb.set = 2;
    tb.is_cube = false;
    refl.textures.push_back (std::move (tb));
  }

  // The shared PBR module always declares u_Samplers[15].
  refl.sampler_count = 15;

  return refl;
}

} // namespace gfx

} // namespace wsl
