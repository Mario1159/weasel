#include "cmake_file_api.hpp"
#include "resource_manager.hpp"

#include "wsl/log/log.hpp"
#include <cereal/external/rapidjson/document.h>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace wsl
{

namespace rsc
{

namespace cmake
{

namespace fs = std::filesystem;
bool
cmake_file_api::query_and_configure (const fs::path &project_root,
                                     const fs::path &build_dir,
                                     const std::string &extra_args,
                                     resource_manager &res_mgr)
{

  if (!write_query (build_dir)) {
    return false;
  }

  std::string command = "cmake -S " + project_root.string () + " -B "
                        + build_dir.string ()
                        + " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";

#ifdef WEASEL_SOURCE_DIR
  // Point Weasel_DIR to source dir where WeaselConfig.cmake lives
  command += " -DWeasel_DIR=" + std::string (WEASEL_SOURCE_DIR);
  // Also keep SOURCE_DIR for add_subdirectory usage in new projects
  command += " -DWeasel_SOURCE_DIR=" + std::string (WEASEL_SOURCE_DIR);
#endif

#ifdef WEASEL_BUILD_DIR
  // Pass the build directory of the engine so we can find pre-built binaries
  command += " -DWeasel_BUILD_DIR=" + std::string (WEASEL_BUILD_DIR);
  // Default resource path to build dir if not set otherwise
  command += " \"-DWeasel_RESOURCE_PATH=" + res_mgr.get_engine_resource_path() + "\"";
#endif


  if (!extra_args.empty ()) {
    command += " " + extra_args;
  }
  
  wsl::log::cmake ()->debug ("cmake_file_api: executing {}", command);
  int const result = std::system (command.c_str ());
  
  return result == 0;
}

bool
cmake_file_api::write_query (const fs::path &build_dir) 
{
  const fs::path query_dir = build_dir / ".cmake/api/v1/query";
  std::error_code ec;
  fs::create_directories (query_dir, ec);
  
  if (ec) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to create query directory: {}", ec.message ());
    return false;
  }

  const fs::path query_file = query_dir / "codemodel-v2";
  std::ofstream out (query_file);
  if (!out) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to create query file: {}", query_file.string ());
    return false;
  }
  out.close ();

  const fs::path test_query_file = query_dir / "ctest-v1";
  std::ofstream const test_out (test_query_file);
  if (!test_out) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to create test query file: {}", test_query_file.string ());
    return false;
  }

  // An empty file is a valid query for version 2 and v1.
  return true;
}

std::optional<cmake_file_api::project_info>
cmake_file_api::parse_replies (const fs::path &build_dir)
{
  const fs::path reply_dir = build_dir / ".cmake/api/v1/reply";
  if (!fs::exists (reply_dir)) {
    wsl::log::cmake ()->error ("cmake_file_api: reply directory not found: {}", reply_dir.string ());
    return std::nullopt;
  }

  auto index_path = find_index_file (reply_dir);
  if (!index_path) {
    return std::nullopt;
  }

  std::ifstream index_file (*index_path);
  std::string const index_content ((std::istreambuf_iterator<char> (index_file)),
                             std::istreambuf_iterator<char> ());

  rapidjson::Document index_doc;
  if (index_doc.Parse (index_content.c_str ()).HasParseError ()) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to parse index file");
    return std::nullopt;
  }

  // Find the codemodel-v2 reply in the index.
  if (!index_doc.HasMember ("reply") || !index_doc["reply"].IsObject ()) {
    return std::nullopt;
  }

  const auto &reply = index_doc["reply"];
  if (!reply.HasMember ("codemodel-v2") || !reply["codemodel-v2"].IsObject ()
      || !reply["codemodel-v2"].HasMember ("jsonFile")) {
    wsl::log::cmake ()->error ("cmake_file_api: index does not contain codemodel-v2 reply");
    return std::nullopt;
  }

  std::string const codemodel_file = reply["codemodel-v2"]["jsonFile"].GetString ();
  fs::path const codemodel_path = reply_dir / codemodel_file;

  std::ifstream cm_file (codemodel_path);
  if (!cm_file) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to open codemodel file: {}", codemodel_path.string ());
    return std::nullopt;
  }
  std::string const cm_content ((std::istreambuf_iterator<char> (cm_file)),
                          std::istreambuf_iterator<char> ());

  rapidjson::Document cm_doc;
  if (cm_doc.Parse (cm_content.c_str ()).HasParseError ()) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to parse codemodel file");
    return std::nullopt;
  }

  project_info info;
  if (cm_doc.HasMember ("project") && cm_doc["project"].IsObject ()) {
    info.name = cm_doc["project"]["name"].GetString ();
  }

  if (!cm_doc.HasMember ("configurations") || !cm_doc["configurations"].IsArray ()
      || cm_doc["configurations"].Empty ()) {
    return std::nullopt;
  }

  // We parse the first configuration (usually Debug or Release).
  const auto &config = cm_doc["configurations"][0];
  if (!config.HasMember ("targets") || !config["targets"].IsArray ()) {
    return std::nullopt;
  }

  for (const auto &target_ref : config["targets"].GetArray ()) {
    if (!target_ref.IsObject () || !target_ref.HasMember ("jsonFile")) {
      continue;
    }

    fs::path const target_path = reply_dir / target_ref["jsonFile"].GetString ();
    auto target_info = parse_target_file (target_path);
    if (target_info) {
      info.targets.push_back (std::move (*target_info));
    }
  }

  // Find the ctest-v1 reply in the index.
  if (reply.HasMember ("ctest-v1") && reply["ctest-v1"].IsObject ()
      && reply["ctest-v1"].HasMember ("jsonFile")) {
    std::string const test_file = reply["ctest-v1"]["jsonFile"].GetString ();
    fs::path const test_path = reply_dir / test_file;

    std::ifstream t_file (test_path);
    if (t_file) {
      std::string const t_content ((std::istreambuf_iterator<char> (t_file)),
                              std::istreambuf_iterator<char> ());

      rapidjson::Document t_doc;
      if (!t_doc.Parse (t_content.c_str ()).HasParseError ()) {
        if (t_doc.HasMember ("tests") && t_doc["tests"].IsArray ()) {
          for (const auto &test_node : t_doc["tests"].GetArray ()) {
            cmake_test_info test;
            test.name = test_node["name"].GetString ();
            if (test_node.HasMember ("command") && test_node["command"].IsArray ()
                && !test_node["command"].Empty ()) {
              test.command = test_node["command"][0].GetString ();
              for (size_t i = 1; i < test_node["command"].Size (); ++i) {
                test.arguments.push_back (test_node["command"][i].GetString ());
              }
            }
            info.tests.push_back (std::move (test));
          }
        }
      }
    }
  }

  return info;
}

