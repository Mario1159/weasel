#include "root.hpp"
#include "editor/ecs_inspector_utils.hpp"
#include "rsc/project.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/gfx/render_window.hpp"
#include "inspector.hpp"
#include "logger.hpp"
#include "ecs_inspector.hpp"
#include "game_view.hpp"
#include "imgui_internal.h"
#include "resource_inspector.hpp"
#include "system_inspector.hpp"
#include "job_manager.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <imgui.h>
#include <optional>
#include <string>

namespace editor
{

static void
open_tracy ()
{
  std::system ("tracy -a localhost &");
}

static void
open_gf2 ()
{
  std::system ("gf2 &");
}

editor::root::root (wsl::comp::singl::runtime_context *runtime_ctx,
                    wsl::comp::singl::editor_context *editor_ctx)
    : m_console_window (editor_ctx),
      m_interactive_console (runtime_ctx, editor_ctx),
      m_system_inspector_window (runtime_ctx, editor_ctx, &m_selection),
      m_ecs_inspector_window (runtime_ctx, editor_ctx, m_selection),
      m_resource_inspector_window (runtime_ctx, editor_ctx),
      m_game_view_window (runtime_ctx, editor_ctx),
      m_signal_inspector_window (runtime_ctx, editor_ctx, &m_selection),
      m_build_inspector_window (runtime_ctx, editor_ctx)
{
  this->m_runtime_ctx = runtime_ctx;
  this->m_editor_ctx = editor_ctx;
  if (const char *docs = SDL_GetUserFolder (SDL_FOLDER_DOCUMENTS)) {
    std::snprintf (m_new_project_folder, sizeof (m_new_project_folder), "%s",
                   docs);
  }
  load_recent_projects ();
}

void
editor::root::attach_console_to_spdlog ()
{
  m_console_window.attach_to_spdlog ();
}

void
editor::root::set_console_command_handler (console::command_handler_t handler)
{
  m_interactive_console.set_command_handler (std::move (handler));
}

void
editor::root::draw (entt::registry &registry, wsl::gfx::render_window &rw)
{
  wsl::comp::singl::runtime_context &runtime_ctx = *this->m_runtime_ctx;
  if (m_editor_ctx->game_fullscreen) {
    return;
  }

  if (m_selection.kind == selection_kind::entity) {
    m_editor_ctx->selected_entity = m_selection.selected_entity;
  } else {
    m_editor_ctx->selected_entity = entt::null;
  }

  draw_dockspace ();
  draw_main_menu ();

  if (m_request_new_project_popup) {
    ImGui::OpenPopup ("New Project");
    m_request_new_project_popup = false;
  }

  draw_new_project_popup ();

  if (m_show_welcome) {
    draw_welcome_tab ();
  }

  if (m_show_console) {
    m_console_window.draw ("Log", &m_show_console);
  }

  if (m_show_system_inspector) {
    m_system_inspector_window.draw ();
  }

  if (m_show_ecs_inspector) {
    m_ecs_inspector_window.draw ();
  }

  if (m_show_resources) {
    m_resource_inspector_window.draw ();
  }

  if (m_show_input_map) {
    m_input_map_window.draw (registry, this->m_runtime_ctx);
  }

  if (m_show_new_console) {
    m_interactive_console.draw ("Console", &m_show_new_console);
  }

  if (m_show_agent) {
    if (ImGui::Begin ("Agent", &m_show_agent)) {
    }
    ImGui::End ();
  }

  if (m_show_signal_inspector) {
    m_signal_inspector_window.draw ();
  }

  if (m_show_build_inspector) {
    m_build_inspector_window.draw ();
  }

  if (m_show_game_view) {
    m_game_view_window.set_render_texture (
        this->m_runtime_ctx->window.present_tex);
    m_game_view_window.set_selection (&m_selection);
    m_game_view_window.draw (registry, rw);
  }

  // feed mono font and background color from renderer_imgui
  m_text_editor_window.set_mono_font (
      m_editor_ctx->get_imgui_renderer ()->get_fonts ().mono);
  m_text_editor_window.set_background_color (ImGui::ColorConvertFloat4ToU32 (
      m_editor_ctx->get_imgui_renderer ()->get_theme ().background2));
  if (m_show_text_editor) {
    m_text_editor_window.draw ("Text Editor", &m_show_text_editor);
  }

  if (m_show_file_list) {
    m_file_list_window.draw (
        "Files", &m_show_file_list, &runtime_ctx.resource_manager,
        &m_text_editor_window, &m_show_text_editor, this->m_runtime_ctx);
  }

  if (!m_waiting_for_file_dialog && m_dialog_result.has_value ()) {
    if (m_current_dialog_mode == dialog_mode::open_project) {
      m_selection = {};
      runtime_ctx.editor_ctx->selected_entity = entt::null;
      runtime_ctx.editor_ctx->pending_project_load = *m_dialog_result;
      update_recent_projects (*m_dialog_result);
      m_show_welcome = false;
      wsl::log::editor ()->debug ("Editor: queued load_project for {}",
                                  *m_dialog_result);
    } else if (m_current_dialog_mode == dialog_mode::select_folder) {
      std::snprintf (m_new_project_folder, sizeof (m_new_project_folder), "%s",
                     m_dialog_result->c_str ());
    } else if (m_current_dialog_mode == dialog_mode::load_scene) {
      m_runtime_ctx->resource_manager.import_scene (*m_dialog_result);
    } else if (m_current_dialog_mode == dialog_mode::save_scene) {
      wsl::rsc::scene const *scene = m_runtime_ctx->scene_manager.get_active ();
      if (scene != nullptr) {
        std::filesystem::path const p (*m_dialog_result);
        bool const is_prefab = p.extension () == ".prefab";
        m_runtime_ctx->resource_manager.save_scene (*scene, *m_dialog_result,
                                                    is_prefab);
      }
    }

    m_dialog_result.reset ();
    m_current_dialog_mode = dialog_mode::none;
  }

  if (m_request_preferences_popup) {
    ImGui::OpenPopup ("Preferences");
    m_request_preferences_popup = false;
  }

  if (m_show_preferences
      || ImGui::IsPopupOpen ("Preferences", ImGuiPopupFlags_AnyPopupId)) {
    draw_preferences_popup ();
  }

  if (m_request_build_settings_popup) {
    ImGui::OpenPopup ("Build Settings");
    m_request_build_settings_popup = false;
  }

  if (m_show_build_settings
      || ImGui::IsPopupOpen ("Build Settings", ImGuiPopupFlags_AnyPopupId)) {
    draw_build_settings_popup ();
  }

  if (m_request_project_settings_popup) {
    ImGui::OpenPopup ("Project Settings");
    m_request_project_settings_popup = false;
  }

  if (m_show_project_settings
      || ImGui::IsPopupOpen ("Project Settings", ImGuiPopupFlags_AnyPopupId)) {
    draw_project_settings_popup ();
  }

  draw_status_bar ();
}

void
editor::root::build_default_dock_layout (ImGuiID dockspace_id)
{
  ImGui::DockBuilderRemoveNode (dockspace_id);
  ImGui::DockBuilderAddNode (dockspace_id, ImGuiDockNodeFlags_DockSpace);

  ImGui::DockBuilderSetNodeSize (dockspace_id,
                                 ImGui::GetMainViewport ()->WorkSize);

  ImGuiID dock_main = dockspace_id;
  ImGuiID dock_left;
  ImGuiID dock_right;
  ImGuiID dock_bottom;

  // left: system inspector
  ImGui::DockBuilderSplitNode (dock_main, ImGuiDir_Left, 0.22F, &dock_left,
                               &dock_main);

  // right: ecs inspector
  ImGui::DockBuilderSplitNode (dock_main, ImGuiDir_Right, 0.25F, &dock_right,
                               &dock_main);

  // bottom: console
  ImGui::DockBuilderSplitNode (dock_main, ImGuiDir_Down, 0.25F, &dock_bottom,
                               &dock_main);

  // dock windows
  ImGui::DockBuilderDockWindow ("Systems", dock_left);
  ImGui::DockBuilderDockWindow ("Resources", dock_bottom);
  ImGui::DockBuilderDockWindow ("Entities and Singletons", dock_left);
  ImGui::DockBuilderDockWindow ("Files", dock_left);
  ImGui::DockBuilderDockWindow ("Inspector", dock_right);
  ImGui::DockBuilderDockWindow ("Log", dock_bottom);
  ImGui::DockBuilderDockWindow ("Input Map", dock_bottom);
  ImGui::DockBuilderDockWindow ("Console", dock_bottom);
  ImGui::DockBuilderDockWindow ("Agent", dock_bottom);
  ImGui::DockBuilderDockWindow ("Welcome", dock_main);
  ImGui::DockBuilderDockWindow ("Game View", dock_main);
  ImGui::DockBuilderDockWindow ("Signals", dock_right);
  ImGui::DockBuilderDockWindow ("Build", dock_right);
  ImGui::DockBuilderDockWindow ("Text Editor", dock_main);

  ImGui::DockBuilderFinish (dockspace_id);
}

void
editor::root::draw_dockspace ()
{
  ImGuiWindowFlags const window_flags
      = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  const ImGuiViewport *viewport = ImGui::GetMainViewport ();
  ImGui::SetNextWindowPos (viewport->WorkPos);
  ImGui::SetNextWindowSize (
      ImVec2 (viewport->WorkSize.x, viewport->WorkSize.y - 24.0F));

  ImGui::Begin ("Editor DockSpace", nullptr, window_flags);

  ImGuiID const dockspace_id = ImGui::GetID ("Editor_DockSpace");

  if (!m_dock_layout_built) {
    build_default_dock_layout (dockspace_id);
    m_dock_layout_built = true;
  }

  ImGui::DockSpace (dockspace_id, ImVec2 (0, 0));

  ImGui::End ();
}

void
editor::root::draw_main_menu ()
{
  if (!ImGui::BeginMainMenuBar ()) {
    return;
  }

  if (ImGui::BeginMenu ("File")) {
    if (ImGui::MenuItem ("New Project")) {
      m_request_new_project_popup = true;
    }

    if (ImGui::MenuItem ("Load Project")) {
      open_project_file ();
    }

    ImGui::Separator ();
    if (ImGui::MenuItem ("Save")) {
      // Optional: implement project save later
    }

    if (ImGui::MenuItem ("Save All")) {
    }

    ImGui::Separator ();

    if (ImGui::MenuItem ("Exit")) {
      SDL_Event quit_event;
      quit_event.type = SDL_EVENT_QUIT;
      SDL_PushEvent (&quit_event);
    }

    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Edit")) {
    ImGui::MenuItem ("Undo");
    ImGui::MenuItem ("Redo");
    ImGui::Separator ();
    ImGui::MenuItem ("Cut");
    ImGui::MenuItem ("Copy");
    ImGui::MenuItem ("Paste");
    ImGui::Separator ();
    if (ImGui::MenuItem ("Preferences")) {
      m_show_preferences = true;
      m_request_preferences_popup = true; // open next draw
    }
    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Scene")) {
    if (ImGui::MenuItem ("New Scene")) {
      m_resource_inspector_window.new_scene_dialog ();
    }
    if (ImGui::MenuItem ("Load Scene")) {
      if (!m_waiting_for_file_dialog) {
        SDL_DialogFileFilter filters[] = { { "Scene files", "scene;json" },
                                           { "Prefab files", "prefab" },
                                           { "All files", "*" } };
        m_current_dialog_mode = dialog_mode::load_scene;
        m_waiting_for_file_dialog = true;
        m_dialog_result.reset ();
        SDL_ShowOpenFileDialog (file_dialog_callback, this,
                                m_runtime_ctx->window.handler, filters,
                                SDL_arraysize (filters), nullptr, false);
      }
    }
    if (ImGui::MenuItem ("Save Scene")) {
      if (!m_waiting_for_file_dialog) {
        SDL_DialogFileFilter filters[] = { { "Scene files", "scene;json" },
                                           { "Prefab files", "prefab" },
                                           { "All files", "*" } };
        m_current_dialog_mode = dialog_mode::save_scene;
        m_waiting_for_file_dialog = true;
        m_dialog_result.reset ();
        SDL_ShowSaveFileDialog (file_dialog_callback, this,
                                m_runtime_ctx->window.handler, filters,
                                SDL_arraysize (filters), nullptr);
      }
    }
    if (ImGui::MenuItem ("Reload Scene")) {
      auto *active_scene = m_runtime_ctx->scene_manager.get_active ();
      if (active_scene != nullptr) {
        for (const auto &info :
             m_runtime_ctx->resource_manager.list_scenes ()) {
          if (!info.is_prefab && info.name == active_scene->get_name ()) {
            wsl::rsc::scene_id sid{ info.id };
            m_runtime_ctx->resource_manager.unload (sid);
            m_runtime_ctx->resource_manager.load (sid);
            break;
          }
        }
      }
    }
    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Project")) {
    if (ImGui::MenuItem ("Project Settings")) {
      m_show_project_settings = true;
      m_request_project_settings_popup = true;
    }
    if (ImGui::MenuItem ("Build Settings")) {
      m_show_build_settings = true;
      m_request_build_settings_popup = true;
    }
    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Editor")) {
    ImGui::MenuItem ("Toggle Play Mode");
    ImGui::MenuItem ("Editor Settings");
    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Window")) {
    ImGui::MenuItem ("Welcome", nullptr, &m_show_welcome);
    ImGui::MenuItem ("Log", nullptr, &m_show_console);
    ImGui::MenuItem ("Systems", nullptr, &m_show_system_inspector);
    ImGui::MenuItem ("Resources", nullptr, &m_show_resources);
    ImGui::MenuItem ("ECS Inspector", nullptr, &m_show_ecs_inspector);
    ImGui::MenuItem ("Input Map", nullptr, &m_show_input_map);
    ImGui::MenuItem ("Console", nullptr, &m_show_new_console);
    ImGui::MenuItem ("Agent", nullptr, &m_show_agent);
    ImGui::MenuItem ("Game View", nullptr, &m_show_game_view);
    ImGui::MenuItem ("Signals", nullptr, &m_show_signal_inspector);
    ImGui::MenuItem ("Build", nullptr, &m_show_build_inspector);
    ImGui::MenuItem ("Text Editor", nullptr, &m_show_text_editor);
    ImGui::MenuItem ("Files", nullptr, &m_show_file_list);
    ImGui::Separator ();

    if (ImGui::MenuItem ("Reset Layout")) {
      m_dock_layout_built = false;
      ImGui::GetIO ().WantSaveIniSettings = true;
    }

    ImGui::EndMenu ();
  }

  if (ImGui::BeginMenu ("Debug")) {
    if (ImGui::MenuItem ("Open Profiler (Tracy)")) {
      open_tracy ();
    }

    if (ImGui::MenuItem ("Open Debugger (gf2)")) {
      open_gf2 ();
    }

    ImGui::EndMenu ();
  }

  ImGui::EndMainMenuBar ();
}

void
editor::root::draw_new_project_popup ()
{
  if (ImGui::BeginPopupModal ("New Project", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {

    ImGui::InputText ("Project Name", m_new_project_name,
                      sizeof (m_new_project_name));

    ImGui::Text ("Project Folder");
    ImGui::PushID ("project_folder");

    ImGui::SetNextItemWidth (-40);
    ImGui::InputText ("##folder", m_new_project_folder,
                      sizeof (m_new_project_folder));

    ImGui::SameLine ();

    if (ImGui::Button ("...")) {
      open_project_folder_dialog ();
    }

    ImGui::PopID ();

    ImGui::Checkbox ("Create Folder", &m_new_project_create_folder);

    ImGui::Separator ();

    if (ImGui::Button ("Create")) {
      std::string root = m_new_project_folder;

      if (m_new_project_create_folder) {
        root += "/";
        root += m_new_project_name;
      }

      wsl::rsc::project proj;
      proj.name = m_new_project_name;
      proj.author = "Unknown";
      proj.root_path = root;
      proj.systems_path = "src/systems";
      proj.components_path = "src/components";
      proj.singletons_path = "src/singletons";
      proj.ui_layouts_path = "src/ui";
      proj.scenes_path = "rsc/scenes";
      proj.models_path = "rsc/models";
      proj.images_path = "rsc/textures";
      proj.cubemaps_path = "rsc/textures/cubemaps";
      proj.audio_path = "rsc/audio";
      proj.fonts_path = "rsc/fonts";
      proj.default_scene_path = "";

      m_selection = {};
      m_editor_ctx->reset_editor_camera ();
      m_runtime_ctx->resource_manager.new_project (proj);

      const std::string project_file
          = (std::filesystem::path (proj.root_path) / "wslpro.json").string ();
      update_recent_projects (project_file);

      m_show_welcome = false;
      ImGui::CloseCurrentPopup ();
    }

    ImGui::SameLine ();

    if (ImGui::Button ("Cancel")) {
      ImGui::CloseCurrentPopup ();
    }

    ImGui::EndPopup ();
  }
}

void
editor::root::open_project_file ()
{
  m_current_dialog_mode = dialog_mode::open_project;
  if (m_waiting_for_file_dialog) {
    return;
  }

  SDL_DialogFileFilter filters[]
      = { { "WSL Project", "json" }, { "All Files", "*" } };

  m_waiting_for_file_dialog = true;
  m_dialog_result.reset ();

  SDL_ShowOpenFileDialog (file_dialog_callback,          // callback
                          this,                          // userdata
                          m_runtime_ctx->window.handler, // your SDL_Window*
                          filters, SDL_arraysize (filters),
                          nullptr, // default location
                          false    // allow_many
  );
}

void
editor::root::save_project_file ()
{
  if (m_waiting_for_file_dialog) {
    return;
  }

  SDL_DialogFileFilter filters[]
      = { { "Project Files", "project;json" }, { "All Files", "*" } };

  m_waiting_for_file_dialog = true;
  m_dialog_result.reset ();

  SDL_ShowSaveFileDialog (file_dialog_callback, this,
                          m_runtime_ctx->window.handler, filters,
                          SDL_arraysize (filters), nullptr);
}

void
editor::root::file_dialog_callback (void *userdata, const char *const *filelist,
                                    int /*unused*/)
{
  auto *self = static_cast<editor::root *> (userdata);

  if (filelist == nullptr) {
    // Error
    self->m_dialog_result = std::nullopt;
  } else if (filelist[0] == nullptr) {
    // User cancelled
    self->m_dialog_result = std::nullopt;
  } else {
    // Take first file only
    self->m_dialog_result = std::string (filelist[0]);
  }

  self->m_waiting_for_file_dialog = false;
}

void
editor::root::open_project_folder_dialog ()
{
  m_current_dialog_mode = dialog_mode::select_folder;

  if (m_waiting_for_file_dialog) {
    return;
  }

  m_waiting_for_file_dialog = true;
  m_dialog_result.reset ();

  SDL_ShowOpenFolderDialog (file_dialog_callback, this,
                            m_runtime_ctx->window.handler, nullptr, false);
}

void
editor::root::apply_preferences_style ()
{
  // Apply font
  ImGuiIO &io = ImGui::GetIO ();

  // Your load order is: regular, light, medium, semibold, bold, mono
  // So indices 0..4 are the UI fonts, and mono is 5.
  if ((io.Fonts != nullptr) && io.Fonts->Fonts.Size >= 5) {
    int idx = m_pref_font_index;
    idx = std::max (0, std::min (4, idx));
    io.FontDefault = io.Fonts->Fonts[idx];
  }

  io.FontGlobalScale = m_pref_font_scale;

  // Apply theme -> derived style colors
  wsl::gfx::editor_theme t{};
  t.primary = m_pref_primary;
  t.secondary = m_pref_secondary;
  t.background1 = m_pref_bg1;
  t.background2 = m_pref_bg2;
  t.foreground = m_pref_fg;

  m_editor_ctx->get_imgui_renderer ()->apply_editor_style (t);
}

void
editor::root::draw_preferences_popup ()
{
  bool open = true; // controls the X button on the modal

  // Optional: center it
  ImGuiViewport const *vp = ImGui::GetMainViewport ();
  ImGui::SetNextWindowPos (vp->GetCenter (), ImGuiCond_Appearing,
                           ImVec2 (0.5F, 0.5F));

  if (ImGui::BeginPopupModal ("Preferences", &open,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::BeginTabBar ("prefs_tabs")) {

      if (ImGui::BeginTabItem ("Style")) {
        const char *font_items[]
            = { "Regular", "Light", "Medium", "Semibold", "Bold" };
        ImGui::TextUnformatted ("Font");
        ImGui::SetNextItemWidth (220.0F);
        ImGui::Combo ("##font_combo", &m_pref_font_index, font_items,
                      IM_ARRAYSIZE (font_items));

        ImGui::TextUnformatted ("Font scale");
        ImGui::SetNextItemWidth (220.0F);
        ImGui::SliderFloat ("##font_scale", &m_pref_font_scale, 0.75F, 1.50F,
                            "%.2f");

        ImGui::Separator ();

        ImGui::TextUnformatted ("Engine colors");
        ImGuiColorEditFlags const flags
            = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayRGB;
        ImGui::ColorEdit3 ("Primary", (float *)&m_pref_primary, flags);
        ImGui::ColorEdit3 ("Secondary", (float *)&m_pref_secondary, flags);
        ImGui::ColorEdit3 ("Background 1", (float *)&m_pref_bg1, flags);
        ImGui::ColorEdit3 ("Background 2", (float *)&m_pref_bg2, flags);
        ImGui::ColorEdit3 ("Foreground", (float *)&m_pref_fg, flags);

        ImGui::Spacing ();
        ImGui::Separator ();
        ImGui::Spacing ();

        if (ImGui::Button ("Apply")) {
          apply_preferences_style ();
        }
        ImGui::SameLine ();
        if (ImGui::Button ("Close")) {
          ImGui::CloseCurrentPopup ();
          m_show_preferences = false;
        }

        ImGui::EndTabItem ();
      }

      if (ImGui::BeginTabItem ("General")) {
        ImGui::TextDisabled ("Nothing here yet.");
        ImGui::EndTabItem ();
      }

      ImGui::EndTabBar ();
    }

    // If user clicked the X
    if (!open) {
      ImGui::CloseCurrentPopup ();
      m_show_preferences = false;
    }

    ImGui::EndPopup ();
    return;
  }

  // If we reach here: popup is not open this frame.
  // Only clear if we are not supposed to keep it open.
  if (!ImGui::IsPopupOpen ("Preferences", ImGuiPopupFlags_AnyPopupId)) {
    // Keep show_preferences as-is unless you explicitly want it to drop.
    // Safer:
    // show_preferences = false;
  }
}

void
editor::root::draw_build_settings_popup ()
{
  bool open = true;
  ImGuiViewport const *vp = ImGui::GetMainViewport ();
  ImGui::SetNextWindowPos (vp->GetCenter (), ImGuiCond_Appearing,
                           ImVec2 (0.5F, 0.5F));

  if (ImGui::BeginPopupModal ("Build Settings", &open,
                              ImGuiWindowFlags_AlwaysAutoResize)) {

    ImGui::TextUnformatted ("WSL Source Path");
    ImGui::SetNextItemWidth (400.0F);

    char buf[512];
    std::strncpy (buf, m_editor_ctx->wsl_library_path.c_str (), sizeof (buf));
    if (ImGui::InputText ("##wsl_path", buf, sizeof (buf))) {
      m_editor_ctx->wsl_library_path = buf;
    }

    ImGui::TextUnformatted ("WSL Resource Path");
    ImGui::SetNextItemWidth (400.0F);

    char res_buf[512];
    std::strncpy (
        res_buf,
        m_runtime_ctx->resource_manager.get_engine_resource_path ().c_str (),
        sizeof (res_buf));
    if (ImGui::InputText ("##wsl_res_path", res_buf, sizeof (res_buf))) {
      m_runtime_ctx->resource_manager.set_engine_resource_path (res_buf);
    }

    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();

    if (ImGui::Button ("Close")) {
      ImGui::CloseCurrentPopup ();
      m_show_build_settings = false;
    }

    ImGui::EndPopup ();
  }

  if (!open) {
    ImGui::CloseCurrentPopup ();
    m_show_build_settings = false;
  }
}

void
editor::root::draw_project_settings_popup ()
{
  bool open = true;
  ImGuiViewport const *vp = ImGui::GetMainViewport ();
  ImGui::SetNextWindowPos (vp->GetCenter (), ImGuiCond_Appearing,
                           ImVec2 (0.5F, 0.5F));

  if (ImGui::BeginPopupModal ("Project Settings", &open,
                              ImGuiWindowFlags_AlwaysAutoResize)) {

    auto proj = m_runtime_ctx->resource_manager.current_project ();
    if (!proj) {
      ImGui::TextDisabled ("No project loaded.");
      ImGui::Spacing ();
      if (ImGui::Button ("Close")) {
        ImGui::CloseCurrentPopup ();
        m_show_project_settings = false;
      }
      ImGui::EndPopup ();
      return;
    }

    // Project Name
    ImGui::TextUnformatted ("Project Name");
    ImGui::SetNextItemWidth (400.0F);
    char name_buf[128];
    std::strncpy (name_buf, proj->name.c_str (), sizeof (name_buf));
    if (ImGui::InputText ("##proj_name", name_buf, sizeof (name_buf))) {
      proj->name = name_buf;
    }

    // Author
    ImGui::TextUnformatted ("Author");
    ImGui::SetNextItemWidth (400.0F);
    char author_buf[128];
    std::strncpy (author_buf, proj->author.c_str (), sizeof (author_buf));
    if (ImGui::InputText ("##proj_author", author_buf, sizeof (author_buf))) {
      proj->author = author_buf;
    }

    // Default Scene
    ImGui::TextUnformatted ("Default Scene");
    ImGui::SetNextItemWidth (400.0F);

    auto scenes = m_runtime_ctx->resource_manager.list_scenes ();
    std::vector<std::string> scene_paths;
    scene_paths.reserve (scenes.size ());
    int current_scene_idx = -1;
    for (const auto &rec : scenes) {
      if (rec.is_prefab)
        continue;
      scene_paths.push_back (rec.path);
      if (rec.path == proj->default_scene_path) {
        current_scene_idx = static_cast<int> (scene_paths.size ()) - 1;
      }
    }

    // Build preview name list
    std::vector<std::string> scene_names;
    scene_names.reserve (scene_paths.size ());
    for (const auto &path : scene_paths) {
      std::filesystem::path p (path);
      scene_names.push_back (p.filename ().string ());
    }

    // Build null-terminated string array for Combo
    std::string combo_items;
    for (const auto &name : scene_names) {
      combo_items += name;
      combo_items += '\0';
    }
    if (!combo_items.empty ()) {
      combo_items.pop_back (); // remove trailing null
    }

    int selected_idx = current_scene_idx >= 0 ? current_scene_idx : 0;
    if (ImGui::Combo ("##default_scene", &selected_idx, combo_items.c_str ())) {
      if (selected_idx >= 0
          && selected_idx < static_cast<int> (scene_paths.size ())) {
        proj->default_scene_path = scene_paths[selected_idx];
      }
    }

    // Resource Paths
    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();
    ImGui::TextUnformatted ("Resource Paths");

    auto draw_path_input = [&] (const char *label, std::string &value) {
      char buf[512];
      std::strncpy (buf, value.c_str (), sizeof (buf));
      if (ImGui::InputText (label, buf, sizeof (buf))) {
        value = buf;
      }
    };

    draw_path_input ("Systems##systems_path", proj->systems_path);
    draw_path_input ("Components##components_path", proj->components_path);
    draw_path_input ("Singletons##singletons_path", proj->singletons_path);
    draw_path_input ("Scenes##scenes_path", proj->scenes_path);
    draw_path_input ("Models##models_path", proj->models_path);
    draw_path_input ("Images##images_path", proj->images_path);
    draw_path_input ("Cubemaps##cubemaps_path", proj->cubemaps_path);
    draw_path_input ("Audio##audio_path", proj->audio_path);
    draw_path_input ("UI Layouts##ui_layouts_path", proj->ui_layouts_path);
    draw_path_input ("Fonts##fonts_path", proj->fonts_path);
    draw_path_input ("Shaders##shaders_path", proj->shaders_path);

    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();

    if (ImGui::Button ("Save")) {
      std::filesystem::path manifest
          = std::filesystem::path (proj->root_path)
            / wsl::rsc::project_loader::manifest_file;
      std::ofstream file (manifest);
      if (file) {
        cereal::JSONOutputArchive archive (file);
        archive (cereal::make_nvp ("project", *proj));
        wsl::log::editor ()->info ("Saved project settings to {}",
                                   manifest.string ());
      } else {
        wsl::log::editor ()->error ("Failed to save project settings to {}",
                                    manifest.string ());
      }
      ImGui::CloseCurrentPopup ();
      m_show_project_settings = false;
    }
    ImGui::SameLine ();
    if (ImGui::Button ("Cancel")) {
      ImGui::CloseCurrentPopup ();
      m_show_project_settings = false;
    }

    ImGui::EndPopup ();
  }

  if (!open) {
    ImGui::CloseCurrentPopup ();
    m_show_project_settings = false;
  }
}

void
editor::root::draw_welcome_tab ()
{
  if (ImGui::Begin ("Welcome", &m_show_welcome)) {
    // Draw background image anchored to bottom-right
    if (m_editor_ctx->icon_welcome_bg.value != entt::null) {
      if (m_editor_ctx->editor_resources.state (m_editor_ctx->icon_welcome_bg)
          == wsl::rsc::image_state::loaded) {
        auto handle = m_editor_ctx->editor_resources.get (
            m_editor_ctx->icon_welcome_bg);
        if (handle) {
          ImVec2 const window_pos = ImGui::GetWindowPos ();
          ImVec2 const window_size = ImGui::GetWindowSize ();
          float const img_size = std::min (window_size.x, window_size.y) * 0.7F;
          ImVec2 const p_min = ImVec2 (window_pos.x + window_size.x - img_size,
                                       window_pos.y + window_size.y - img_size);
          ImVec2 const p_max = ImVec2 (window_pos.x + window_size.x,
                                       window_pos.y + window_size.y);
          ImGui::GetWindowDrawList ()->AddImage (
              (ImTextureID)handle->texture.get (), p_min, p_max, ImVec2 (0, 0),
              ImVec2 (1, 1), ImColor (255, 255, 255, 255));
        }
      }
    }

    ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts ().title);
    ImGui::Text ("weasel");
    ImGui::PopFont ();

    ImGui::Spacing ();
    ImGui::Separator ();
    ImGui::Spacing ();

    if (ImGui::BeginTable ("WelcomeActions", 2, ImGuiTableFlags_None)) {
      ImGui::TableNextRow ();

      // Left Panel: Create a new project
      ImGui::TableSetColumnIndex (0);
      {
        float const avail = ImGui::GetContentRegionAvail ().x;
        const char *text = "Create a new project";
        float const tw = ImGui::CalcTextSize (text).x;
        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - tw) * 0.5F));
        ImGui::Text ("%s", text);

        ImGui::Spacing ();

        float const bw = 150.0F;
        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - bw) * 0.5F));
        if (ImGui::Button ("New Project", ImVec2 (bw, 40))) {
          m_request_new_project_popup = true;
        }
      }

      // Right Panel: Load an existing project
      ImGui::TableSetColumnIndex (1);
      {
        float const avail = ImGui::GetContentRegionAvail ().x;
        const char *text = "Load a existing project";
        float const tw = ImGui::CalcTextSize (text).x;
        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - tw) * 0.5F));
        ImGui::Text ("%s", text);

        ImGui::Spacing ();

        float const bw = 200.0F;
        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - bw) * 0.5F));
        if (ImGui::Button ("Load Local Project", ImVec2 (bw, 30))) {
          open_project_file ();
        }

        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - bw) * 0.5F));
        ImGui::BeginDisabled ();
        ImGui::Button ("Load Remote Project", ImVec2 (bw, 30));
        ImGui::EndDisabled ();

        ImGui::SetCursorPosX (ImGui::GetCursorPosX () + ((avail - bw) * 0.5F));
        ImGui::BeginDisabled ();
        ImGui::Button ("Load Example/Demo", ImVec2 (bw, 30));
        ImGui::EndDisabled ();
      }

      ImGui::EndTable ();
    }

    ImGui::Spacing ();
    ImGui::Text ("Recent Projects");
    ImGui::Separator ();

    if (m_recent_projects.empty ()) {
      ImGui::TextDisabled ("No recent projects.");
    } else {
      std::string project_to_remove;
      for (const auto &path : m_recent_projects) {
        ImGui::PushID (path.c_str ());
        std::filesystem::path const p (path);
        std::string label = p.parent_path ().filename ().string ();
        if (label.empty ()) {
          label = path;
        }

        if (ImGui::Selectable (
                label.c_str (), false, 0,
                ImVec2 (ImGui::GetContentRegionAvail ().x - 25, 0))) {
          m_selection = {};
          m_runtime_ctx->editor_ctx->selected_entity = entt::null;
          m_runtime_ctx->editor_ctx->pending_project_load = path;
          update_recent_projects (path);
          m_show_welcome = false;
        }

        if (ImGui::IsItemHovered ()) {
          ImGui::SetTooltip ("%s", path.c_str ());
        }

        ImGui::SameLine ();
        if (ImGui::SmallButton ("x")) {
          project_to_remove = path;
        }
        ImGui::PopID ();
      }

      if (!project_to_remove.empty ()) {
        auto it = std::find (m_recent_projects.begin (),
                             m_recent_projects.end (), project_to_remove);
        if (it != m_recent_projects.end ()) {
          m_recent_projects.erase (it);
          save_recent_projects ();
        }
      }
    }
  }
  ImGui::End ();
}

