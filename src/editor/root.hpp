#pragma once

#include <entt/entt.hpp>
#include <imgui.h>

#include "inspector.hpp"
#include "logger.hpp"
#include "console.hpp"
#include "ecs_inspector.hpp"
#include "file_list.hpp"
#include "game_view.hpp"
#include "input_map_inspector.hpp"
#include "resource_inspector.hpp"
#include "signal_inspector.hpp"
#include "build_inspector.hpp"
#include "system_inspector.hpp"
#include "text_editor.hpp"
#include "shader_graph_editor.hpp"

namespace editor
{

class root
{
public:
  root (wsl::comp::singl::runtime_context *runtime_ctx,
        wsl::comp::singl::editor_context *editor_ctx);

  // setup / configuration
  void attach_console_to_spdlog ();
  void set_console_command_handler (console::command_handler_t handler);

  // frame rendering
  void draw (entt::registry &registry, wsl::gfx::render_window &rw);

  ecs_selection &
  get_selection ()
  {
    return m_selection;
  }

private:
  static void build_default_dock_layout (ImGuiID dockspace_id);
  void draw_dockspace ();
  void draw_main_menu ();
  void draw_new_project_popup ();
  void open_project_file ();
  void save_project_file ();
  void open_project_folder_dialog ();

  logger m_console_window;
  console m_interactive_console;
  system_inspector m_system_inspector_window;
  ecs_inspector m_ecs_inspector_window;
  resource_inspector m_resource_inspector_window;
  game_view m_game_view_window;
  input_map_inspector m_input_map_window;
  signal_inspector m_signal_inspector_window;
  build_inspector m_build_inspector_window;
  text_editor m_text_editor_window;
  file_list m_file_list_window;
  shader_graph_editor m_shader_graph_editor;

  bool m_show_console = true;
  bool m_show_system_inspector = true;
  bool m_show_ecs_inspector = true;
  bool m_show_game_view = true;
  bool m_show_resources = true;
  bool m_show_input_map = true;
  bool m_show_new_console = true;
  bool m_show_agent = true;
  bool m_show_signal_inspector = true;
  bool m_show_build_inspector = true;
  bool m_show_text_editor = true;
  bool m_show_file_list = true;
  bool m_show_shader_graph = true;

  bool m_dock_layout_built = false;

  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  ecs_selection m_selection;

  enum class dialog_mode
  {
    none,
    open_project,
    select_folder,
    load_scene,
    save_scene
  };
  dialog_mode m_current_dialog_mode = dialog_mode::none;

  bool m_request_new_project_popup = false;
  char m_new_project_name[128] = "NewProject";
  char m_new_project_folder[512] = "./";
  bool m_new_project_create_folder = true;

  bool m_waiting_for_file_dialog = false;
  std::optional<std::string> m_dialog_result;

  static void file_dialog_callback (void *userdata, const char *const *filelist,
                                    int filter);

  // Preferences popup
  bool m_show_preferences = false;
  bool m_request_preferences_popup = false;

  bool m_show_build_settings = false;
  bool m_request_build_settings_popup = false;

  bool m_show_project_settings = false;
  bool m_request_project_settings_popup = false;

  // Style prefs
  int m_pref_font_index = 0;      // 0..4 (regular/light/medium/semibold/bold)
  float m_pref_font_scale = 1.0F; // global scale

  // 5 engine colors
  ImVec4 m_pref_primary
      = ImVec4 (0x68 / 255.F, 0x76 / 255.F, 0x3c / 255.F, 1.0F);
  ImVec4 m_pref_secondary
      = ImVec4 (0xde / 255.F, 0xc1 / 255.F, 0x6b / 255.F, 1.0F);
  ImVec4 m_pref_bg1 = ImVec4 (0x09 / 255.F, 0x09 / 255.F, 0x0c / 255.F, 1.0F);
  ImVec4 m_pref_bg2 = ImVec4 (0x10 / 255.F, 0x11 / 255.F, 0x15 / 255.F, 1.0F);
  ImVec4 m_pref_fg = ImVec4 (0xf2 / 255.F, 0xeb / 255.F, 0xe9 / 255.F, 1.0F);

  void draw_preferences_popup ();
  void apply_preferences_style ();

  void draw_build_settings_popup ();
  void draw_project_settings_popup ();

  void draw_welcome_tab ();
  void draw_status_bar ();
  void update_recent_projects (const std::string &path);
  void load_recent_projects ();
  void save_recent_projects ();

  std::vector<std::string> m_recent_projects;
  bool m_show_welcome = true;
};

} // namespace editor
