#include "shader_graph_editor.hpp"

#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/gfx/material_asset.hpp"
#include "wsl/gfx/shader_program.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include "wsl/log/log.hpp"

#include <ImNodeFlow.h>
#include <cereal/archives/json.hpp>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace editor
{

// ---------------------------------------------------------------------------
// Internal wrapper: bridge between gfx::graph_node and ImFlow::BaseNode
// ---------------------------------------------------------------------------

// Short human-readable label for a pin's data type (e.g. "float2"). Shown next
// to every socket so users can see what each input/output carries.
static const char *
pin_type_label (wsl::gfx::graph_pin_type t)
{
  using namespace wsl::gfx;
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
  case graph_pin_type::texture2d:
    return "tex2D";
  case graph_pin_type::texturecube:
    return "texCube";
  case graph_pin_type::mat4:
    return "mat4";
  default:
    return "";
  }
}

// Every data type is drawn with a fixed socket colour so the graph is easy to
// read at a glance:
//   single value  -> red
//   float2 tuple  -> green
//   float3 tuple  -> blue
//   float4 tuple  -> yellow
//   textures      -> cyan
//   int/bool/mat  -> brown / grey
static std::shared_ptr<ImFlow::PinStyle>
pin_style_for (wsl::gfx::graph_pin_type t)
{
  using namespace wsl::gfx;
  ImU32 color = IM_COL32 (255, 255, 255, 255);
  switch (t) {
  case graph_pin_type::float_scalar:
    color = IM_COL32 (191, 90, 90, 255); // red
    break;
  case graph_pin_type::float2:
    color = IM_COL32 (90, 191, 93, 255); // green
    break;
  case graph_pin_type::float3:
    color = IM_COL32 (90, 117, 191, 255); // blue
    break;
  case graph_pin_type::float4:
    color = IM_COL32 (205, 170, 70, 255); // yellow
    break;
  case graph_pin_type::texture2d:
  case graph_pin_type::texturecube:
    color = IM_COL32 (87, 155, 185, 255); // cyan
    break;
  case graph_pin_type::int_scalar:
  case graph_pin_type::bool_scalar:
    color = IM_COL32 (191, 134, 90, 255); // brown
    break;
  case graph_pin_type::mat4:
    color = IM_COL32 (200, 200, 200, 255); // grey
    break;
  default:
    break;
  }
  return std::make_shared<ImFlow::PinStyle> (
      ImFlow::PinStyle (color, 0, 4.f, 4.67f, 3.7f, 1.f));
}

class GraphNodeWrapper : public ImFlow::BaseNode
{
public:
  GraphNodeWrapper (wsl::gfx::shader_graph *graph, uint64_t node_id,
                    wsl::comp::singl::runtime_context *runtime_ctx)
      : m_graph (graph), m_node_id (node_id), m_runtime_ctx (runtime_ctx)
  {
    auto *n = node ();
    if (!n)
      return;
    setTitle (n->name.empty () ? "Node" : n->name);

    for (const auto &pin : n->pins) {
      // The display name carries the data-type label (e.g. "UV  float2") while
      // the uid stays the stable model pin id, so socket matching in the sync
      // helpers can rely on the uid rather than the human-readable name.
      std::string label = pin.name;
      const char *tl = pin_type_label (pin.type);
      if (tl && tl[0] != '\0') {
        label += "  ";
        label += tl;
      }
      auto style = pin_style_for (pin.type);
      if (pin.is_input) {
        addIN_uid<float> (pin.id, label, 0.0f,
                          ImFlow::ConnectionFilter::SameType (), style);
      } else {
        addOUT_uid<float> (pin.id, label, style);
      }
    }
  }

