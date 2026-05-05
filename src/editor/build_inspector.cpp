#include "build_inspector.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "job_manager.hpp"
#include <algorithm>
#include <future>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <utility>

namespace editor
{

build_inspector::build_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                                  wsl::comp::singl::editor_context *editor_ctx)
    : m_runtime_ctx (runtime_ctx),
      m_editor_ctx (editor_ctx)
{
}

namespace
{
void
draw_hsplitter (const char *id, float &top_height, float thickness = 6.0F)
{
  ImGui::InvisibleButton (id, ImVec2 (-1, thickness));

  // Nice cursor while hovering
  if (ImGui::IsItemHovered ()) {
    ImGui::SetMouseCursor (ImGuiMouseCursor_ResizeNS);
}

  if (ImGui::IsItemActive ()) {
    top_height += ImGui::GetIO ().MouseDelta.y;
  }

  // Draw the splitter bar
  ImU32 col = ImGui::GetColorU32 (ImGuiCol_Separator);
  if (ImGui::IsItemHovered () || ImGui::IsItemActive ()) {
    col = ImGui::GetColorU32 (ImGuiCol_SeparatorHovered);
}

  ImVec2 const min = ImGui::GetItemRectMin ();
  ImVec2 const max = ImGui::GetItemRectMax ();
  ImGui::GetWindowDrawList ()->AddRectFilled (min, max, col);
}
} // namespace

