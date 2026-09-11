module;
#include <cstdint>
#include <string>
#include <vector>

module wsl.core;

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

} // namespace gfx

} // namespace wsl
