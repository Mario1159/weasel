#include "system_inspector.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "rsc/resource_ids.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/sys/core_systems.hpp"
#include "ecs_inspector.hpp" // ecs_selection

#include <entt/entity/fwd.hpp>
#include <imgui.h>
#include <imsearch.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace editor
{

system_inspector::system_inspector (
    wsl::comp::singl::runtime_context *runtime_ctx,
    wsl::comp::singl::editor_context *editor_ctx, ecs_selection *selection)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_selection (selection)
{
}

void
system_inspector::set_visible (bool value)
{
  m_visible = value;
}

bool
system_inspector::is_visible () const
{
  return m_visible;
}

// One list renderer (filtered + selectable row + checkbox on same line)
static void
draw_system_list (const std::vector<wsl::sys::ecs_system *> &systems,
                  wsl::comp::singl::runtime_context *runtime_ctx,
                  wsl::comp::singl::editor_context *editor_ctx,
                  editor::ecs_selection *selection, bool is_core = false)
{
  if (systems.empty ()) {
    return;
  }

  const float gap = 8.0F;
  const float pad_right = 2.0F;

  const float row_h = ImGui::GetFrameHeight ();      // checkbox height
  const float checkbox_w = ImGui::GetFrameHeight (); // good approximation
  entt::registry *registry = nullptr;
  if (runtime_ctx != nullptr) {
    if (auto *scene = runtime_ctx->scene_manager.get_active ()) {
      registry = &scene->get_registry ();
    }
  }
  const bool is_playing = (runtime_ctx != nullptr) && runtime_ctx->is_running;

  for (wsl::sys::ecs_system *system : systems) {
    if (system == nullptr) {
      continue;
    }

    const std::string &name = system->get_name ();
    ImSearch::SearchableItem (name.c_str (), [&] (const char *) {
      ImGui::PushID (system);

      const bool is_selected
          = (selection && selection->selected_system == system);

      // Allocate only part of the row for Selectable, leaving room for
      // activation toggles
      float const avail = ImGui::GetContentRegionAvail ().x;
      float select_w = avail - (checkbox_w * 2.0F) - (gap * 2.0F) - pad_right;
      select_w = std::max (select_w, 80.0f);

      // Make selectable the same height as checkbox
      if (ImGui::Selectable ("##system_row", is_selected, 0,
                             ImVec2 (select_w, row_h))) {
        if (selection) {
          selection->select_system (system);
        }
      }

      // DRAW ACTIVATION Toggles over the selectable (AllowItemOverlap style)
      ImGui::SetCursorScreenPos (ImVec2 (ImGui::GetItemRectMin ().x + 4.0F,
                                         ImGui::GetItemRectMin ().y));
      ImGui::AlignTextToFramePadding ();

      if (is_core) {
        ImGui::PushStyleColor (
            ImGuiCol_Text,
            editor_ctx->get_imgui_renderer ()->get_theme ().primary);
      }
      ImGui::TextUnformatted (name.c_str ());
      if (is_core) {
        ImGui::PopStyleColor ();
      }

      // Right-aligned buttons
      ImGui::SameLine (avail - (checkbox_w * 2.0F) - gap - pad_right);

      // Editor activation (show/hide)
      bool const ed_active = system->is_editor_active ();
      wsl::rsc::image_id const ed_icon
          = ed_active ? editor_ctx->icon_show : editor_ctx->icon_hide;
      auto ed_handle = editor_ctx->editor_resources.get (ed_icon);

      ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0, 0, 0, 0));
      if (ed_handle && (*ed_handle).texture) {
        if (ImGui::ImageButton ("##ed_active_btn",
                                (ImTextureID)(*ed_handle).texture,
                                ImVec2 (checkbox_w - 4, checkbox_w - 4))) {
          system->set_editor_active (!ed_active);
        }
      } else {
        editor_ctx->editor_resources.load (ed_icon);
        if (ImGui::Button (ed_active ? "S##ed" : "H##ed",
                           ImVec2 (checkbox_w, checkbox_w))) {
          system->set_editor_active (!ed_active);
        }
      }
      ImGui::PopStyleColor ();

      if (ImGui::IsItemHovered ()) {
        ImGui::SetTooltip ("Render/Active on Editor");
      }

      ImGui::SameLine (avail - checkbox_w - pad_right);

      // Runtime activation (play/empty OR checkbox when playing)
      if (is_playing) {
        bool active = system->is_active ();
        if (ImGui::Checkbox ("##rt_active_cb", &active)) {
          system->set_active (active, registry);
        }
        if (ImGui::IsItemHovered ()) {
          ImGui::SetTooltip ("Active at Runtime (Live)");
        }
      } else {
        bool const rt_active = system->is_init_on_startup ();

        ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0, 0, 0, 0));
        if (rt_active) {
          auto rt_handle
              = editor_ctx->editor_resources.get (editor_ctx->icon_play);
          if (rt_handle && (*rt_handle).texture) {
            if (ImGui::ImageButton ("##rt_active_btn",
                                    (ImTextureID)(*rt_handle).texture,
                                    ImVec2 (checkbox_w - 4, checkbox_w - 4))) {
              system->set_init_on_startup (!rt_active, registry, is_playing);
            }
          } else {
            editor_ctx->editor_resources.load (editor_ctx->icon_play);
            if (ImGui::Button (rt_active ? "P##rt" : " ##rt",
                               ImVec2 (checkbox_w, checkbox_w))) {
              system->set_init_on_startup (!rt_active, registry, is_playing);
            }
          }
        } else {
          // Empty box for unchecked runtime
          if (ImGui::Button ("##rt_active_empty",
                             ImVec2 (checkbox_w, checkbox_w))) {
            system->set_init_on_startup (!rt_active, registry, is_playing);
          }
        }
        ImGui::PopStyleColor ();

        if (ImGui::IsItemHovered ()) {
          ImGui::SetTooltip ("Active at Runtime (Init on Startup)");
        }
      }

      ImGui::PopID ();
    });
  }
}