  void
  draw () override
  {
    auto *n = node ();
    if (!n)
      return;

    using namespace wsl::gfx;

    switch (n->kind) {
    case graph_node_kind::uniform_float: {
      auto it = n->properties.find ("default");
      float val = 0.0F;
      if (it != n->properties.end ()) {
        val = std::stof (it->second);
      }
      ImGui::SetNextItemWidth (120.0F);
      if (ImGui::DragFloat ("Value", &val, 0.01F)) {
        if (it != n->properties.end ()) {
          it->second = std::to_string (val);
        }
      }
      break;
    }
    case graph_node_kind::uniform_float3: {
      auto it = n->properties.find ("default");
      glm::vec3 val (1.0f);
      if (it != n->properties.end ()) {
        std::sscanf (it->second.c_str (), "%f,%f,%f", &val.x, &val.y, &val.z);
      }
      ImGui::SetNextItemWidth (180.0F);
      if (ImGui::ColorEdit3 ("Value", glm::value_ptr (val))) {
        if (it != n->properties.end ()) {
          it->second = std::to_string (val.x) + "," + std::to_string (val.y)
                       + "," + std::to_string (val.z);
        }
      }
      break;
    }
    case graph_node_kind::uniform_float4: {
      auto it = n->properties.find ("default");
      glm::vec4 val (1.0f);
      if (it != n->properties.end ()) {
        std::sscanf (it->second.c_str (), "%f,%f,%f,%f", &val.x, &val.y, &val.z,
                     &val.w);
      }
      ImGui::SetNextItemWidth (180.0F);
      if (ImGui::ColorEdit4 ("Value", glm::value_ptr (val))) {
        if (it != n->properties.end ()) {
          it->second = std::to_string (val.x) + "," + std::to_string (val.y)
                       + "," + std::to_string (val.z) + ","
                       + std::to_string (val.w);
        }
      }
      break;
    }
    case graph_node_kind::uniform_texture2d: {
      auto name_it = n->properties.find ("name");
      const std::string uniform_name
          = (name_it != n->properties.end ()) ? name_it->second : "u_Tex2D";
      ImGui::TextDisabled ("Uniform: %s", uniform_name.c_str ());

      if (m_runtime_ctx == nullptr) {
        ImGui::TextDisabled ("No runtime context");
        break;
      }

      auto &res_mgr = m_runtime_ctx->resource_manager ();
      entt::id_type selected_id = entt::null;
      if (auto it = n->properties.find ("image_id");
          it != n->properties.end () && !it->second.empty ()) {
        char *end_ptr = nullptr;
        unsigned long long parsed
            = std::strtoull (it->second.c_str (), &end_ptr, 10);
        if (end_ptr != it->second.c_str () && *end_ptr == '\0') {
          selected_id = static_cast<entt::id_type> (parsed);
        }
      }

      std::string preview = "None";
      if (selected_id != entt::null) {
        if (auto info = res_mgr.info (wsl::rsc::image_id{ selected_id })) {
          preview = info->name + " (" + info->path + ")";
        } else {
          preview = "(" + std::to_string (selected_id) + ")";
        }
      }

      if (ImGui::BeginCombo ("Texture", preview.c_str ())) {
        bool none_selected = (selected_id == entt::null);
        if (ImGui::Selectable ("None", none_selected)) {
          n->properties.erase ("image_id");
        }
        if (none_selected) {
          ImGui::SetItemDefaultFocus ();
        }

        for (const auto &image_info : res_mgr.list_images ()) {
          bool const is_selected = (image_info.id == selected_id);
          std::string item = image_info.name + " (" + image_info.path + ")";
          if (ImGui::Selectable (item.c_str (), is_selected)) {
            n->properties["image_id"] = std::to_string (image_info.id);
            res_mgr.load (wsl::rsc::image_id{ image_info.id });
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus ();
          }
        }
        ImGui::EndCombo ();
      }
      break;
    }
    default:
      for (auto &prop : n->properties) {
        char buf[256];
        std::snprintf (buf, sizeof (buf), "%s", prop.second.c_str ());
        ImGui::SetNextItemWidth (120.0F);
        if (ImGui::InputText (prop.first.c_str (), buf, sizeof (buf))) {
          prop.second = buf;
        }
      }
      break;
    }
  }

  wsl::gfx::graph_node *
  node ()
  {
    return m_graph ? m_graph->find_node (m_node_id) : nullptr;
  }

  uint64_t
  node_id () const
  {
    return m_node_id;
  }

private:
  wsl::gfx::shader_graph *m_graph;
  uint64_t m_node_id;
  wsl::comp::singl::runtime_context *m_runtime_ctx;
};

// ---------------------------------------------------------------------------
// Helpers for syncing ImNodeFlow <-> gfx::shader_graph
// ---------------------------------------------------------------------------

static ImFlow::BaseNode *
find_wrapper (ImFlow::ImNodeFlow *nf, uint64_t node_id)
{
  if (!nf)
    return nullptr;
  for (auto &kv : nf->getNodes ()) {
    auto *w = dynamic_cast<GraphNodeWrapper *> (kv.second.get ());
    if (w && w->node_id () == node_id)
      return kv.second.get ();
  }
  return nullptr;
}

static ImFlow::Pin *
find_pin_by_uid (ImFlow::BaseNode *bn, uint64_t uid, bool input)
{
  if (!bn)
    return nullptr;
  auto &pins = input ? bn->getIns () : bn->getOuts ();
  for (auto &p : pins)
    if (p->getUid () == uid)
      return p.get ();
  return nullptr;
}

// ---------------------------------------------------------------------------

shader_graph_editor::shader_graph_editor (
    wsl::comp::singl::runtime_context *runtime_ctx,
    wsl::comp::singl::editor_context *editor_ctx)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_codegen (std::make_unique<wsl::gfx::shader_graph_codegen> (m_graph)),
      m_compiler (std::make_unique<wsl::gfx::shader_compiler> ())
{
  m_nodeflow_handle = new ImFlow::ImNodeFlow ();
  new_graph ();
}

shader_graph_editor::~shader_graph_editor ()
{
  delete static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);
}

void
shader_graph_editor::new_graph ()
{
  m_graph = wsl::gfx::shader_graph{};
  m_graph.name = "Untitled";
  m_next_node_id = 2;
  build_default_graph ();
  m_current_path.clear ();
  m_compile_log.clear ();
  sync_graph_to_nodeflow ();
}

