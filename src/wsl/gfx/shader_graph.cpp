#include "shader_graph.hpp"

namespace wsl
{

namespace gfx
{

const graph_node *
shader_graph::find_node (uint64_t id) const
{
  for (const auto &n : nodes) {
    if (n.id == id) {
      return &n;
    }
  }
  return nullptr;
}

graph_node *
shader_graph::find_node (uint64_t id)
{
  for (auto &n : nodes) {
    if (n.id == id) {
      return &n;
    }
  }
  return nullptr;
}

const graph_node *
shader_graph::find_output_node () const
{
  for (const auto &n : nodes) {
    if (n.kind == graph_node_kind::material_output) {
      return &n;
    }
  }
  return nullptr;
}

const graph_link *
shader_graph::find_input_link (uint64_t to_node, uint64_t to_pin) const
{
  for (const auto &l : links) {
    if (l.to_node == to_node && l.to_pin == to_pin) {
      return &l;
    }
  }
  return nullptr;
}

std::vector<const graph_link *>
shader_graph::find_output_links (uint64_t from_node, uint64_t from_pin) const
{
  std::vector<const graph_link *> result;
  for (const auto &l : links) {
    if (l.from_node == from_node && l.from_pin == from_pin) {
      result.push_back (&l);
    }
  }
  return result;
}

} // namespace gfx

} // namespace wsl
