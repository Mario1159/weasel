#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <imgui.h>
#include <imsearch.h>

namespace wsl::comp::singl { class runtime_context; }
namespace wsl::rsc { class resource_manager; }
namespace editor { class text_editor; }

namespace editor
{

class file_list
{
public:
  file_list () = default;

  struct entry
  {
    std::string label; // shown in UI (relative)
    std::string path;  // absolute/normalized path
  };

  void draw (const char *title, bool *p_open, wsl::rsc::resource_manager *resources,
             editor::text_editor *editor, bool *show_editor,
             wsl::comp::singl::runtime_context *runtime_ctx);

  // Call every frame; it will refresh when the active project changes.

  enum class create_kind
  {
    component,
    system,
    singleton,
  };

  void refresh_if_needed (wsl::rsc::resource_manager *resources);
  void queue_create_popup (create_kind kind, bool header_only);
  void draw_create_popup (wsl::rsc::resource_manager *resources,
                          editor::text_editor *editor, bool *show_editor);
  static void gather_cpp_hpp (const std::filesystem::path &base,
                              std::vector<entry> &out);
  static void gather_files_with_extensions (
      const std::filesystem::path &base, const std::vector<std::string> &exts,
      std::vector<entry> &out);

  // UI state
  int selected_comp = -1;
  int selected_sys = -1;
  int selected_singleton = -1;
  int selected_ui = -1;
  int selected_shader = -1;

  bool needs_refresh = true;
  bool create_header_only = false;
  bool request_create_popup_open = false;
  bool request_name_focus = false;
  create_kind pending_create_kind = create_kind::component;
  char create_name[128] = "";
  std::string create_error;
  std::string pending_open_path;

  // Data
  std::string last_project_root;
  std::string last_components_abs;
  std::string last_singletons_abs;
  std::string last_systems_abs;
  std::string last_ui_layouts_abs;
  std::string last_shaders_abs;

  std::vector<entry> component_files;
  std::vector<entry> singleton_files;
  std::vector<entry> system_files;
  std::vector<entry> ui_layout_files;
  std::vector<entry> shader_files;
};

} // namespace editor