void
shader_graph_editor::build_default_graph ()
{
  wsl::gfx::graph_node output{};
  output.id = 1;
  output.name = "Material Output";
  output.kind = wsl::gfx::graph_node_kind::material_output;
  output.pos_x = 400.0F;
  output.pos_y = 200.0F;

  wsl::gfx::graph_pin albedo_pin{};
  albedo_pin.id = 101;
  albedo_pin.name = "Albedo";
  albedo_pin.type = wsl::gfx::graph_pin_type::float4;
  albedo_pin.is_input = true;
  output.pins.push_back (albedo_pin);

  wsl::gfx::graph_pin emissive_pin{};
  emissive_pin.id = 102;
  emissive_pin.name = "Emissive";
  emissive_pin.type = wsl::gfx::graph_pin_type::float3;
  emissive_pin.is_input = true;
  output.pins.push_back (emissive_pin);

  wsl::gfx::graph_pin metallic_pin{};
  metallic_pin.id = 103;
  metallic_pin.name = "Metallic";
  metallic_pin.type = wsl::gfx::graph_pin_type::float_scalar;
  metallic_pin.is_input = true;
  output.pins.push_back (metallic_pin);

  wsl::gfx::graph_pin roughness_pin{};
  roughness_pin.id = 104;
  roughness_pin.name = "Roughness";
  roughness_pin.type = wsl::gfx::graph_pin_type::float_scalar;
  roughness_pin.is_input = true;
  output.pins.push_back (roughness_pin);

  m_graph.nodes.push_back (std::move (output));
}

void
shader_graph_editor::sync_nodeflow_to_graph ()
{
  auto *nf = static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);
  if (!nf)
    return;

  // Capture live node positions back into the model.
  for (auto &kv : nf->getNodes ()) {
    auto *w = dynamic_cast<GraphNodeWrapper *> (kv.second.get ());
    if (!w)
      continue;
    wsl::gfx::graph_node *gn = m_graph.find_node (w->node_id ());
    if (!gn)
      continue;
    ImVec2 p = kv.second->getPos ();
    gn->pos_x = p.x;
    gn->pos_y = p.y;
  }

  // Rebuild the model's links from the live editor connections. The model
  // identifies pins by their uint64 id, while ImNodeFlow identifies them by
  // name, so translate via the node's pin list.
  m_graph.links.clear ();
  for (auto &wl : nf->getLinks ()) {
    auto link = wl.lock ();
    if (!link)
      continue;
    ImFlow::Pin *out = link->left ();
    ImFlow::Pin *in = link->right ();
    if (!out || !in)
      continue;
    ImFlow::BaseNode *from = out->getParent ();
    ImFlow::BaseNode *to = in->getParent ();
    if (!from || !to)
      continue;
    auto *fw = dynamic_cast<GraphNodeWrapper *> (from);
    auto *tw = dynamic_cast<GraphNodeWrapper *> (to);
    if (!fw || !tw)
      continue;

    wsl::gfx::graph_node *fn = m_graph.find_node (fw->node_id ());
    wsl::gfx::graph_node *tn = m_graph.find_node (tw->node_id ());
    if (!fn || !tn)
      continue;

    // ImNodeFlow pins carry the model pin id as their uid, so the link
    // endpoints can be read directly.
    uint64_t from_pin = out->getUid ();
    uint64_t to_pin = in->getUid ();
    if (from_pin == 0 || to_pin == 0)
      continue;

    wsl::gfx::graph_link l{};
    l.from_node = fw->node_id ();
    l.from_pin = from_pin;
    l.to_node = tw->node_id ();
    l.to_pin = to_pin;
    m_graph.links.push_back (l);
  }
}

void
shader_graph_editor::sync_graph_to_nodeflow ()
{
  // Blow away the old ImNodeFlow instance and start fresh.
  delete static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);
  m_nodeflow_handle = new ImFlow::ImNodeFlow ();

  auto *nf = static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);

  // Recreate every graph_node as a GraphNodeWrapper.
  for (const auto &node : m_graph.nodes) {
    nf->addNode<GraphNodeWrapper> (ImVec2 (node.pos_x, node.pos_y), &m_graph,
                                   node.id, m_runtime_ctx);
  }

  // Recreate connections from the model. The model stores pins by id, but
  // ImNodeFlow pins are addressed by name, so resolve via the node's pins.
  for (const auto &link : m_graph.links) {
    const wsl::gfx::graph_node *fn = m_graph.find_node (link.from_node);
    const wsl::gfx::graph_node *tn = m_graph.find_node (link.to_node);
    if (!fn || !tn)
      continue;

    const wsl::gfx::graph_pin *fp = nullptr;
    const wsl::gfx::graph_pin *tp = nullptr;
    for (const auto &p : fn->pins)
      if (p.id == link.from_pin) {
        fp = &p;
        break;
      }
    for (const auto &p : tn->pins)
      if (p.id == link.to_pin) {
        tp = &p;
        break;
      }
    if (!fp || !tp)
      continue;

    ImFlow::BaseNode *fw = find_wrapper (nf, link.from_node);
    ImFlow::BaseNode *tw = find_wrapper (nf, link.to_node);
    if (!fw || !tw)
      continue;

    ImFlow::Pin *out_pin = find_pin_by_uid (fw, fp->id, false);
    ImFlow::Pin *in_pin = find_pin_by_uid (tw, tp->id, true);
    if (out_pin && in_pin)
      in_pin->createLink (out_pin);
  }
}