void
build_inspector::draw ()
{
  if (!ImGui::Begin ("Build", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End ();
    return;
  }

  auto project = m_runtime_ctx->resource_manager.current_project ();
  const bool has_project = project != nullptr;

  auto active_jobs = job_manager::get ().get_active_jobs ();
  const bool is_busy = !active_jobs.empty ();
  
  ImGui::BeginDisabled (is_busy || !has_project);

  const float spacing = ImGui::GetStyle ().ItemSpacing.y;
  const float total_h = ImGui::GetContentRegionAvail ().y;

  // Kits Section
  ImGui::AlignTextToFramePadding ();
  ImGui::TextUnformatted ("Configure");
  ImGui::SameLine (ImGui::GetContentRegionAvail ().x - ImGui::CalcTextSize ("Configure Project").x - (ImGui::GetStyle ().FramePadding.x * 2.0F));

  const bool can_configure = has_project && m_selected_kit_idx >= 0;
  ImGui::BeginDisabled (!can_configure);
  if (ImGui::Button ("Configure Project")) {
    configure_project (m_kits[m_selected_kit_idx]);
  }
  ImGui::EndDisabled ();

  // Clamp Kits height
  m_kits_height = std::clamp (m_kits_height, 40.0F, total_h * 0.5F);

  if (ImGui::BeginChild ("##KitsRegion", ImVec2 (-1, m_kits_height), 1)) {
    for (size_t i = 0; i < m_kits.size (); ++i) {
      const bool is_selected = (m_selected_kit_idx == (int)i);
      if (ImGui::Selectable (m_kits[i].c_str (), is_selected)) {
        m_selected_kit_idx = (int)i;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }
  }
  ImGui::EndChild ();

  draw_hsplitter ("##kits_splitter", m_kits_height);

  // Build Targets Section
  ImGui::AlignTextToFramePadding ();
  ImGui::TextUnformatted ("Build Targets");
  ImGui::SameLine (ImGui::GetContentRegionAvail ().x - ImGui::CalcTextSize ("Build Target").x - (ImGui::GetStyle ().FramePadding.x * 2.0F));

  const bool can_build = has_project && m_project_info.has_value () && m_selected_target_idx >= 0;
  ImGui::BeginDisabled (!can_build);
  if (ImGui::Button ("Build Target")) {
    build_target (m_project_info->targets[m_selected_target_idx].name);
  }
  ImGui::EndDisabled ();

  // Calculate remaining room for targets and tests
  float const room_left = ImGui::GetContentRegionAvail ().y 
                  - ImGui::GetTextLineHeightWithSpacing () // Tests header line
                  - 40.0F;                                 // Minimal room for Tests ListBox

  m_targets_height = std::clamp (m_targets_height, 40.0F, room_left);

  if (ImGui::BeginChild ("##TargetsRegion", ImVec2 (-1, m_targets_height), 1)) {
    if (m_project_info.has_value ()) {
      if (m_selected_target_idx >= (int)m_project_info->targets.size ()) { m_selected_target_idx = -1;
}
      for (size_t i = 0; i < m_project_info->targets.size (); ++i) {
        const bool is_selected = (m_selected_target_idx == (int)i);
        if (ImGui::Selectable (m_project_info->targets[i].name.c_str (), is_selected)) {
          m_selected_target_idx = (int)i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus ();
        }
      }
    } else {
      ImGui::TextDisabled ("No project info. Click Refresh.");
    }
  }
  ImGui::EndChild ();

  draw_hsplitter ("##targets_splitter", m_targets_height);

  // Tests Section
  ImGui::AlignTextToFramePadding ();
  ImGui::TextUnformatted ("Tests");

  // Right-align the "Run Test" and "Refresh Info" buttons on the header line
  float const refresh_btn_w = ImGui::CalcTextSize ("Refresh Info").x + (ImGui::GetStyle ().FramePadding.x * 2.0F);
  float const run_btn_w = ImGui::CalcTextSize ("Run Test").x + (ImGui::GetStyle ().FramePadding.x * 2.0F);
  ImGui::SameLine (ImGui::GetContentRegionAvail ().x - refresh_btn_w - run_btn_w - ImGui::GetStyle ().ItemSpacing.x);

  const bool can_run_test = has_project && m_project_info.has_value () && m_selected_test_idx >= 0;
  ImGui::BeginDisabled (!can_run_test);
  if (ImGui::Button ("Run Test")) {
    run_test (m_project_info->tests[m_selected_test_idx].name);
  }
  ImGui::EndDisabled ();

  ImGui::SameLine ();
  if (ImGui::Button ("Refresh Info")) {
    refresh_cmake_info ();
  }

  // Calculate remaining space for Tests list
  float const list_h = ImGui::GetContentRegionAvail ().y - spacing;

  if (ImGui::BeginChild ("##TestsRegion", ImVec2 (-1, std::max (40.0F, list_h)), 1)) {
    if (m_project_info.has_value ()) {
      if (m_selected_test_idx >= (int)m_project_info->tests.size ()) { m_selected_test_idx = -1;
}
      for (size_t i = 0; i < m_project_info->tests.size (); ++i) {
        const bool is_selected = (m_selected_test_idx == (int)i);
        if (ImGui::Selectable (m_project_info->tests[i].name.c_str (), is_selected)) {
          m_selected_test_idx = (int)i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus ();
        }
      }
    }
  }
  ImGui::EndChild ();

  ImGui::EndDisabled ();

  ImGui::End ();
}




void
build_inspector::refresh_cmake_info ()
{
  auto project = m_runtime_ctx->resource_manager.current_project ();
  if (!project) { return;
}

  std::string const name = "Refreshing CMake Info";
  auto future = std::async (std::launch::async, [this, root_path = project->root_path] () {
    std::string const build_dir = root_path + "/build";
    
    std::string extra_args;
    if (!m_editor_ctx->wsl_library_path.empty ()) {
      extra_args = "-DWeasel_DIR=" + m_editor_ctx->wsl_library_path;
    }

    if (m_cmake_api.query_and_configure (root_path, build_dir, extra_args, m_runtime_ctx->resource_manager)) {
      this->m_project_info = m_cmake_api.parse_replies (build_dir);
    }
  });
  job_manager::get ().add_job (name, std::move (future));
}

void
build_inspector::build_target (const std::string &target_name)
{
  auto project = m_runtime_ctx->resource_manager.current_project ();
  if (!project) { return;
}

  std::string const name = "Building Target " + target_name;
  auto future = std::async (std::launch::async, [root_path = project->root_path, target_name] () {
    std::string const build_dir = root_path + "/build";
    std::string command = "cmake --build " + build_dir + " --target " + target_name;

    spdlog::info ("build_inspector: executing {}", command);
    std::system (command.c_str ());
  });
  job_manager::get ().add_job (name, std::move (future));
}

void
build_inspector::configure_project (const std::string &kit_name)
{
  auto project = m_runtime_ctx->resource_manager.current_project ();
  if (!project) { return;
}

  std::string const name = "Configuring Project (" + kit_name + ")";
  auto future = std::async (std::launch::async, [this, root_path = project->root_path, kit_name] () {
    std::string const build_dir = root_path + "/build";

    // Use the kit name to set build type for now.
    std::string extra_args;
    if (!m_editor_ctx->wsl_library_path.empty ()) {
      extra_args = "-DWeasel_DIR=" + m_editor_ctx->wsl_library_path;
    }

    if (kit_name != "Default") {
      if (!extra_args.empty ()) { extra_args += " ";
}
      extra_args += "-DCMAKE_BUILD_TYPE=" + kit_name;
    }

    spdlog::info ("build_inspector: configuring with kit {}", kit_name);
    if (m_cmake_api.query_and_configure (root_path, build_dir, extra_args, m_runtime_ctx->resource_manager)) {
      this->m_project_info = m_cmake_api.parse_replies (build_dir);
    }
  });
  job_manager::get ().add_job (name, std::move (future));
}

void
build_inspector::run_test (const std::string &test_name)
{
  auto project = m_runtime_ctx->resource_manager.current_project ();
  if (!project) { return;
}

  std::string const name = "Running Test " + test_name;
  auto future = std::async (std::launch::async, [root_path = project->root_path, test_name] () {
    std::string const build_dir = root_path + "/build";
    std::string command = "ctest --test-dir " + build_dir + " -R ^" + test_name + "$";

    spdlog::info ("build_inspector: executing {}", command);
    std::system (command.c_str ());
  });
  job_manager::get ().add_job (name, std::move (future));
}

} // namespace editor
