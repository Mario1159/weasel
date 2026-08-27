#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "mcp-server/cli_reference.hpp"
#include "mcp-server/mcp_server_handlers.hpp"
#include "mcp-server/namespace_info.hpp"

#include <mcp_message.h>

#include <string>

#include <fstream>
#include <filesystem>
#include <regex>
#include <set>

using namespace wsl::mcp_server;

// Helper: extract text from the MCP content array
static std::string
get_text (const mcp::json &content)
{
  REQUIRE (content.is_array ());
  REQUIRE (content.size () == 1);
  REQUIRE (content[0].contains ("text"));
  return content[0]["text"].get<std::string> ();
}

// ============================================================================
// cli_reference
// ============================================================================

TEST_CASE ("get_cli_reference returns all commands")
{
  const auto &ref = get_cli_reference ();
  CHECK (ref.size () > 30);

  bool has_proj_new = false;
  bool has_ent_new = false;
  bool has_help = false;
  for (const auto &cmd : ref) {
    if (cmd.name == "proj new")
      has_proj_new = true;
    if (cmd.name == "ent new")
      has_ent_new = true;
    if (cmd.name == "help")
      has_help = true;
  }
  CHECK (has_proj_new);
  CHECK (has_ent_new);
  CHECK (has_help);
}

TEST_CASE ("commands have required fields")
{
  const auto &ref = get_cli_reference ();
  for (const auto &cmd : ref) {
    CHECK (!cmd.name.empty ());
    CHECK (!cmd.category.empty ());
    CHECK ((cmd.category == "cli" || cmd.category == "repl"));
    CHECK (!cmd.description.empty ());
    CHECK (!cmd.syntax.empty ());
  }
}

TEST_CASE ("find_command finds existing commands")
{
  const cli_command_info *cmd = find_command ("proj new");
  REQUIRE (cmd != nullptr);
  CHECK (cmd->name == "proj new");
  CHECK (cmd->category == "repl");

  cmd = find_command ("ent new");
  REQUIRE (cmd != nullptr);
  CHECK (cmd->name == "ent new");
}

TEST_CASE ("find_command returns null for unknown")
{
  CHECK (find_command ("nonexistent") == nullptr);
  CHECK (find_command ("") == nullptr);
}

TEST_CASE ("find_command finds cli flags")
{
  const cli_command_info *cmd = find_command ("--project");
  REQUIRE (cmd != nullptr);
  CHECK (cmd->category == "cli");
  CHECK (cmd->syntax.find ("--project") != std::string::npos);
}

// ============================================================================
// namespace_info
// ============================================================================

TEST_CASE ("handle_list_namespaces lists all 7 namespaces")
{
  mcp::json params = mcp::json::object ();
  mcp::json result = handle_list_namespaces (params);
  std::string text = get_text (result);

  CHECK (text.find ("comp") != std::string::npos);
  CHECK (text.find ("sys") != std::string::npos);
  CHECK (text.find ("rsc") != std::string::npos);
  CHECK (text.find ("phys") != std::string::npos);
  CHECK (text.find ("gfx") != std::string::npos);
  CHECK (text.find ("math") != std::string::npos);
  CHECK (text.find ("reg") != std::string::npos);
}

TEST_CASE ("handle_describe_namespace returns detail for known ns")
{
  mcp::json params = { { "name", "comp" } };
  mcp::json result = handle_describe_namespace (params);
  std::string text = get_text (result);

  CHECK (text.find ("wsl::comp") != std::string::npos);
  CHECK (text.find ("transform") != std::string::npos);
  CHECK (text.find ("camera") != std::string::npos);
}

TEST_CASE ("handle_describe_namespace throws for unknown ns")
{
  mcp::json params = { { "name", "foobar" } };
  CHECK_THROWS_AS (handle_describe_namespace (params), mcp::mcp_exception);
}

TEST_CASE ("handle_describe_namespace throws when name missing")
{
  mcp::json params = mcp::json::object ();
  CHECK_THROWS_AS (handle_describe_namespace (params), mcp::mcp_exception);
}

TEST_CASE ("handle_describe_namespace is case-insensitive")
{
  mcp::json params = { { "name", "COMP" } };
  mcp::json result = handle_describe_namespace (params);
  CHECK (get_text (result).find ("wsl::comp") != std::string::npos);
}

TEST_CASE ("handle_describe_namespace for sys")
{
  mcp::json params = { { "name", "sys" } };
  mcp::json result = handle_describe_namespace (params);
  CHECK (get_text (result).find ("EcsSystem") != std::string::npos);
}

// ============================================================================
// mcp_server_handlers
// ============================================================================