void
shader_graph_editor::add_node (wsl::gfx::graph_node_kind kind,
                               const ImVec2 &pos)
{
  using namespace wsl::gfx;

  uint64_t const nid = m_next_node_id++;
  graph_node node{};
  node.id = nid;
  node.kind = kind;
  node.pos_x = pos.x;
  node.pos_y = pos.y;

  auto add_pin
      = [&] (uint64_t pid, const char *name, graph_pin_type pt, bool input) {
          graph_pin pin{};
          pin.id = pid;
          pin.name = name;
          pin.type = pt;
          pin.is_input = input;
          node.pins.push_back (pin);
        };

  switch (kind) {
  case graph_node_kind::uv0:
    node.name = "UV0";
    add_pin ((nid * 100) + 0, "UV", graph_pin_type::float2, false);
    break;
  case graph_node_kind::normal:
    node.name = "Geometry Normal";
    add_pin ((nid * 100) + 0, "Normal", graph_pin_type::float3, false);
    break;
  case graph_node_kind::world_position:
    node.name = "World Position";
    add_pin ((nid * 100) + 0, "Position", graph_pin_type::float3, false);
    break;
  case graph_node_kind::model_pbr_albedo:
    node.name = "Model Albedo";
    add_pin ((nid * 100) + 0, "Albedo", graph_pin_type::float4, false);
    break;
  case graph_node_kind::model_pbr_metallic:
    node.name = "Model Metallic";
    add_pin ((nid * 100) + 0, "Metallic", graph_pin_type::float_scalar, false);
    break;
  case graph_node_kind::model_pbr_roughness:
    node.name = "Model Roughness";
    add_pin ((nid * 100) + 0, "Roughness", graph_pin_type::float_scalar, false);
    break;
  case graph_node_kind::model_pbr_normal:
    node.name = "Model Normal";
    add_pin ((nid * 100) + 0, "Normal", graph_pin_type::float3, false);
    break;
  case graph_node_kind::model_pbr_emissive:
    node.name = "Model Emissive";
    add_pin ((nid * 100) + 0, "Emissive", graph_pin_type::float3, false);
    break;
  case graph_node_kind::texture_sample_2d:
    node.name = "Texture2D Sample";
    add_pin ((nid * 100) + 0, "UV", graph_pin_type::float2, true);
    add_pin ((nid * 100) + 1, "Tex", graph_pin_type::texture2d, true);
    add_pin ((nid * 100) + 2, "RGBA", graph_pin_type::float4, false);
    break;
  case graph_node_kind::add:
    node.name = "Add";
    add_pin ((nid * 100) + 0, "A", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 1, "B", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 2, "Out", graph_pin_type::float4, false);
    break;
  case graph_node_kind::multiply:
    node.name = "Multiply";
    add_pin ((nid * 100) + 0, "A", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 1, "B", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 2, "Out", graph_pin_type::float4, false);
    break;
  case graph_node_kind::lerp:
    node.name = "Lerp";
    add_pin ((nid * 100) + 0, "A", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 1, "B", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 2, "T", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 3, "Out", graph_pin_type::float4, false);
    break;
  case graph_node_kind::split_vector:
    node.name = "Split";
    // Takes a 4-component vector and exposes each channel as a scalar output.
    add_pin ((nid * 100) + 0, "Vector", graph_pin_type::float4, true);
    add_pin ((nid * 100) + 1, "X", graph_pin_type::float_scalar, false);
    add_pin ((nid * 100) + 2, "Y", graph_pin_type::float_scalar, false);
    add_pin ((nid * 100) + 3, "Z", graph_pin_type::float_scalar, false);
    add_pin ((nid * 100) + 4, "W", graph_pin_type::float_scalar, false);
    break;
  case graph_node_kind::combine_vector:
    node.name = "Combine";
    // Builds a 4-component vector from up to four scalar inputs.
    add_pin ((nid * 100) + 0, "X", graph_pin_type::float_scalar, true);
    add_pin ((nid * 100) + 1, "Y", graph_pin_type::float_scalar, true);
    add_pin ((nid * 100) + 2, "Z", graph_pin_type::float_scalar, true);
    add_pin ((nid * 100) + 3, "W", graph_pin_type::float_scalar, true);
    add_pin ((nid * 100) + 4, "Vector", graph_pin_type::float4, false);
    break;
  case graph_node_kind::uniform_float:
    node.name = "Float";
    node.properties["name"] = "u_Float" + std::to_string (nid);
    node.properties["default"] = "0.0";
    add_pin ((nid * 100) + 0, "Value", graph_pin_type::float_scalar, false);
    break;
  case graph_node_kind::uniform_float3:
    node.name = "Float3";
    node.properties["name"] = "u_Float3_" + std::to_string (nid);
    node.properties["default"] = "1.0,1.0,1.0";
    add_pin ((nid * 100) + 0, "Value", graph_pin_type::float3, false);
    break;
  case graph_node_kind::uniform_float4:
    node.name = "Float4";
    node.properties["name"] = "u_Float4_" + std::to_string (nid);
    node.properties["default"] = "1.0,1.0,1.0,1.0";
    add_pin ((nid * 100) + 0, "Value", graph_pin_type::float4, false);
    break;
  case graph_node_kind::uniform_texture2d:
    node.name = "Texture2D";
    node.properties["name"] = "u_Tex2D_" + std::to_string (nid);
    add_pin ((nid * 100) + 0, "Tex", graph_pin_type::texture2d, false);
    break;
  default:
    return;
  }

  // Store in graph model.
  m_graph.nodes.push_back (std::move (node));
  uint64_t const stored_id = m_graph.nodes.back ().id;

  auto *nf = static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);
  if (nf) {
    nf->placeNodeAt<GraphNodeWrapper> (pos, &m_graph, stored_id, m_runtime_ctx);
  }
}

