#pragma once

#include "wsl/rsc/cmake_file_api.hpp"
#include <string>
#include <vector>
#include <future>

namespace wsl::comp::singl { class runtime_context; class editor_context; }

namespace editor
{

class build_inspector
{
public:
  build_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                   wsl::comp::singl::editor_context *editor_ctx);

  void draw ();

private:
  void refresh_cmake_info ();
  void build_target (const std::string &target_name);
  void configure_project (const std::string &kit_name);
  void run_test (const std::string &test_name);

  wsl::comp::singl::runtime_context *m_runtime_ctx;
  wsl::comp::singl::editor_context *m_editor_ctx;
  wsl::rsc::cmake::cmake_file_api m_cmake_api;
  std::optional<wsl::rsc::cmake::cmake_file_api::project_info> m_project_info;

  int m_selected_kit_idx = -1;
  int m_selected_target_idx = -1;
  int m_selected_test_idx = -1;

  float m_kits_height = 80.0F;
  float m_targets_height = 120.0F;

  std::vector<std::string> m_kits = { "Default", "Debug", "Release", "RelWithDebInfo" };
};

} // namespace editor
