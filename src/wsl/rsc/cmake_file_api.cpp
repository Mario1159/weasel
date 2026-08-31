#include "cmake_file_api.hpp"
#include "../gfx/cubemap.hpp"
#include "resource_manager.hpp"

#include "wsl/log/log.hpp"
#include <yyjson.h>
#include <memory>
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
                                     [[maybe_unused]] resource_manager &res_mgr)
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
  command += " \"-DWeasel_RESOURCE_PATH=" + res_mgr.get_engine_resource_path ()
             + "\"";
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
    wsl::log::cmake ()->error (
        "cmake_file_api: failed to create query directory: {}", ec.message ());
    return false;
  }

  const fs::path query_file = query_dir / "codemodel-v2";
  std::ofstream out (query_file);
  if (!out) {
    wsl::log::cmake ()->error (
        "cmake_file_api: failed to create query file: {}",
        query_file.string ());
    return false;
  }
  out.close ();

  const fs::path test_query_file = query_dir / "ctest-v1";
  std::ofstream const test_out (test_query_file);
  if (!test_out) {
    wsl::log::cmake ()->error (
        "cmake_file_api: failed to create test query file: {}",
        test_query_file.string ());
    return false;
  }

  // An empty file is a valid query for version 2 and v1.
  return true;
}

namespace
{

// Minimal yyjson navigation helpers (replacement for cereal's rapidjson).
yyjson_val *
jget (yyjson_val *obj, const char *key)
{
  if (obj == nullptr || !yyjson_is_obj (obj)) {
    return nullptr;
  }
  return yyjson_obj_get (obj, key);
}

std::unique_ptr<yyjson_doc, void (*) (yyjson_doc *)>
jparse (const std::string &content)
{
  yyjson_doc *doc = yyjson_read (content.data (), content.size (), 0);
  return { doc, &yyjson_doc_free };
}

std::string
jstr (yyjson_val *val)
{
  return (val != nullptr && yyjson_is_str (val)) ? yyjson_get_str (val) : "";
}

} // namespace