bool
shader_graph_editor::load_graph (const std::string &path)
{
  std::ifstream file (path);
  if (!file) {
    m_compile_log = "Failed to open: " + path;
    return false;
  }

  try {
    cereal::JSONInputArchive ar (file);
    ar (cereal::make_nvp ("graph", m_graph));
    m_current_path = path;
    m_compile_log = "Loaded " + path;
    try {
      m_graph_file_mtime = std::filesystem::last_write_time (path);
    } catch (const std::exception &) {
    }
    // Keep the next node id above anything loaded so new nodes don't collide.
    for (const auto &n : m_graph.nodes)
      if (n.id >= m_next_node_id)
        m_next_node_id = n.id + 1;
    sync_graph_to_nodeflow ();
    return true;
  } catch (const std::exception &e) {
    m_compile_log = std::string ("Load error: ") + e.what ();
    return false;
  }
}

bool
shader_graph_editor::save_graph (const std::string &path)
{
  std::ofstream file (path);
  if (!file) {
    m_compile_log = "Failed to write: " + path;
    return false;
  }

  // Capture the live editor state (node positions + connections) before
  // serializing so the on-disk file round-trips correctly.
  sync_nodeflow_to_graph ();

  try {
    cereal::JSONOutputArchive ar (file);
    ar (cereal::make_nvp ("graph", m_graph));
    m_current_path = path;
    m_compile_log = "Saved " + path;
    // Refresh the hot-reload timestamp so the next frame's check does not
    // treat our own write as an external change and reload the graph.
    try {
      m_graph_file_mtime = std::filesystem::last_write_time (path);
    } catch (const std::exception &) {
    }
    return true;
  } catch (const std::exception &e) {
    m_compile_log = std::string ("Save error: ") + e.what ();
    return false;
  }
}

void
shader_graph_editor::compile_and_preview ()
{
  if (!m_codegen || !m_compiler) {
    m_compile_log = "Compiler not initialized";
    wsl::log::gfx ()->warn ("[shader_graph] compile aborted: codegen={} "
                            "compiler={}",
                            (m_codegen ? "ok" : "null"),
                            (m_compiler ? "ok" : "null"));
    return;
  }

  // Capture the live editor state (positions + connections) so compilation
  // reflects what the user actually wired up in the node view.
  sync_nodeflow_to_graph ();

  m_codegen = std::make_unique<wsl::gfx::shader_graph_codegen> (m_graph);

  // Inline the shared PBR module so the generated shader is lit with the
  // exact same engine PBR as the standard cube.frag material.
  {
    auto &res_mgr = m_runtime_ctx->resource_manager ();
    std::vector<std::string> pbr_candidates;
    pbr_candidates.push_back (
        res_mgr.resolve_path ("engine://compiled_shaders/pbr_common.slang"));
    // Fallback to the source tree so the graph never silently compiles
    // unlit when the compiled_shaders copy is missing (e.g. dev builds).
    pbr_candidates.push_back ("../../../rsc/shaders/pbr_common.slang");
    pbr_candidates.push_back ("rsc/shaders/pbr_common.slang");
    bool loaded = false;
    for (auto &pbr_path : pbr_candidates) {
      if (pbr_path.empty ())
        continue;
      std::ifstream pbr_fs (pbr_path);
      if (pbr_fs) {
        std::stringstream ss;
        ss << pbr_fs.rdbuf ();
        m_codegen->set_pbr_common_source (ss.str ());
        loaded = true;
        break;
      }
    }
    if (!loaded) {
      wsl::log::gfx ()->warn ("[shader_graph] could not read pbr_common.slang "
                              "from any known location; generated material "
                              "will be unlit");
    }
  }

  std::string slang_source = m_codegen->generate ();
  if (slang_source.empty ()) {
    m_compile_log = "Code generation produced empty source";
    wsl::log::gfx ()->debug ("[shader_graph] compile aborted: codegen "
                             "produced empty source for graph '{}'",
                             m_graph.name);
    return;
  }

  wsl::log::gfx ()->debug ("[shader_graph] compiling graph '{}': {} bytes of "
                           "slang source generated",
                           m_graph.name, slang_source.size ());

  wsl::gfx::shader_program prog;
  bool ok = m_compiler->compile_fragment (slang_source.c_str (),
                                          slang_source.size (), prog);
  if (!ok) {
    m_compile_log = "Compilation failed: " + m_compiler->last_error ();
    wsl::log::gfx ()->debug ("[shader_graph] compile FAILED for graph '{}': {}",
                             m_graph.name, m_compiler->last_error ());
    return;
  }

  // The runtime compiler (slangc, out-of-process) parses reflection from
  // slangc's JSON output. If that ever comes back empty, fall back to the
  // layout the code generator knows about so the preview still builds.
  bool used_reflection_fallback = false;
  if (prog.fragment_reflection.uniform_buffers.empty ()) {
    used_reflection_fallback = true;
    prog.fragment_reflection = m_codegen->build_reflection ();
    wsl::log::gfx ()->debug (
        "[shader_graph] compiler returned no reflection for "
        "graph '{}'; using codegen reflection fallback",
        m_graph.name);
  }

  // The generated shader's `u_Samplers[N]` array size is known exactly by the
  // code generator (one slot per texture_sample_2d node). Force it so the
  // renderer declares the matching sampler count regardless of how slang's
  // reflection JSON reports samplers.
  prog.fragment_reflection.sampler_count
      = m_codegen->build_reflection ().sampler_count;

  m_compile_log = "Compilation succeeded.\nSource length: "
                  + std::to_string (slang_source.size ()) + " bytes";
  if (used_reflection_fallback) {
    m_compile_log
        += "\nWarning: slang reflection missing, used fallback layout";
  }
  wsl::log::gfx ()->debug (
      "[shader_graph] compile SUCCEEDED for graph '{}': {} "
      "uniform buffer(s), {} texture(s)",
      m_graph.name, prog.fragment_reflection.uniform_buffers.size (),
      prog.fragment_reflection.textures.size ());
  update_preview (prog);
}