void
system_inspector::draw ()
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  if (!ImGui::Begin ("Systems", &m_visible)) {
    ImGui::End ();
    return;
  }

  ecs_selection *selection_ptr = m_selection;

  wsl::rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();
  const bool has_scene = (scene != nullptr);

  ImGui::AlignTextToFramePadding ();
  ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts ().bold);
  ImGui::TextUnformatted ("Systems");
  ImGui::PopFont ();
  ImGui::SameLine ();

  if (!has_scene) {
    ImGui::BeginDisabled ();
  }

  draw_add_system_ui ();
  ImGui::SameLine ();

  const bool can_remove = (selection_ptr != nullptr)
                          && (selection_ptr->selected_system != nullptr);
  bool is_app_system = false;
  if (can_remove && has_scene) {
    auto app_systems = scene->get_systems ();
    if (std::find (app_systems.begin (), app_systems.end (),
                   selection_ptr->selected_system)
        != app_systems.end ()) {
      is_app_system = true;
    }
  }

  if (!is_app_system && has_scene) {
    ImGui::BeginDisabled ();
  }

  if (ImGui::Button ("-##remove_system", ImVec2 (ImGui::GetFrameHeight (),
                                                 ImGui::GetFrameHeight ()))) {
    if (scene != nullptr) {
      scene->remove_system (selection_ptr->selected_system);
      selection_ptr->clear_all ();
    }
  }

  if (!is_app_system && has_scene) {
    ImGui::EndDisabled ();
  }

  if (!has_scene) {
    ImGui::EndDisabled ();
  }

  ImGui::PushID ("SystemsSearchID");
  if (ImSearch::BeginSearch ()) {
    ImSearch::SearchBar ("Search");

    ImGui::BeginChild ("SystemsRegion", ImVec2 (0, 0), 1);

    if (m_runtime_ctx->resource_manager.current_project ()) {
      // Core systems
      draw_system_list (m_runtime_ctx->core_systems->to_vec (), m_runtime_ctx,
                        m_editor_ctx, selection_ptr, true);

      // App systems
      if (has_scene) {
        draw_system_list (scene->get_systems (), m_runtime_ctx, m_editor_ctx,
                          selection_ptr, false);
      } else {
        ImGui::TextDisabled ("(No active scene)");
      }
    } else {
      draw_centered_icon (
          m_editor_ctx, m_editor_ctx->icon_system, 128.0F,
          "Systems contain all the logic of the game,\nload a project to get "
          "the core systems of your project\nand code your own systems.");
    }

    ImSearch::Submit ();
    ImGui::EndChild ();
    ImSearch::EndSearch ();
  }
  ImGui::PopID ();

  ImGui::End ();
}

void
system_inspector::draw_add_system_ui ()
{
  wsl::rsc::scene *scene = m_runtime_ctx->scene_manager.get_active ();

  if (ImGui::Button ("+##add_system", ImVec2 (ImGui::GetFrameHeight (),
                                              ImGui::GetFrameHeight ()))) {
    ImGui::OpenPopup ("AddSystemPopup");
  }

  if ((scene != nullptr) && ImGui::BeginPopup ("AddSystemPopup")) {
    ImGui::PushID ("AddSystemSearchID");
    if (ImSearch::BeginSearch ()) {
      ImSearch::SearchBar ("Search Systems");
      ImGui::Separator ();

      auto systems = m_runtime_ctx->system_factory_registry.get_systems ();
      auto scene_systems = scene->get_systems ();

      for (const auto *desc : systems) {
        if (!desc->runtime_registered) {
          continue;
        }

        bool already_present = false;
        for (const auto *sys : scene_systems) {
          if (sys->get_type_id () == desc->type_id) {
            already_present = true;
            break;
          }
        }
        if (already_present) {
          continue;
        }

        ImSearch::SearchableItem (
            desc->display_name.c_str (), [&, desc] (const char *) {
              if (ImGui::MenuItem (desc->display_name.c_str ())) {
                if (auto sys = m_runtime_ctx->system_factory_registry.create (
                        desc->display_name, *scene)) {
                  scene->add_system_instance (std::move (sys));
                }
              }
            });
      }
      ImSearch::Submit ();
      ImSearch::EndSearch ();
    }
    ImGui::PopID ();
    ImGui::EndPopup ();
  }
}

} // namespace editor
