#pragma once

#ifndef IN_MODULE_INTERFACE
#include <filesystem>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif
#ifndef IN_MODULE_INTERFACE
#include <optional>
#endif
#include "resource_manager.hpp"

namespace wsl
{

namespace rsc
{

namespace xmake
{

/** Parsed information about an xmake target. */
struct target_info
{
  std::string name;
  std::string type; // binary, static, shared, phony, ...
  std::vector<std::string> include_directories;
  std::vector<std::string> compile_definitions;
  std::vector<std::string> link_libraries;
};

/** Parsed information about an xmake test. */
struct test_info
{
  std::string name;
  std::string command;
  std::vector<std::string> arguments;
};

/**
 * Interface for interacting with the xmake build system.
 *
 * This class triggers project configuration and queries target/test
 * metadata using xmake's JSON output (xmake show --format=json).
 */
class xmake_file_api
{
public:
  /** Represents the result of a project metadata query. */
  struct project_info
  {
    std::string name;
    std::vector<target_info> targets;
    std::vector<test_info> tests;
  };

  /**
   * Configures the project and triggers an xmake run.
   * :param project_root: Path to the project containing xmake.lua.
   * :param build_dir: Path to the build directory.
   * :param extra_args: Additional arguments to pass to xmake f.
   * :param res_mgr: Reference to the resource manager to get engine settings.
   * :return: \c true if configuration succeeded.
   */
  bool query_and_configure (const std::filesystem::path &project_root,
                            const std::filesystem::path &build_dir,
                            const std::string &extra_args,
                            resource_manager &res_mgr);

  /**
   * Queries project metadata via xmake's JSON output.
   * :param project_root: Path to the project containing xmake.lua.
   * :param build_dir: Path to the build directory.
   * :return: The parsed project information, or \c std::nullopt on failure.
   */
  std::optional<project_info>
  parse_replies (const std::filesystem::path &project_root,
                 const std::filesystem::path &build_dir);

private:
  static std::string exec (const std::string &command);
  static std::optional<std::vector<std::string>>
  list_targets (const std::filesystem::path &project_root);
  static std::vector<std::string>
  list_tests (const std::filesystem::path &project_root);
  static std::optional<target_info>
  parse_target (const std::filesystem::path &project_root,
                const std::string &target_name);
};

} // namespace xmake

} // namespace rsc

} // namespace wsl