void
shader_graph_editor::update_preview (const wsl::gfx::shader_program &prog)
{
  if (!m_runtime_ctx) {
    return;
  }

  auto &res_mgr = m_runtime_ctx->resource_manager ();

  auto prog_copy = std::make_shared<wsl::gfx::shader_program> (prog);
  m_preview_program_id
      = res_mgr.register_shader_program (prog_copy, "preview_" + m_graph.name);

  auto mat = std::make_shared<wsl::gfx::material_asset> ();
  mat->name = "preview_" + m_graph.name;
  mat->shader_program = m_preview_program_id;

  for (const auto &node : m_graph.nodes) {
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float";
      float def = 0.0f;
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        def = std::stof (dv->second);
      }
      mat->default_parameters[name] = wsl::gfx::material_parameter (name, def);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float3) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float3";
      glm::vec3 v (1.0f);
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        std::sscanf (dv->second.c_str (), "%f,%f,%f", &v.x, &v.y, &v.z);
      }
      mat->default_parameters[name] = wsl::gfx::material_parameter (name, v);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float4) {
      auto it = node.properties.find ("name");
      std::string name
          = (it != node.properties.end ()) ? it->second : "u_Float4";
      glm::vec4 v (1.0f);
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        std::sscanf (dv->second.c_str (), "%f,%f,%f,%f", &v.x, &v.y, &v.z,
                     &v.w);
      }
      mat->default_parameters[name] = wsl::gfx::material_parameter (name, v);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_texture2d) {
      auto name_it = node.properties.find ("name");
      std::string uniform_name
          = (name_it != node.properties.end ()) ? name_it->second : "u_Tex2D";

      if (auto image_it = node.properties.find ("image_id");
          image_it != node.properties.end () && !image_it->second.empty ()) {
        char *end_ptr = nullptr;
        unsigned long long parsed
            = std::strtoull (image_it->second.c_str (), &end_ptr, 10);
        if (end_ptr != image_it->second.c_str () && *end_ptr == '\0') {
          const wsl::rsc::image_id image_id{ static_cast<entt::id_type> (
              parsed) };
          mat->default_parameters[uniform_name]
              = wsl::gfx::material_parameter (uniform_name, image_id);
          res_mgr.load (image_id);
        }
      }
    }
  }

  std::filesystem::path tmp_path
      = std::filesystem::temp_directory_path () / (mat->name + ".wslmat");
  {
    std::ofstream ofs (tmp_path);
    if (!ofs) {
      return;
    }
    cereal::JSONOutputArchive ar (ofs);
    ar (cereal::make_nvp ("material", *mat));
    // `ofs` is flushed and closed when it goes out of scope, so the file is
    // fully written before we register/load it below.
  }
  m_preview_material_id = res_mgr.register_material (tmp_path.string ());
  res_mgr.load (m_preview_material_id);
}

