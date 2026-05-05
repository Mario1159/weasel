#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "resource_manager.hpp"

namespace wsl
{

namespace rsc
{

namespace cmake
{

/*!
 * \brief Parsed information about a CMake target.
 */
struct cmake_target_info
{
  std::string name;
  std::string type; // EXECUTABLE, SHARED_LIBRARY, etc.
  std::vector<std::string> include_directories;
  std::vector<std::string> compile_definitions;
  std::vector<std::string> link_libraries;
};

/*!
 * \brief Parsed information about a CMake test.
 */
struct cmake_test_info
{
  std::string name;
  std::string command;
  std::vector<std::string> arguments;
};

/*!
 * \brief Interface for interacting with the CMake File API.
 *
 * This class handles creating API queries, triggering CMake configuration,
 * and parsing the resulting reply files to extract project structure metadata.
 */
class cmake_file_api
{
public:
  /*!
   * \brief Represents the result of a project metadata query.
   */
  struct project_info
  {
    std::string name;
    std::vector<cmake_target_info> targets;
    std::vector<cmake_test_info> tests;
  };

  /*!
   * \brief Configures the API query and triggers a CMake run.
   * \param project_root Path to the project containing CMakeLists.txt.
   * \param build_dir Path to the directory where CMake should generate files.
   * \param extra_args Additional arguments to pass to the CMake command.
   * \param res_mgr Reference to the resource manager to get engine settings.
   * \return \c true if the API query was successfully configured and executed.
   */
  bool query_and_configure (const std::filesystem::path &project_root,
                            const std::filesystem::path &build_dir,
                            const std::string &extra_args,
                            resource_manager &res_mgr);

  /*!
   * \brief Parses the CMake API replies from a build directory.
   * \param build_dir Path to the build directory containing the .cmake/api/v1/reply folder.
   * \return The parsed project information, or \c std::nullopt on failure.
   */
  std::optional<project_info> parse_replies (const std::filesystem::path &build_dir);

private:
  static bool write_query (const std::filesystem::path &build_dir) ;
  
  static std::optional<std::filesystem::path> 
  find_index_file (const std::filesystem::path &reply_dir) ;

  static std::optional<cmake_target_info>
  parse_target_file (const std::filesystem::path &target_json_path) ;
};

} // namespace cmake

} // namespace rsc

} // namespace wsl