std::optional<cmake_file_api::project_info>
cmake_file_api::parse_replies (const fs::path &build_dir)
{
  const fs::path reply_dir = build_dir / ".cmake/api/v1/reply";
  if (!fs::exists (reply_dir)) {
    wsl::log::cmake ()->error ("cmake_file_api: reply directory not found: {}",
                               reply_dir.string ());
    return std::nullopt;
  }

  auto index_path = find_index_file (reply_dir);
  if (!index_path) {
    return std::nullopt;
  }

  std::ifstream index_file (*index_path);
  std::string const index_content (
      (std::istreambuf_iterator<char> (index_file)),
      std::istreambuf_iterator<char> ());

  auto index_doc = jparse (index_content);
  if (index_doc == nullptr) {
    wsl::log::cmake ()->error ("cmake_file_api: failed to parse index file");
    return std::nullopt;
  }
  yyjson_val *index_root = yyjson_doc_get_root (index_doc.get ());

  // Find the codemodel-v2 reply in the index.
  yyjson_val *reply = jget (index_root, "reply");
  yyjson_val *codemodel = jget (reply, "codemodel-v2");
  yyjson_val *codemodel_file_val = jget (codemodel, "jsonFile");
  if (codemodel_file_val == nullptr) {
    wsl::log::cmake ()->error (
        "cmake_file_api: index does not contain codemodel-v2 reply");
    return std::nullopt;
  }

  std::string const codemodel_file = yyjson_get_str (codemodel_file_val);
  fs::path const codemodel_path = reply_dir / codemodel_file;

  std::ifstream cm_file (codemodel_path);
  if (!cm_file) {
    wsl::log::cmake ()->error (
        "cmake_file_api: failed to open codemodel file: {}",
        codemodel_path.string ());
    return std::nullopt;
  }
  std::string const cm_content ((std::istreambuf_iterator<char> (cm_file)),
                                std::istreambuf_iterator<char> ());

  auto cm_doc = jparse (cm_content);
  if (cm_doc == nullptr) {
    wsl::log::cmake ()->error (
        "cmake_file_api: failed to parse codemodel file");
    return std::nullopt;
  }
  yyjson_val *cm_root = yyjson_doc_get_root (cm_doc.get ());

  project_info info;
  info.name = jstr (jget (jget (cm_root, "project"), "name"));

  yyjson_val *configurations = jget (cm_root, "configurations");
  if (configurations == nullptr || !yyjson_is_arr (configurations)
      || yyjson_arr_size (configurations) == 0) {
    return std::nullopt;
  }

  // We parse the first configuration (usually Debug or Release).
  yyjson_val *config = yyjson_arr_get (configurations, 0);
  yyjson_val *targets = jget (config, "targets");
  if (targets == nullptr || !yyjson_is_arr (targets)) {
    return std::nullopt;
  }

  yyjson_val *target_ref = nullptr;
  yyjson_arr_iter target_iter = yyjson_arr_iter_with (targets);
  while ((target_ref = yyjson_arr_iter_next (&target_iter)) != nullptr) {
    yyjson_val *target_file_val = jget (target_ref, "jsonFile");
    if (target_file_val == nullptr) {
      continue;
    }

    fs::path const target_path = reply_dir / yyjson_get_str (target_file_val);
    auto target_info = parse_target_file (target_path);
    if (target_info) {
      info.targets.push_back (std::move (*target_info));
    }
  }

  // Find the ctest-v1 reply in the index.
  yyjson_val *ctest = jget (reply, "ctest-v1");
  yyjson_val *test_file_val = jget (ctest, "jsonFile");
  if (test_file_val != nullptr) {
    std::string const test_file = yyjson_get_str (test_file_val);
    fs::path const test_path = reply_dir / test_file;

    std::ifstream t_file (test_path);
    if (t_file) {
      std::string const t_content ((std::istreambuf_iterator<char> (t_file)),
                                   std::istreambuf_iterator<char> ());

      auto t_doc = jparse (t_content);
      if (t_doc != nullptr) {
        yyjson_val *tests = jget (yyjson_doc_get_root (t_doc.get ()), "tests");
        if (tests != nullptr && yyjson_is_arr (tests)) {
          yyjson_val *test_node = nullptr;
          yyjson_arr_iter test_iter = yyjson_arr_iter_with (tests);
          while ((test_node = yyjson_arr_iter_next (&test_iter)) != nullptr) {
            cmake_test_info test;
            test.name = jstr (jget (test_node, "name"));
            yyjson_val *command = jget (test_node, "command");
            if (command != nullptr && yyjson_is_arr (command)
                && yyjson_arr_size (command) > 0) {
              test.command = jstr (yyjson_arr_get (command, 0));
              for (size_t i = 1; i < yyjson_arr_size (command); ++i) {
                test.arguments.push_back (
                    jstr (yyjson_arr_get (command, i)));
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
    if (entry.is_regular_file ()
        && entry.path ().filename ().string ().starts_with ("index-")) {
      return entry.path ();
    }
  }
  wsl::log::cmake ()->error ("cmake_file_api: could not find index file in {}",
                             reply_dir.string ());
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

  auto doc = jparse (content);
  if (doc == nullptr) {
    return std::nullopt;
  }
  yyjson_val *root = yyjson_doc_get_root (doc.get ());

  cmake_target_info target;
  target.name = jstr (jget (root, "name"));
  target.type = jstr (jget (root, "type"));

  // Extract include directories and defines from compileGroups.
  yyjson_val *compile_groups = jget (root, "compileGroups");
  if (compile_groups != nullptr && yyjson_is_arr (compile_groups)) {
    yyjson_val *group = nullptr;
    yyjson_arr_iter group_iter = yyjson_arr_iter_with (compile_groups);
    while ((group = yyjson_arr_iter_next (&group_iter)) != nullptr) {
      yyjson_val *includes = jget (group, "includes");
      if (includes != nullptr && yyjson_is_arr (includes)) {
        yyjson_val *inc = nullptr;
        yyjson_arr_iter inc_iter = yyjson_arr_iter_with (includes);
        while ((inc = yyjson_arr_iter_next (&inc_iter)) != nullptr) {
          target.include_directories.push_back (jstr (jget (inc, "path")));
        }
      }
      yyjson_val *fragments = jget (group, "compileCommandFragments");
      if (fragments != nullptr && yyjson_is_arr (fragments)) {
        yyjson_val *frag = nullptr;
        yyjson_arr_iter frag_iter = yyjson_arr_iter_with (fragments);
        while ((frag = yyjson_arr_iter_next (&frag_iter)) != nullptr) {
          std::string const fragment = jstr (jget (frag, "fragment"));
          // Simple extraction of -D flags.
          if (fragment.starts_with ("-D")) {
            target.compile_definitions.push_back (fragment.substr (2));
          }
        }
      }
    }
  }

  // Extract link libraries.
  yyjson_val *link = jget (root, "link");
  yyjson_val *link_fragments = jget (link, "commandFragments");
  if (link_fragments != nullptr && yyjson_is_arr (link_fragments)) {
    yyjson_val *frag = nullptr;
    yyjson_arr_iter frag_iter = yyjson_arr_iter_with (link_fragments);
    while ((frag = yyjson_arr_iter_next (&frag_iter)) != nullptr) {
      std::string const fragment = jstr (jget (frag, "fragment"));
      // We only care about actual library paths/names, skipping flags for
      // now.
      if (!fragment.starts_with ("-")) {
        target.link_libraries.push_back (fragment);
      }
    }
  }

  return target;
}

} // namespace cmake

} // namespace rsc

} // namespace wsl