wsl::rsc::material_id
shader_graph_editor::create_material_from_graph (const std::string &name)
{
  if (!m_runtime_ctx || m_preview_program_id.value == 0) {
    return wsl::rsc::material_id{};
  }

  auto &res_mgr = m_runtime_ctx->resource_manager ();
  auto proj = res_mgr.current_project ();
  if (!proj) {
    wsl::log::editor ()->warn ("Cannot create material: no active project.");
    return wsl::rsc::material_id{};
  }

  std::filesystem::path graph_path = std::filesystem::path (proj->root_path)
                                     / proj->materials_path
                                     / (name + ".wslgraph");
  std::filesystem::create_directories (graph_path.parent_path ());
  save_graph (graph_path.string ());

  auto mat = std::make_shared<wsl::gfx::material_asset> ();
  mat->name = name;
  mat->shader_program = m_preview_program_id;
  mat->path
      = (std::filesystem::path (proj->materials_path) / (name + ".wslmat"))
            .string ();

  for (const auto &node : m_graph.nodes) {
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float) {
      auto it = node.properties.find ("name");
      std::string n = (it != node.properties.end ()) ? it->second : "u_Float";
      float def = 0.0f;
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        def = std::stof (dv->second);
      }
      mat->default_parameters[n] = wsl::gfx::material_parameter (n, def);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float3) {
      auto it = node.properties.find ("name");
      std::string n = (it != node.properties.end ()) ? it->second : "u_Float3";
      glm::vec3 v (1.0f);
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        std::sscanf (dv->second.c_str (), "%f,%f,%f", &v.x, &v.y, &v.z);
      }
      mat->default_parameters[n] = wsl::gfx::material_parameter (n, v);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_float4) {
      auto it = node.properties.find ("name");
      std::string n = (it != node.properties.end ()) ? it->second : "u_Float4";
      glm::vec4 v (1.0f);
      if (auto dv = node.properties.find ("default");
          dv != node.properties.end ()) {
        std::sscanf (dv->second.c_str (), "%f,%f,%f,%f", &v.x, &v.y, &v.z,
                     &v.w);
      }
      mat->default_parameters[n] = wsl::gfx::material_parameter (n, v);
    }
    if (node.kind == wsl::gfx::graph_node_kind::uniform_texture2d) {
      auto name_it = node.properties.find ("name");
      std::string uniform_name
          = (name_it != node.properties.end ()) ? name_it->second : "u_Tex2D";

      if (auto image_it = node.properties.find ("image_id");
          image_it != node.properties.end () && !image_it->second.empty ()) {
        char *end_ptr = nullptr;
        unsigned long long parsed
            = std::strtoull (image_it->second.c_str (), &end_ptr, 10);
        if (end_ptr != image_it->second.c_str () && *end_ptr == '\0') {
          const wsl::rsc::image_id image_id{ static_cast<entt::id_type> (
              parsed) };
          mat->default_parameters[uniform_name]
              = wsl::gfx::material_parameter (uniform_name, image_id);
          res_mgr.load (image_id);
        }
      }
    }
  }

  std::filesystem::path mat_path
      = std::filesystem::path (proj->root_path) / mat->path;
  std::ofstream ofs (mat_path);
  if (!ofs) {
    wsl::log::editor ()->error ("Failed to write material file: {}",
                                mat_path.string ());
    return wsl::rsc::material_id{};
  }

  try {
    cereal::JSONOutputArchive ar (ofs);
    ar (cereal::make_nvp ("material", *mat));
    // Flush and close before registering/loading so the on-disk file is
    // complete when the resource manager reads it back.
  } catch (const std::exception &e) {
    wsl::log::editor ()->error ("Failed to serialize material: {}", e.what ());
    return wsl::rsc::material_id{};
  }

  auto mid = res_mgr.register_material (mat_path.string ());
  res_mgr.load (mid);
  wsl::log::editor ()->info ("Created material '{}' at {}", name,
                             mat_path.string ());
  return mid;
}

