#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cli/cli_handler.hpp"
#include "wsl/log/log.hpp"

#include <string>

using wsl::cli::cli_handler;

// Global log initializer (idempotent, safe to call multiple times)
namespace
{
struct log_initializer
{
  log_initializer () { wsl::log::init (); }
};
static log_initializer init_log;
}

// ===== Basic Flags and Options =====

TEST_CASE ("--help triggers exit with code 0")
{
  const char *argv[] = { "weasel-cli", "--help" };
  auto res = cli_handler ().parse (2, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code == 0);
}

TEST_CASE ("--project <path> sets project_to_load")
{
  const char *argv[] = { "weasel-cli", "--project", "/some/path" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.project_to_load.has_value ());
  CHECK (*res.project_to_load == "/some/path");
  CHECK (res.interactive == false);
  CHECK (res.attach == false);
  CHECK (res.command.has_value () == false);
}

TEST_CASE ("--project <path> --scene <path> sets both")
{
  const char *argv[]
      = { "weasel-cli", "--project", "/proj", "--scene", "/scene.wscn.json" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.project_to_load.has_value ());
  CHECK (*res.project_to_load == "/proj");
  REQUIRE (res.scene_to_load.has_value ());
  CHECK (*res.scene_to_load == "/scene.wscn.json");
}

TEST_CASE ("--project <path> --interactive works")
{
  const char *argv[]
      = { "weasel-cli", "--project", "myproject", "--interactive" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  CHECK (res.interactive == true);
  REQUIRE (res.project_to_load.has_value ());
  CHECK (*res.project_to_load == "myproject");
}

TEST_CASE ("--project <path> --attach works")
{
  const char *argv[] = { "weasel-cli", "--project", "/proj", "--attach" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  CHECK (res.attach == true);
  REQUIRE (res.project_to_load.has_value ());
  CHECK (*res.project_to_load == "/proj");
}

TEST_CASE ("-i short flag for interactive works")
{
  const char *argv[] = { "weasel-cli", "-i" };
  auto res = cli_handler ().parse (2, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  CHECK (res.interactive == true);
}

TEST_CASE ("No args returns empty defaults")
{
  const char *argv[] = { "weasel-cli" };
  auto res = cli_handler ().parse (1, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  CHECK (res.interactive == false);
  CHECK (res.attach == false);
  CHECK (res.project_to_load.has_value () == false);
  CHECK (res.scene_to_load.has_value () == false);
  CHECK (res.command.has_value () == false);
}

TEST_CASE ("--scene without --project is rejected")
{
  const char *argv[] = { "weasel-cli", "--scene", "scene.json" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code != 0);
}

TEST_CASE ("--attach without --project is rejected")
{
  const char *argv[] = { "weasel-cli", "--attach" };
  auto res = cli_handler ().parse (2, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code != 0);
}

// ===== Headless Subcommands (error cases - no engine needed) =====

TEST_CASE ("create-project without args is error")
{
  const char *argv[] = { "weasel-cli", "create-project" };
  auto res = cli_handler ().parse (2, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code == 1);
}

TEST_CASE ("--create-project alias without args is error")
{
  const char *argv[] = { "weasel-cli", "--create-project" };
  auto res = cli_handler ().parse (2, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code == 1);
}

// ===== REPL Subcommands - proj family =====

TEST_CASE ("proj new <path> <name> builds command")
{
  const char *argv[] = { "weasel-cli", "proj", "new", "/my/path", "MyProject" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj new /my/path MyProject");
}

TEST_CASE ("proj load <path> builds command")
{
  const char *argv[] = { "weasel-cli", "proj", "load", "/path/to/project" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj load /path/to/project");
}

TEST_CASE ("proj info builds command")
{
  const char *argv[] = { "weasel-cli", "proj", "info" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj info");
}

TEST_CASE ("proj save builds command")
{
  const char *argv[] = { "weasel-cli", "proj", "save" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj save");
}

// ===== REPL Subcommands - scene family =====

TEST_CASE ("scene new <name> builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "new", "MainScene" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene new MainScene");
}

TEST_CASE ("scene load <path> builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "load", "scene.wscn.json" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene load scene.wscn.json");
}

TEST_CASE ("scene save without path builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "save" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene save");
}

TEST_CASE ("scene save <path> builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "save", "custom.wscn.json" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene save custom.wscn.json");
}

TEST_CASE ("scene ls builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "ls" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene ls");
}

TEST_CASE ("scene status builds command")
{
  const char *argv[] = { "weasel-cli", "scene", "status" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene status");
}

// ===== REPL Subcommands - ent family =====

TEST_CASE ("ent new without name builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "new" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent new");
}

TEST_CASE ("ent new <name> builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "new", "PlayerShip" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent new PlayerShip");
}

TEST_CASE ("ent new --empty builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "new", "--empty" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent new --empty");
}

TEST_CASE ("ent new --empty <name> builds command")
{
  const char *argv[]
      = { "weasel-cli", "ent", "new", "--empty", "MyBareEntity" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent new --empty MyBareEntity");
}

TEST_CASE ("ent ls builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "ls" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent ls");
}

TEST_CASE ("ent rm <id> builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "rm", "42" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent rm 42");
}