TEST_CASE ("handle_list_commands returns categorized list")
{
  mcp::json params = mcp::json::object ();
  mcp::json result = handle_list_commands (params);
  std::string text = get_text (result);

  CHECK (text.find ("CLI Subcommands") != std::string::npos);
  CHECK (text.find ("REPL Commands") != std::string::npos);
  CHECK (text.find ("--project") != std::string::npos);
  CHECK (text.find ("proj new") != std::string::npos);
  CHECK (text.find ("comp avail") != std::string::npos);
}

TEST_CASE ("handle_describe_command returns command details")
{
  mcp::json params = { { "name", "proj new" } };
  mcp::json result = handle_describe_command (params);
  std::string text = get_text (result);

  CHECK (text.find ("Command: proj new") != std::string::npos);
  CHECK (text.find ("Category: repl") != std::string::npos);
  CHECK (text.find ("Syntax:") != std::string::npos);
}

TEST_CASE ("handle_describe_command throws for unknown command")
{
  mcp::json params = { { "name", "does_not_exist" } };
  CHECK_THROWS_AS (handle_describe_command (params), mcp::mcp_exception);
}

TEST_CASE ("handle_describe_command throws when name missing")
{
  mcp::json params = mcp::json::object ();
  CHECK_THROWS_AS (handle_describe_command (params), mcp::mcp_exception);
}

TEST_CASE ("handle_describe_command shows parameters and examples")
{
  mcp::json params = { { "name", "proj new" } };
  mcp::json result = handle_describe_command (params);
  std::string text = get_text (result);

  CHECK (text.find ("Parameters:") != std::string::npos);
  CHECK (text.find ("Examples:") != std::string::npos);
}

TEST_CASE ("handle_get_quick_start returns guide")
{
  mcp::json params = mcp::json::object ();
  mcp::json result = handle_get_quick_start (params);
  std::string text = get_text (result);

  CHECK (text.find ("Quick Start") != std::string::npos);
  CHECK (text.find ("create-project") != std::string::npos);
  CHECK (text.find ("REPL") != std::string::npos);
}

TEST_CASE ("handle_cli_capabilities returns manifest")
{
  mcp::json params = mcp::json::object ();
  mcp::json result = handle_cli_capabilities (params);
  std::string text = get_text (result);

  CHECK (text.find ("Fully Implemented") != std::string::npos);
  CHECK (text.find ("NOT Implemented") != std::string::npos);
  CHECK (text.find ("proj new") != std::string::npos);
  CHECK (text.find ("sig") != std::string::npos);
}

// ============================================================================
// doc-drift guard: the generated Daslang API ledger
// (src/wsl/das/das_api_catalog.gen.hpp) must stay in sync with the real
// registration site (addExtern calls in src/wsl/das/wsl_api_module.cpp).
// ============================================================================

#ifndef WSL_SOURCE_DIR
#define WSL_SOURCE_DIR "."
#endif

TEST_CASE ("das_api_catalog stays in sync with wsl_api_module registrations")
{
  const std::filesystem::path source = std::filesystem::path (WSL_SOURCE_DIR)
                                       / "src/wsl/das/wsl_api_module.cpp";
  const std::filesystem::path catalog = std::filesystem::path (WSL_SOURCE_DIR)
                                        / "src/wsl/das/das_api_catalog.gen.hpp";

  std::ifstream src (source);
  REQUIRE_MESSAGE (src.good (), "could not open wsl_api_module.cpp");
  std::ifstream cat (catalog);
  REQUIRE_MESSAGE (cat.good (), "could not open das_api_catalog.gen.hpp");

  std::string src_text ((std::istreambuf_iterator<char> (src)),
                        std::istreambuf_iterator<char> ());
  std::string cat_text ((std::istreambuf_iterator<char> (cat)),
                        std::istreambuf_iterator<char> ());

  // Mirror the generator's regex for the C++ registration name.
  const std::regex extern_re (
      R"PAT(addExtern\s*<\s*DAS_BIND_FUN\s*\(\s*([A-Za-z_][\w]*)\s*\)\s*>\s*\()PAT");
  std::set<std::string> registered;
  for (auto it
       = std::sregex_iterator (src_text.begin (), src_text.end (), extern_re);
       it != std::sregex_iterator (); ++it) {
    registered.insert ((*it)[1].str ());
  }
  REQUIRE_MESSAGE (!registered.empty (),
                   "no addExtern registrations found in source");

  // Catalog entries store the C++ name as the second array element.
  const std::regex cat_re (R"PAT(\{\s*"([^"]+)",\s*"([^"]+)",)PAT");
  std::set<std::string> cataloged;
  for (auto it
       = std::sregex_iterator (cat_text.begin (), cat_text.end (), cat_re);
       it != std::sregex_iterator (); ++it) {
    cataloged.insert ((*it)[2].str ());
  }
  REQUIRE_MESSAGE (!cataloged.empty (), "no entries found in catalog");

  for (const auto &name : registered) {
    INFO ("registered function '" << name
                                  << "' missing from generated catalog");
    CHECK (cataloged.count (name) == 1);
  }
}
