#pragma once

#include "wsl/comp/singl/runtime_context_fwd.hpp"
#include "wsl/rsc/scene.hpp"
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <sstream>
#include <memory>

namespace wsl::cli
{

/**
 * Selects how eagerly the runtime module is loaded before a command runs.
 */
enum class runtime_load_mode
{
  /**
   * Fast path: allow cached registration metadata, escalating to a full
   * compile automatically when the cache is missing, stale, or empty.
   */
  auto_mode,

  /** Always compile from source when nothing is loaded (scene-load
   * semantics). */
  force_full
};

// Command executor that works with an existing runtime_context
class command_executor
{
public:
  explicit command_executor (wsl::comp::singl::runtime_context &rtc);

  // Set the current project (used when running inside editor server)
  void set_current_project (std::shared_ptr<wsl::rsc::project> proj);

  // Execute a command and return the output
  std::string execute (const std::string &line);

  // Enable auto-save after mutation commands (used for one-shot CLI mode)
  void
  set_auto_save (bool enabled)
  {
    m_auto_save = enabled;
  }

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
  void cmd_prefab (const std::vector<std::string> &tokens);
  void cmd_script (const std::vector<std::string> &tokens);
  void cmd_das (const std::vector<std::string> &tokens);
  void cmd_play (const std::vector<std::string> &tokens);
  void cmd_help ();

  entt::entity resolve_entity_token (const std::string &token,
                                     wsl::rsc::scene *scene);

  /**
   * Makes sure user runtime code is usable before a command runs.
   *
   * :param mode: \c auto_mode trusts the registration metadata cache and
   *   escalates to a full compile whenever the cache is missing, stale, or
   *   empty while sources exist; \c force_full always compiles.
   * :return: ``true`` if the runtime is usable (or the project has none),
   *   ``false`` when a full compile was attempted and failed.
   */
  bool ensure_runtime (runtime_load_mode mode);

  wsl::rsc::scene *get_active_scene ();

  void auto_save_scene (bool verbose = true);
  void auto_save_project ();

  wsl::comp::singl::runtime_context &m_rtc;
  std::shared_ptr<wsl::rsc::project> m_current_project;
  std::ostringstream m_output;
  bool m_auto_save = false;
  std::string m_active_scene_source_path;

  // One-shot exit code, set when a command wants to fail the process
  // (e.g. `proj load --strict-runtime` with a runtime compile failure).
  int m_exit_code = 0;

public:
  int
  exit_code () const
  {
    return m_exit_code;
  }
};

} // namespace wsl::cli