TEST_CASE ("ent ren <id> <name> builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "ren", "7", "NewName" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent ren 7 NewName");
}

TEST_CASE ("ent inspect <id> builds command")
{
  const char *argv[] = { "weasel-cli", "ent", "inspect", "123" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent inspect 123");
}

// ===== REPL Subcommands - comp family =====

TEST_CASE ("comp ls without entity_id builds command")
{
  const char *argv[] = { "weasel-cli", "comp", "ls" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "comp ls");
}

TEST_CASE ("comp ls <ent_id> builds command")
{
  const char *argv[] = { "weasel-cli", "comp", "ls", "42" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "comp ls 42");
}

TEST_CASE ("comp avail builds command")
{
  const char *argv[] = { "weasel-cli", "comp", "avail" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "comp avail");
}

TEST_CASE ("comp add <id> <type> builds command")
{
  const char *argv[] = { "weasel-cli", "comp", "add", "7", "Transform" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "comp add 7 Transform");
}

// ===== REPL Subcommands - sys family =====

TEST_CASE ("sys ls builds command")
{
  const char *argv[] = { "weasel-cli", "sys", "ls" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "sys ls");
}

TEST_CASE ("sys avail builds command")
{
  const char *argv[] = { "weasel-cli", "sys", "avail" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "sys avail");
}

// ===== Error Cases =====

TEST_CASE ("--interactive with subcommand produces error")
{
  const char *argv[] = { "weasel-cli", "--interactive", "proj", "info" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code == 1);
}

TEST_CASE ("missing required proj new args produces parse error")
{
  const char *argv[] = { "weasel-cli", "proj", "new" };
  auto res = cli_handler ().parse (3, const_cast<char **> (argv));
  CHECK (res.should_exit == true);
  CHECK (res.exit_code != 0);
}

// ===== Extra args treated as raw command =====

TEST_CASE ("extra args build raw command")
{
  const char *argv[] = { "weasel-cli", "some", "extra", "command" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "some extra command");
}

// ===== Quote handling in built commands =====

TEST_CASE ("ent new name with spaces is quoted in command")
{
  const char *argv[] = { "weasel-cli", "ent", "new", "My Entity" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "ent new \"My Entity\"");
}

TEST_CASE ("scene new name with spaces is quoted in command")
{
  const char *argv[] = { "weasel-cli", "scene", "new", "Main Scene" };
  auto res = cli_handler ().parse (4, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "scene new \"Main Scene\"");
}

TEST_CASE ("proj new path with spaces is quoted in command")
{
  const char *argv[]
      = { "weasel-cli", "proj", "new", "/my/project", "My Project" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj new /my/project \"My Project\"");
}

TEST_CASE ("arg with embedded quote is escaped")
{
  const char *argv[] = { "weasel-cli", "proj", "new", "/p", "Project\"Name" };
  auto res = cli_handler ().parse (5, const_cast<char **> (argv));
  CHECK (res.should_exit == false);
  REQUIRE (res.command.has_value ());
  CHECK (*res.command == "proj new /p \"Project\\\"Name\"");
}