std::optional<fs::path>
cmake_file_api::find_index_file (const fs::path &reply_dir) 
{
  for (const auto &entry : fs::directory_iterator (reply_dir)) {
    if (entry.is_regular_file () && entry.path ().filename ().string ().starts_with ("index-")) {
      return entry.path ();
    }
  }
  wsl::log::cmake ()->error ("cmake_file_api: could not find index file in {}", reply_dir.string ());
  return std::nullopt;
}

std::optional<cmake_target_info>
cmake_file_api::parse_target_file (const fs::path &target_json_path) 
{
  std::ifstream file (target_json_path);
  if (!file) {
    return std::nullopt;
  }

  std::string const content ((std::istreambuf_iterator<char> (file)),
                       std::istreambuf_iterator<char> ());

  rapidjson::Document doc;
  if (doc.Parse (content.c_str ()).HasParseError ()) {
    return std::nullopt;
  }

  cmake_target_info target;
  target.name = doc["name"].GetString ();
  target.type = doc["type"].GetString ();

  // Extract include directories and defines from compileGroups.
  if (doc.HasMember ("compileGroups") && doc["compileGroups"].IsArray ()) {
    for (const auto &group : doc["compileGroups"].GetArray ()) {
      if (group.HasMember ("includes") && group["includes"].IsArray ()) {
        for (const auto &inc : group["includes"].GetArray ()) {
          target.include_directories.push_back (inc["path"].GetString ());
        }
      }
      if (group.HasMember ("compileCommandFragments") && group["compileCommandFragments"].IsArray ()) {
          for (const auto &frag : group["compileCommandFragments"].GetArray ()) {
              std::string const fragment = frag["fragment"].GetString ();
              // Simple extraction of -D flags.
              if (fragment.starts_with ("-D")) {
                  target.compile_definitions.push_back (fragment.substr (2));
              }
          }
      }
    }
  }

  // Extract link libraries.
  if (doc.HasMember ("link") && doc["link"].IsObject ()) {
    const auto &link = doc["link"];
    if (link.HasMember ("commandFragments") && link["commandFragments"].IsArray ()) {
      for (const auto &frag : link["commandFragments"].GetArray ()) {
        std::string const fragment = frag["fragment"].GetString ();
        // We only care about actual library paths/names, skipping flags for now.
        if (!fragment.starts_with ("-")) {
          target.link_libraries.push_back (fragment);
        }
      }
    }
  }

  return target;
}

} // namespace cmake

} // namespace rsc

} // namespace wsl
