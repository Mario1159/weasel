#pragma once

#include "wsl/comp/singl/runtime_context.hpp"
#include "editor_client.hpp"
#include "command_executor.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <sstream>

namespace wsl::cli
{

class repl_handler
{
public:
  repl_handler (const std::string &engine_res_path, bool attach = false);
  bool prepare (std::optional<std::string> initial_project = std::nullopt,
                std::optional<std::string> initial_scene = std::nullopt);
  void run (std::optional<std::string> initial_project = std::nullopt,
            std::optional<std::string> initial_scene = std::nullopt);

  void execute_command (const std::string &line);

  void set_auto_save (bool enabled);

private:
  void ensure_local_executor ();

  std::unique_ptr<wsl::comp::singl::runtime_context> m_rtc;
  std::shared_ptr<wsl::rsc::project> m_current_project;
  bool m_running = true;
  std::string m_engine_res_path;
  bool m_attach = false;
  editor_client m_editor_client;
  std::unique_ptr<command_executor> m_local_executor;
};

} // namespace wsl::cli
