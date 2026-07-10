#pragma once

#include "wsl/gfx/shader_graph.hpp"
#include "wsl/gfx/shader_graph_codegen.hpp"
#include "wsl/gfx/shader_compiler.hpp"
#include "wsl/gfx/material_asset.hpp"
#include "wsl/rsc/resource_ids.hpp"

#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace wsl::comp::singl
{
class runtime_context;
class editor_context;
}

namespace editor
{

/*!
 * \brief Editor panel for authoring shader graphs using ImNodeFlow.
 *
 * Provides node-based authoring of fragment shaders, compilation preview,
 * and material creation.
 */
class shader_graph_editor
{
public:
  shader_graph_editor (wsl::comp::singl::runtime_context *runtime_ctx,
                       wsl::comp::singl::editor_context *editor_ctx);
  ~shader_graph_editor ();

  void draw (const char *title, bool *open);

  void new_graph ();
  bool load_graph (const std::string &path);
  bool save_graph (const std::string &path);

  /*! \brief Compile the current graph and update the preview material. */
  void compile_and_preview ();

  /*! \brief Create a material_asset from the compiled graph. */
  wsl::rsc::material_id create_material_from_graph (const std::string &name);

private:
  wsl::comp::singl::runtime_context *m_runtime_ctx = nullptr;
  wsl::comp::singl::editor_context *m_editor_ctx = nullptr;

  wsl::gfx::shader_graph m_graph;
  std::unique_ptr<wsl::gfx::shader_graph_codegen> m_codegen;
  std::unique_ptr<wsl::gfx::shader_compiler> m_compiler;

  // Opaque handle to ImFlow::ImNodeFlow ( avoids pulling ImNodeFlow.h into
  // root.hpp )
  void *m_nodeflow_handle = nullptr;

  std::string m_current_path;
  std::string m_compile_log;
  bool m_show_compile_log = false;

  // Preview
  wsl::rsc::material_id m_preview_material_id{};
  wsl::rsc::shader_program_id m_preview_program_id{};

  uint64_t m_next_node_id = 2;

  // Cached mouse position when the context menu was opened (screen space).
  ImVec2 m_context_menu_pos;

  // Hot reload tracking
  std::filesystem::file_time_type m_graph_file_mtime;
  std::filesystem::file_time_type m_mat_file_mtime;
  bool m_enable_hot_reload = true;

  void build_default_graph ();
  void sync_graph_to_nodeflow ();
  void sync_nodeflow_to_graph ();
  void draw_toolbar ();
  void draw_properties ();
  void draw_compile_log ();
  void update_preview (const wsl::gfx::shader_program &prog);
  void add_node (wsl::gfx::graph_node_kind kind, const ImVec2 &pos);
};

} // namespace editor