void
shader_graph_editor::draw (const char *title, bool *open)
{
  if (!ImGui::Begin (title, open, ImGuiWindowFlags_MenuBar)) {
    ImGui::End ();
    return;
  }

  // Hot reload: check if the current .wslgraph or .wslmat changed on disk.
  if (m_enable_hot_reload && !m_current_path.empty ()) {
    try {
      auto mtime = std::filesystem::last_write_time (m_current_path);
      if (mtime != m_graph_file_mtime) {
        m_graph_file_mtime = mtime;
        load_graph (m_current_path);
        m_compile_log = "Hot-reloaded graph from disk.";
      }

      // Also check the companion .wslmat
      std::string mat_path = m_current_path;
      auto dot = mat_path.rfind ('.');
      if (dot != std::string::npos) {
        mat_path = mat_path.substr (0, dot) + ".wslmat";
      } else {
        mat_path += ".wslmat";
      }
      if (std::filesystem::exists (mat_path)) {
        auto mat_mtime = std::filesystem::last_write_time (mat_path);
        if (mat_mtime != m_mat_file_mtime) {
          m_mat_file_mtime = mat_mtime;
          auto &res_mgr = m_runtime_ctx->resource_manager ();
          auto mid = res_mgr.register_material (mat_path);
          res_mgr.load (mid);
          m_compile_log = "Hot-reloaded material from disk.";
        }
      }
    } catch (const std::exception &) {
      // Ignore filesystem errors during hot-reload checks.
    }
  }

  draw_toolbar ();

  ImGui::Separator ();

  ImVec2 canvas_avail = ImGui::GetContentRegionAvail ();

  ImGui::BeginChild ("ShaderGraphCanvas", canvas_avail, true,
                     ImGuiWindowFlags_NoScrollbar);
  {
    // Detect right-click BEFORE nf->update() changes the window context.
    bool const canvas_hovered
        = ImGui::IsWindowHovered (ImGuiHoveredFlags_RootAndChildWindows);
    if (canvas_hovered && ImGui::IsMouseClicked (1)) {
      m_context_menu_pos = ImGui::GetMousePos ();
      ImGui::OpenPopup ("AddNodeMenu");
    }

    auto *nf = static_cast<ImFlow::ImNodeFlow *> (m_nodeflow_handle);
    if (nf) {
      nf->update ();
    }

    if (ImGui::BeginPopup ("AddNodeMenu")) {
      if (ImGui::BeginMenu ("Input")) {
        if (ImGui::MenuItem ("UV0")) {
          add_node (wsl::gfx::graph_node_kind::uv0, m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Geometry Normal")) {
          add_node (wsl::gfx::graph_node_kind::normal, m_context_menu_pos);
        }
        if (ImGui::MenuItem ("World Position")) {
          add_node (wsl::gfx::graph_node_kind::world_position,
                    m_context_menu_pos);
        }
        if (ImGui::BeginMenu ("Model PBR")) {
          if (ImGui::MenuItem ("Albedo")) {
            add_node (wsl::gfx::graph_node_kind::model_pbr_albedo,
                      m_context_menu_pos);
          }
          if (ImGui::MenuItem ("Metallic")) {
            add_node (wsl::gfx::graph_node_kind::model_pbr_metallic,
                      m_context_menu_pos);
          }
          if (ImGui::MenuItem ("Roughness")) {
            add_node (wsl::gfx::graph_node_kind::model_pbr_roughness,
                      m_context_menu_pos);
          }
          if (ImGui::MenuItem ("Normal")) {
            add_node (wsl::gfx::graph_node_kind::model_pbr_normal,
                      m_context_menu_pos);
          }
          if (ImGui::MenuItem ("Emissive")) {
            add_node (wsl::gfx::graph_node_kind::model_pbr_emissive,
                      m_context_menu_pos);
          }
          ImGui::EndMenu ();
        }
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("Texture")) {
        if (ImGui::MenuItem ("Texture2D Sample")) {
          add_node (wsl::gfx::graph_node_kind::texture_sample_2d,
                    m_context_menu_pos);
        }
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("Math")) {
        if (ImGui::MenuItem ("Add")) {
          add_node (wsl::gfx::graph_node_kind::add, m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Multiply")) {
          add_node (wsl::gfx::graph_node_kind::multiply, m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Lerp")) {
          add_node (wsl::gfx::graph_node_kind::lerp, m_context_menu_pos);
        }
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("Vector")) {
        if (ImGui::MenuItem ("Split")) {
          add_node (wsl::gfx::graph_node_kind::split_vector,
                    m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Combine")) {
          add_node (wsl::gfx::graph_node_kind::combine_vector,
                    m_context_menu_pos);
        }
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("Uniform")) {
        if (ImGui::MenuItem ("Float")) {
          add_node (wsl::gfx::graph_node_kind::uniform_float,
                    m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Float3")) {
          add_node (wsl::gfx::graph_node_kind::uniform_float3,
                    m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Float4")) {
          add_node (wsl::gfx::graph_node_kind::uniform_float4,
                    m_context_menu_pos);
        }
        if (ImGui::MenuItem ("Texture2D")) {
          add_node (wsl::gfx::graph_node_kind::uniform_texture2d,
                    m_context_menu_pos);
        }
        ImGui::EndMenu ();
      }
      ImGui::EndPopup ();
    }
  }
  ImGui::EndChild ();

  ImGui::End ();
}

void
shader_graph_editor::draw_toolbar ()
{
  if (ImGui::BeginMenuBar ()) {
    if (ImGui::BeginMenu ("File")) {
      if (ImGui::MenuItem ("New")) {
        new_graph ();
      }
      if (ImGui::MenuItem ("Open...")) {
      }
      if (ImGui::MenuItem ("Save")) {
        if (!m_current_path.empty ()) {
          save_graph (m_current_path);
        }
      }
      if (ImGui::MenuItem ("Save As...")) {
      }
      ImGui::Separator ();
      if (ImGui::MenuItem ("Create Material")) {
        create_material_from_graph (m_graph.name);
      }
      ImGui::EndMenu ();
    }

    if (ImGui::Button ("Compile")) {
      wsl::log::editor ()->debug ("[shader_graph] Compile button clicked for "
                                  "graph '{}'",
                                  m_graph.name);
      compile_and_preview ();
      m_show_compile_log = true;
    }

    ImGui::EndMenuBar ();
  }
}

void
shader_graph_editor::draw_compile_log ()
{
  ImGui::BeginChild ("CompileLog", ImVec2 (0, 100), true);
  ImGui::TextUnformatted (m_compile_log.c_str ());
  ImGui::EndChild ();
}

void
shader_graph_editor::draw_properties ()
{
}

} // namespace editor
