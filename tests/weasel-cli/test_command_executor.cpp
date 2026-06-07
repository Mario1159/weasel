#include <doctest/doctest.h>

#include "cli/command_executor.hpp"
#include "cli/cli_handler.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/log/log.hpp"

#include <string>
#include <vector>

using wsl::cli::cli_handler;
using wsl::cli::command_executor;

namespace
{

struct exec_fixture
{
  wsl::comp::singl::runtime_context rtc;
  command_executor exec;

  exec_fixture ()
      : rtc ("Weasel CLI Test", 0, 0,
             cli_handler::default_engine_resource_path (), true),
        exec (rtc)
  {
    static bool log_inited = false;
    if (!log_inited) {
      wsl::log::init ();
      log_inited = true;
    }
    rtc.set_editor_ctx (nullptr);
  }

  std::string
  execute (const std::string &cmd)
  {
    return exec.execute (cmd);
  }
};

} // namespace

// ===== tokenize =====

TEST_CASE ("tokenize empty string returns empty vector")
{
  auto tokens = command_executor::tokenize ("");
  CHECK (tokens.empty ());
}

TEST_CASE ("tokenize multiple tokens")
{
  auto tokens = command_executor::tokenize ("proj info");
  REQUIRE (tokens.size () == 2);
  CHECK (tokens[0] == "proj");
  CHECK (tokens[1] == "info");
}

TEST_CASE ("tokenize double-quoted string preserves spaces")
{
  auto tokens = command_executor::tokenize ("ent new \"My Entity\"");
  REQUIRE (tokens.size () == 3);
  CHECK (tokens[0] == "ent");
  CHECK (tokens[1] == "new");
  CHECK (tokens[2] == "My Entity");
}

TEST_CASE ("tokenize single-quoted string preserves spaces")
{
  auto tokens = command_executor::tokenize ("ent new 'My Entity'");
  REQUIRE (tokens.size () == 3);
  CHECK (tokens[0] == "ent");
  CHECK (tokens[1] == "new");
  CHECK (tokens[2] == "My Entity");
}

// ===== help command =====

TEST_CASE ("help lists all command families")
{
  exec_fixture fx;
  auto out = fx.execute ("help");
  CHECK (out.find ("Available Commands") != std::string::npos);
  CHECK (out.find ("proj") != std::string::npos);
  CHECK (out.find ("scene") != std::string::npos);
  CHECK (out.find ("ent") != std::string::npos);
  CHECK (out.find ("comp") != std::string::npos);
  CHECK (out.find ("sys") != std::string::npos);
  CHECK (out.find ("exit") != std::string::npos);
}

// ===== exit / quit =====

TEST_CASE ("exit command returns exit marker")
{
  exec_fixture fx;
  auto out = fx.execute ("exit");
  CHECK (out.find ("exit") != std::string::npos);
}

TEST_CASE ("quit command returns exit marker")
{
  exec_fixture fx;
  auto out = fx.execute ("quit");
  CHECK (out.find ("exit") != std::string::npos);
}

// ===== unknown command =====

TEST_CASE ("unknown command family prints error")
{
  exec_fixture fx;
  auto out = fx.execute ("foobar x y");
  CHECK (out.find ("Unknown command family") != std::string::npos);
  CHECK (out.find ("foobar") != std::string::npos);
}

// ===== proj commands =====

TEST_CASE ("proj info with no project loaded")
{
  exec_fixture fx;
  auto out = fx.execute ("proj info");
  CHECK (out.find ("No project loaded") != std::string::npos);
}

TEST_CASE ("proj with no subcommand prints usage")
{
  exec_fixture fx;
  auto out = fx.execute ("proj");
  CHECK (out.find ("Usage: proj") != std::string::npos);
}

TEST_CASE ("proj unknown action produces no output")
{
  exec_fixture fx;
  auto out = fx.execute ("proj unknown");
  CHECK (out.empty ());
}

// ===== scene commands =====

TEST_CASE ("scene new creates and activates a scene")
{
  exec_fixture fx;
  auto out = fx.execute ("scene new MyScene");
  CHECK (out.find ("Scene 'MyScene' created and set as active")
         != std::string::npos);
}

TEST_CASE ("scene with no subcommand prints usage")
{
  exec_fixture fx;
  auto out = fx.execute ("scene");
  CHECK (out.find ("Usage: scene") != std::string::npos);
}

TEST_CASE ("scene new without name prints usage")
{
  exec_fixture fx;
  auto out = fx.execute ("scene new");
  CHECK (out.find ("Usage: scene") != std::string::npos);
}

TEST_CASE ("scene status with no active scene")
{
  exec_fixture fx;
  auto out = fx.execute ("scene status");
  CHECK (out.find ("No active scene") != std::string::npos);
}

TEST_CASE ("scene status after scene new shows scene info")
{
  exec_fixture fx;
  fx.execute ("scene new MyScene");
  auto out = fx.execute ("scene status");
  CHECK (out.find ("Active Scene: MyScene") != std::string::npos);
  CHECK (out.find ("Entities: 2") != std::string::npos);
}

// ===== ent commands =====

TEST_CASE ("ent with no active scene")
{
  exec_fixture fx;
  auto out = fx.execute ("ent ls");
  CHECK (out.find ("No active scene") != std::string::npos);
}

