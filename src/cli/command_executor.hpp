#pragma once

#include "wsl/comp/singl/runtime_context.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <memory>

namespace wsl::cli
{

// Command executor that works with an existing runtime_context
class command_executor
{
public:
  explicit command_executor (wsl::comp::singl::runtime_context &rtc);

  // Set the current project (used when running inside editor server)
  void set_current_project (std::shared_ptr<wsl::rsc::project> proj);

  // Execute a command and return the output
  std::string execute (const std::string &line);

  static std::vector<std::string> tokenize (const std::string &line);

private:
  void cmd_proj (const std::vector<std::string> &tokens);
  void cmd_scene (const std::vector<std::string> &tokens);
  void cmd_ent (const std::vector<std::string> &tokens);
  void cmd_comp (const std::vector<std::string> &tokens);
  void cmd_singl (const std::vector<std::string> &tokens);
  void cmd_sig (const std::vector<std::string> &tokens);
  void cmd_sys (const std::vector<std::string> &tokens);
  void cmd_check (const std::vector<std::string> &tokens);
  void cmd_rsc (const std::vector<std::string> &tokens);
  void cmd_help ();

  void ensure_runtime_module_loaded ();

  wsl::rsc::scene *get_active_scene ();

  wsl::comp::singl::runtime_context &m_rtc;
  std::shared_ptr<wsl::rsc::project> m_current_project;
  std::ostringstream m_output;
};

} // namespace wsl::cli
