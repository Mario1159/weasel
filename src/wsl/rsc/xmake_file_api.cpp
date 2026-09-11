#include "xmake_file_api.hpp"
#include "resource_manager.hpp"

#include "wsl/log/log.hpp"
#include <cereal/external/rapidjson/document.h>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace wsl
{

namespace rsc
{

namespace xmake
{

namespace fs = std::filesystem;

namespace
{
// Flags from xmake show target JSON that we do not map into target_info.
bool
is_flag_fragment (const std::string &value)
{
  return !value.empty () && value.front () == '-';
}

std::string
quoted (const std::string &path)
{
  return "\"" + path + "\"";
}

// xmake JSON array entries can be plain strings or objects of the shape
// { source = {...}, value = "..." }.
std::string
string_value (const rapidjson::Value &value)
{
  if (value.IsString ()) {
    return value.GetString ();
  }
  if (value.IsObject () && value.HasMember ("value")
      && value["value"].IsString ()) {
    return value["value"].GetString ();
  }
  return {};
}

// xmake may append plain-text warnings after the JSON payload.
bool
parse_json (const std::string &output, rapidjson::Document &doc)
{
  const size_t first = output.find_first_of ("{[");
  const size_t last = output.find_last_of ("}]");
  if (first == std::string::npos || last == std::string::npos || last < first) {
    return false;
  }
  return !doc.Parse (output.substr (first, last - first + 1).c_str ())
              .HasParseError ();
}

// xmake prints diagnostics and warnings with ANSI escapes on stdout; test
// names must survive them.
std::vector<std::string>
clean_lines (const std::string &output)
{
  std::vector<std::string> lines;
  std::string plain;
  plain.reserve (output.size ());
  for (size_t i = 0; i < output.size ();) {
    if (output[i] == '\033') {
      ++i;
      // Skip a CSI sequence (ESC '[' ... final byte) or lone escape char.
      if (i < output.size () && output[i] == '[') {
        ++i;
        while (i < output.size () && !isalpha ((unsigned char)output[i])) {
          ++i;
        }
        ++i;
      } else {
        ++i;
      }
      continue;
    }
    plain += output[i++];
  }

  size_t start = 0;
  while (start <= plain.size ()) {
    size_t const end = plain.find ('\n', start);
    std::string const line = plain.substr (
        start, end == std::string::npos ? std::string::npos : end - start);
    if (end == std::string::npos) {
      start = plain.size () + 1;
    } else {
      start = end + 1;
    }
    if (!line.empty ()) {
      lines.push_back (line);
    }
    if (end == std::string::npos) {
      break;
    }
  }
  return lines;
}

// A test name is a plain identifier; anything else (xmake warnings,
// diagnostics) is discarded.
bool
is_test_name (const std::string &line)
{
  if (line.empty ()) {
    return false;
  }
  for (const char c : line) {
    if (!isalnum ((unsigned char)c) && c != '_' && c != '-' && c != '.'
        && c != '/' && c != '*') {
      return false;
    }
  }
  return true;
}
} // namespace

std::string
xmake_file_api::exec (const std::string &command)
{
  std::string output;
  FILE *pipe = popen (command.c_str (), "r");
  if (!pipe) {
    wsl::log::xmake ()->error ("xmake_file_api: failed to run: {}", command);
    return {};
  }

  std::array<char, 4096> buffer{};
  while (fgets (buffer.data (), (int)buffer.size (), pipe) != nullptr) {
    output += buffer.data ();
  }

  int const status = pclose (pipe);
  if (status != 0) {
    wsl::log::xmake ()->error ("xmake_file_api: command failed ({}): {}",
                               status, command);
    return {};
  }
  return output;
}

bool
xmake_file_api::query_and_configure (const fs::path &project_root,
                                     const fs::path &build_dir,
                                     const std::string &extra_args,
                                     [[maybe_unused]] resource_manager &res_mgr)
{
  std::string command
      = "cd " + quoted (project_root.string ()) + " && xmake f -y";
  if (!build_dir.empty ()) {
    command += " --buildir=" + quoted (build_dir.string ());
  }

  if (!extra_args.empty ()) {
    command += " " + extra_args;
  }

  wsl::log::xmake ()->debug ("xmake_file_api: executing {}", command);
  int const result = std::system (command.c_str ());
  return result == 0;
}

std::optional<std::vector<std::string>>
xmake_file_api::list_targets (const fs::path &project_root)
{
  std::string const command = "cd " + quoted (project_root.string ())
                              + " && xmake show -l targets --format=json";
  std::string const output = exec (command);
  if (output.empty ()) {
    return std::nullopt;
  }
  rapidjson::Document doc;
  if (!parse_json (output, doc) || !doc.IsArray ()) {
    wsl::log::xmake ()->error ("xmake_file_api: failed to parse targets list");
    return std::nullopt;
  }

  std::vector<std::string> targets;
  for (const auto &name : doc.GetArray ()) {
    std::string const value = string_value (name);
    if (!value.empty ()) {
      targets.push_back (value);
    }
  }
  return targets;
}

std::vector<std::string>
xmake_file_api::list_tests (const fs::path &project_root)
{
  // xmake does not expose tests via `show` in all versions, so enumerate
  // them with the embedded Lua API instead.
  static constexpr const char *script
      = "import(\"core.project.project\"); import(\"core.project.config\"); "
        "config.load(); project.load_targets(); "
        "for _, t in pairs(project.targets()) do "
        "local tests = t:get(\"tests\"); "
        "if tests then for _, n in ipairs(tests) do print(n) end end end";

  std::string const command = std::string ("cd ")
                              + quoted (project_root.string ()) + " && echo '"
                              + script + "' | xmake lua --stdin";
  std::string const output = exec (command);

  std::vector<std::string> tests;
  for (const std::string &line : clean_lines (output)) {
    if (is_test_name (line)) {
      tests.push_back (line);
    }
  }
  return tests;
}

std::optional<target_info>
xmake_file_api::parse_target (const fs::path &project_root,
                              const std::string &target_name)
{
  std::string const command = "cd " + quoted (project_root.string ())
                              + " && xmake show -t " + quoted (target_name)
                              + " --format=json";
  std::string const output = exec (command);
  if (output.empty ()) {
    return std::nullopt;
  }

  rapidjson::Document doc;
  if (!parse_json (output, doc) || !doc.IsObject ()) {
    wsl::log::xmake ()->error ("xmake_file_api: failed to parse target JSON "
                               "for {}",
                               target_name);
    return std::nullopt;
  }

  target_info target;
  if (doc.HasMember ("name") && doc["name"].IsString ()) {
    target.name = doc["name"].GetString ();
  }
  if (doc.HasMember ("kind") && doc["kind"].IsString ()) {
    target.type = doc["kind"].GetString ();
  }

  if (doc.HasMember ("includedirs") && doc["includedirs"].IsArray ()) {
    for (const auto &inc : doc["includedirs"].GetArray ()) {
      std::string const value = string_value (inc);
      if (!value.empty ()) {
        target.include_directories.push_back (value);
      }
    }
  }
  if (doc.HasMember ("sysincludedirs") && doc["sysincludedirs"].IsArray ()) {
    for (const auto &inc : doc["sysincludedirs"].GetArray ()) {
      std::string const value = string_value (inc);
      if (!value.empty ()) {
        target.include_directories.push_back (value);
      }
    }
  }

  if (doc.HasMember ("defines") && doc["defines"].IsArray ()) {
    for (const auto &def : doc["defines"].GetArray ()) {
      const std::string value = string_value (def);
      if (value.empty () || is_flag_fragment (value)) {
        continue;
      }
      if (value.starts_with ("NDEBUG")) {
        continue;
      }
      target.compile_definitions.push_back (value);
    }
  }

  if (doc.HasMember ("links") && doc["links"].IsArray ()) {
    for (const auto &link : doc["links"].GetArray ()) {
      std::string const value = string_value (link);
      if (!value.empty ()) {
        target.link_libraries.push_back (value);
      }
    }
  }
  if (doc.HasMember ("syslinks") && doc["syslinks"].IsArray ()) {
    for (const auto &link : doc["syslinks"].GetArray ()) {
      std::string const value = string_value (link);
      if (!value.empty ()) {
        target.link_libraries.push_back (value);
      }
    }
  }

  return target;
}

std::optional<xmake_file_api::project_info>
xmake_file_api::parse_replies (const fs::path &project_root,
                               const fs::path &build_dir)
{
  (void)build_dir;
  auto targets = list_targets (project_root);
  if (!targets) {
    return std::nullopt;
  }

  project_info info;

  // Project name from xmake show (matches what CMake codemodel exposed).
  std::string const command = "cd " + quoted (project_root.string ())
                              + " && xmake show --format=json";
  std::string const output = exec (command);
  if (!output.empty ()) {
    rapidjson::Document doc;
    if (parse_json (output, doc) && doc.IsObject () && doc.HasMember ("project")
        && doc["project"].IsObject () && doc["project"].HasMember ("name")
        && doc["project"]["name"].IsString ()) {
      info.name = doc["project"]["name"].GetString ();
    }
  }

  for (const std::string &name : *targets) {
    auto target_info = parse_target (project_root, name);
    if (target_info) {
      info.targets.push_back (std::move (*target_info));
    }
  }

  const std::vector<std::string> test_names = list_tests (project_root);
  for (const std::string &test_name : test_names) {
    info.tests.push_back ({ test_name, {}, {} });
  }

  return info;
}

} // namespace xmake

} // namespace rsc

} // namespace wsl