TEST_CASE ("ent new without name creates entity with defaults")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent new");
  CHECK (out.find ("Entity 2 created with Transform") != std::string::npos);
}

TEST_CASE ("ent new with name creates named entity")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent new Player");
  CHECK (out.find ("Entity 2 (Player) created with Transform")
         != std::string::npos);
}

TEST_CASE ("ent new --empty creates empty entity")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent new --empty");
  CHECK (out.find ("Entity 2 created (empty)") != std::string::npos);
}

TEST_CASE ("ent new --empty with name creates empty named entity")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent new --empty MyEntity");
  CHECK (out.find ("Entity 2 (MyEntity) created (empty)") != std::string::npos);
}

TEST_CASE ("ent ls lists created entities")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  fx.execute ("ent new");
  auto out = fx.execute ("ent ls");
  CHECK (out.find ("ID: 2") != std::string::npos);
  CHECK (out.find ("ID: 3") != std::string::npos);
}

TEST_CASE ("ent rm destroys entity")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  auto out = fx.execute ("ent rm 0");
  CHECK (out.find ("Entity 0 destroyed") != std::string::npos);
}

TEST_CASE ("ent ren renames entity")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  auto out = fx.execute ("ent ren 0 RenamedEntity");
  CHECK (out.find ("Entity 0 renamed to 'RenamedEntity'") != std::string::npos);
}

TEST_CASE ("ent inspect shows entity details")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  auto out = fx.execute ("ent inspect 0");
  CHECK (out.find ("Inspecting Entity 0") != std::string::npos);
  CHECK (out.find ("Components:") != std::string::npos);
}

// ===== comp commands =====

TEST_CASE ("comp avail lists registered components")
{
  exec_fixture fx;
  auto out = fx.execute ("comp avail");
  CHECK (out.find ("Registered Components") != std::string::npos);
}

TEST_CASE ("comp ls without args lists registered components")
{
  exec_fixture fx;
  auto out = fx.execute ("comp ls");
  CHECK (out.find ("Registered Components") != std::string::npos);
}

TEST_CASE ("comp ls with entity id needs active scene")
{
  exec_fixture fx;
  auto out = fx.execute ("comp ls 0");
  CHECK (out.find ("No active scene") != std::string::npos);
}

TEST_CASE ("comp add with unknown component type")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  auto out = fx.execute ("comp add 0 NonExistentType");
  CHECK (out.find ("Unknown component type") != std::string::npos);
}

// ===== sys commands =====

TEST_CASE ("sys ls lists per-scene systems")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  // CLI runs in headless mode — core systems may be listed; "No active scene"
  // should not appear
  auto out = fx.execute ("sys ls");
  CHECK (out.find ("Scene systems") != std::string::npos);
  CHECK (out.find ("No active scene") == std::string::npos);
}

TEST_CASE ("sys avail lists registered systems")
{
  exec_fixture fx;
  auto out = fx.execute ("sys avail");
  // CLI runs in headless mode — no built-in engine systems are registered
  CHECK (out.find ("No user-defined system types registered")
         != std::string::npos);
}

TEST_CASE ("sys with unknown subcommand prints usage")
{
  exec_fixture fx;
  auto out = fx.execute ("sys unknown");
  CHECK (out.find ("Usage: sys") != std::string::npos);
}

// ===== cls =====

TEST_CASE ("cls command returns clear screen code")
{
  exec_fixture fx;
  auto out = fx.execute ("cls");
  CHECK (out.find ("\033[2J") != std::string::npos);
}

// ===== sig command =====

TEST_CASE ("sig command prints usage or not-implemented message")
{
  exec_fixture fx;
  auto out = fx.execute ("sig");
  bool ok = (out.find ("not yet implemented") != std::string::npos)
            || (out.find ("Usage: sig") != std::string::npos);
  CHECK (ok);
}

// ===== check command =====

TEST_CASE ("check command shows validation placeholder")
{
  exec_fixture fx;
  auto out = fx.execute ("check 42");
  CHECK (out.find ("Validation placeholder for 42") != std::string::npos);
}

// ===== ent with missing args =====

TEST_CASE ("ent with no subcommand prints usage")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent");
  CHECK (out.find ("Usage: ent") != std::string::npos);
}

TEST_CASE ("ent rm with missing id does nothing")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent rm");
  CHECK (out.empty ());
}

TEST_CASE ("ent ren with missing args does nothing")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent ren");
  CHECK (out.empty ());
}

TEST_CASE ("ent inspect with missing id does nothing")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  auto out = fx.execute ("ent inspect");
  CHECK (out.empty ());
}

// ===== comp with missing args =====

TEST_CASE ("comp with no subcommand prints usage")
{
  exec_fixture fx;
  auto out = fx.execute ("comp");
  CHECK (out.find ("Usage: comp") != std::string::npos);
}

TEST_CASE ("comp add with missing args prints usage")
{
  exec_fixture fx;
  fx.execute ("scene new TestScene");
  fx.execute ("ent new");
  auto out = fx.execute ("comp add");
  CHECK (out.find ("Usage: comp") != std::string::npos);
}