void
editor::root::update_recent_projects (const std::string &path)
{
  std::string path_copy = path; // Make a copy because path might be a reference
                                // to a vector element
  auto it = std::find (m_recent_projects.begin (), m_recent_projects.end (),
                       path_copy);
  if (it != m_recent_projects.end ()) {
    m_recent_projects.erase (it);
  }
  m_recent_projects.insert (m_recent_projects.begin (), path_copy);

  if (m_recent_projects.size () > 10) {
    m_recent_projects.pop_back ();
  }
  save_recent_projects ();
}

void
editor::root::load_recent_projects ()
{
  std::ifstream file ("recent_projects.txt");
  if (file) {
    std::string line;
    while (std::getline (file, line)) {
      if (!line.empty ()) {
        m_recent_projects.push_back (line);
      }
    }
  }
}

void
editor::root::save_recent_projects ()
{
  std::ofstream file ("recent_projects.txt");
  if (file) {
    for (const auto &path : m_recent_projects) {
      file << path << "\n";
    }
  }
}

void
editor::root::draw_status_bar ()
{
  job_manager::get ().update ();

  const ImGuiViewport *viewport = ImGui::GetMainViewport ();
  float const height = 24.0F;
  float const progress_bar_height = 2.0F;

  ImGui::SetNextWindowPos (
      ImVec2 (viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
  ImGui::SetNextWindowSize (ImVec2 (viewport->Size.x, height));

  ImGuiWindowFlags const window_flags
      = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::PushStyleVar (ImGuiStyleVar_WindowPadding, ImVec2 (0, 0));
  ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleColor (ImGuiCol_WindowBg,
                         ImGui::GetStyleColorVec4 (ImGuiCol_MenuBarBg));

  ImGui::Begin ("##StatusBar", nullptr, window_flags);

  // 0. FPS counter (left-anchored)
  {
    ImGuiIO const &io = ImGui::GetIO ();
    char fps_buf[32];
    snprintf (fps_buf, sizeof (fps_buf), "%.1f FPS", io.Framerate);
    float const text_y_offset
        = ((height - progress_bar_height - ImGui::GetTextLineHeight ()) * 0.5F)
          + progress_bar_height;
    ImGui::SetCursorPos (ImVec2 (10.0F, text_y_offset));
    ImGui::TextDisabled ("%s", fps_buf);
  }

  auto active_jobs = job_manager::get ().get_active_jobs ();

  // 1. Progress bar (2px)
  if (!active_jobs.empty ()) {
    const auto &current_job = active_jobs.front ();
    float progress = current_job.progress;
    if (progress < 0.0F) {
      // Indeterminate
      progress = (float)ImGui::GetTime () * 0.5F;
      progress = std::fmod (progress, 1.0F);
    }

    ImVec2 const min = ImGui::GetWindowPos ();
    ImVec2 const max
        = ImVec2 (min.x + viewport->Size.x, min.y + progress_bar_height);
    ImGui::GetWindowDrawList ()->AddRectFilled (
        min, max, ImGui::GetColorU32 (ImGuiCol_FrameBg));

    ImVec2 const progress_max
        = ImVec2 (min.x + (viewport->Size.x * progress), max.y);
    ImGui::GetWindowDrawList ()->AddRectFilled (
        min, progress_max, ImGui::GetColorU32 (ImGuiCol_PlotHistogram));
  }

  // 2. Text below it, anchored to the right
  std::string status_text;
  if (!active_jobs.empty ()) {
    status_text = active_jobs.front ().name;
  } else if (m_editor_ctx->is_loading_project) {
    status_text = "Loading project...";
  } else {
    auto last_info = job_manager::get ().get_last_finished_info ();
    if (last_info) {
      status_text = *last_info;
    } else {
      auto *active_scene = m_runtime_ctx->scene_manager.get_active ();
      if (active_scene != nullptr) {
        status_text = active_scene->get_name ();
      } else {
        status_text = "No project loaded";
      }
    }
  }

  float const text_y_offset
      = ((height - progress_bar_height - ImGui::GetTextLineHeight ()) * 0.5F)
        + progress_bar_height;
  ImVec2 const text_size = ImGui::CalcTextSize (status_text.c_str ());
  ImGui::SetCursorPos (
      ImVec2 (viewport->Size.x - text_size.x - 10.0F, text_y_offset));
  ImGui::TextUnformatted (status_text.c_str ());

  ImGui::End ();
  ImGui::PopStyleColor ();
  ImGui::PopStyleVar (2);
}

} // namespace editor
