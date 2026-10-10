#include "cli_passthrough.hpp"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#ifdef _WIN32
// Kept for GetModuleFileNameA in resolve_weasel_cli; process and pipe
// handling below is SDL-only on every platform.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <chrono>
#include <filesystem>
#include <sstream>
#include <vector>

namespace wsl::mcp_server
{

namespace
{

// Shell-style tokenizer: whitespace splits, quotes group, backslash escapes.
std::vector<std::string>
tokenize_command (const std::string &line)
{
  std::vector<std::string> tokens;
  std::string current;
  bool in_single = false;
  bool in_double = false;
  bool escape_next = false;

  for (char ch : line) {
    if (escape_next) {
      current.push_back (ch);
      escape_next = false;
      continue;
    }
    if (ch == '\\' && !in_single) {
      escape_next = true;
      continue;
    }
    if (ch == '\'' && !in_double) {
      in_single = !in_single;
      continue;
    }
    if (ch == '"' && !in_single) {
      in_double = !in_double;
      continue;
    }
    if (std::isspace (static_cast<unsigned char> (ch)) && !in_single
        && !in_double) {
      if (!current.empty ()) {
        tokens.push_back (current);
        current.clear ();
      }
      continue;
    }
    current.push_back (ch);
  }
  if (!current.empty ())
    tokens.push_back (current);
  return tokens;
}

// Wrap a value in double quotes when it contains whitespace so the downstream
// command tokenizer keeps it as a single argument. Empty strings are left
// empty (the caller decides whether to emit the token at all).
std::string
quote_if_needed (const std::string &value)
{
  if (value.empty ())
    return value;
  for (char c : value) {
    if (std::isspace (static_cast<unsigned char> (c)))
      return "\"" + value + "\"";
  }
  return value;
}

} // namespace

std::string
resolve_weasel_cli ()
{
  if (const char *env = std::getenv ("WEASEL_CLI_PATH")) {
    if (*env != '\0')
      return env;
  }

  std::error_code ec;
#ifdef _WIN32
  // Sibling weasel-cli.exe next to this binary.
  char self_buf[MAX_PATH];
  const DWORD self_len = GetModuleFileNameA (nullptr, self_buf, MAX_PATH);
  if (self_len != 0 && self_len < MAX_PATH) {
    const std::filesystem::path candidate
        = std::filesystem::path (std::string (self_buf, self_len))
              .parent_path ()
          / "weasel-cli.exe";
    if (std::filesystem::exists (candidate, ec))
      return candidate.string ();
  }
#else
  std::filesystem::path self
      = std::filesystem::read_symlink ("/proc/self/exe", ec);
  if (!ec) {
    const std::filesystem::path candidate = self.parent_path () / "weasel-cli";
    if (std::filesystem::exists (candidate, ec))
      return candidate.string ();
  }
#endif

  return "weasel-cli";
}

// Run weasel-cli and capture its output. Single implementation for every
// platform on top of SDL processes: stdout/stderr travel on separate pipes
// (preserved independently in the result, as with fork), stdin is inherited,
// and the deadline loop mirrors the old poll() version.
static mcp::json
run_sdl (const std::vector<std::string> &full_argv, int timeout_seconds)
{
  // argv for SDL (null-terminated array of pointers into full_argv, which
  // outlives the call; SDL copies what it needs during creation).
  std::vector<const char *> argv;
  argv.reserve (full_argv.size () + 1);
  for (const std::string &arg : full_argv) {
    argv.push_back (arg.c_str ());
  }
  argv.push_back (nullptr);

  SDL_PropertiesID props = SDL_CreateProperties ();
  SDL_SetPointerProperty (props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                          argv.data ());
  SDL_SetNumberProperty (props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
                         SDL_PROCESS_STDIO_INHERITED);
  SDL_SetNumberProperty (props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                         SDL_PROCESS_STDIO_APP);
  SDL_SetNumberProperty (props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
                         SDL_PROCESS_STDIO_APP);
  SDL_Process *proc = SDL_CreateProcessWithProperties (props);
  SDL_DestroyProperties (props);
  if (!proc) {
    throw mcp::mcp_exception (mcp::error_code::internal_error,
                              std::string ("SDL_CreateProcess failed: ")
                                  + SDL_GetError ());
  }
  SDL_PropertiesID pprops = SDL_GetProcessProperties (proc);
  SDL_IOStream *out = (SDL_IOStream *)SDL_GetPointerProperty (
      pprops, SDL_PROP_PROCESS_STDOUT_POINTER, nullptr);
  SDL_IOStream *err = (SDL_IOStream *)SDL_GetPointerProperty (
      pprops, SDL_PROP_PROCESS_STDERR_POINTER, nullptr);
  if (!out || !err) {
    SDL_DestroyProcess (proc);
    throw mcp::mcp_exception (mcp::error_code::internal_error,
                              "SDL process missing stdio streams");
  }

  const auto deadline = std::chrono::steady_clock::now ()
                        + std::chrono::seconds (timeout_seconds);
  bool timed_out = false;
  bool out_open = true, err_open = true;
  std::string stdout_data;
  std::string stderr_data;

  // Pump one stream; returns true on progress. Mirrors the old per-fd
  // handling: data appends, EOF/error closes that side, NOT_READY waits.
  auto pump = [&] (SDL_IOStream *s, std::string &into, bool &open) -> bool {
    if (!open) {
      return false;
    }
    char buffer[4096];
    const size_t n = SDL_ReadIO (s, buffer, sizeof (buffer));
    if (n > 0) {
      into.append (buffer, n);
      return true;
    }
    const SDL_IOStatus st = SDL_GetIOStatus (s);
    if (st == SDL_IO_STATUS_EOF || st == SDL_IO_STATUS_ERROR) {
      open = false;
    }
    return false;
  };

  while (out_open || err_open) {
    if (std::chrono::steady_clock::now () > deadline) {
      timed_out = true;
      SDL_KillProcess (proc, true);
      break;
    }
    bool progress = pump (out, stdout_data, out_open);
    progress = pump (err, stderr_data, err_open) || progress;
    if (!progress) {
      SDL_Delay (1);
    }
  }

  int exit_code = -1;
  SDL_WaitProcess (proc, true, &exit_code);
  // SDL reports fatal signals negated; match the old 128+signal mapping.
  if (exit_code < 0) {
    exit_code = 128 - exit_code;
  }
  SDL_CloseIO (out);
  SDL_CloseIO (err);
  SDL_DestroyProcess (proc);

  mcp::json result{
    { "command", full_argv },   { "exit_code", exit_code },
    { "stdout", stdout_data },  { "stderr", stderr_data },
    { "timed_out", timed_out },
  };
  return result;
}

mcp::json
execute_cli_command (const std::string &command, bool attach,
                     const std::string &project, int timeout_seconds)
{
  std::vector<std::string> argv = tokenize_command (command);
  if (!argv.empty () && argv[0] == "weasel-cli")
    argv.erase (argv.begin ());
  if (argv.empty ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Missing command to run");
  }
  if (timeout_seconds <= 0)
    timeout_seconds = 120;
  timeout_seconds = std::min (timeout_seconds, 600);

  std::vector<std::string> full_argv;
  full_argv.push_back (resolve_weasel_cli ());
  if (attach)
    full_argv.push_back ("-a");
  if (!project.empty ()) {
    full_argv.push_back ("--project");
    full_argv.push_back (project);
  }
  for (const std::string &arg : argv)
    full_argv.push_back (arg);

  return run_sdl (full_argv, timeout_seconds);
}

mcp::json
handle_script_reload (const mcp::json & /*params*/)
{
  mcp::json result = execute_cli_command ("script reload", true, "", 300);

  // Surface a compact success flag so agents do not have to parse text.
  result["success"] = result["exit_code"].get<int> () == 0
                      && !result["timed_out"].get<bool> ();
  return result;
}

mcp::json
handle_run_cli_command (const mcp::json &params)
{
  if (!params.contains ("command") || !params["command"].is_string ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Missing 'command' string parameter");
  }
  if (!params.contains ("confirm") || !params["confirm"].is_boolean ()
      || !params["confirm"].get<bool> ()) {
    throw mcp::mcp_exception (
        mcp::error_code::invalid_params,
        "This tool mutates engine state: pass confirm=true to proceed");
  }

  const bool attach
      = !params.contains ("attach") || !params["attach"].is_boolean ()
            ? true
            : params["attach"].get<bool> ();
  const std::string project
      = params.contains ("project") && params["project"].is_string ()
            ? params["project"].get<std::string> ()
            : std::string ();
  int timeout = 120;
  if (params.contains ("timeout_seconds")
      && params["timeout_seconds"].is_number_integer ())
    timeout = params["timeout_seconds"].get<int> ();

  return execute_cli_command (params["command"].get<std::string> (), attach,
                              project, timeout);
}

namespace
{

// Shared gate for the structured mutation tools: require confirm=true.
void
require_confirm (const mcp::json &params)
{
  if (!params.contains ("confirm") || !params["confirm"].is_boolean ()
      || !params["confirm"].get<bool> ()) {
    throw mcp::mcp_exception (
        mcp::error_code::invalid_params,
        "This tool mutates engine state: pass confirm=true to proceed");
  }
}

// Resolve the common attach/project/timeout options used by every tool.
bool
resolve_attach (const mcp::json &params)
{
  return !params.contains ("attach") || !params["attach"].is_boolean ()
             ? true
             : params["attach"].get<bool> ();
}

std::string
resolve_project (const mcp::json &params)
{
  return params.contains ("project") && params["project"].is_string ()
             ? params["project"].get<std::string> ()
             : std::string ();
}

int
resolve_timeout (const mcp::json &params)
{
  int timeout = 120;
  if (params.contains ("timeout_seconds")
      && params["timeout_seconds"].is_number_integer ())
    timeout = params["timeout_seconds"].get<int> ();
  return timeout;
}

} // namespace

mcp::json
handle_scene_save (const mcp::json &params)
{
  require_confirm (params);
  if (params.contains ("path") && !params["path"].is_string ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "'path' must be a string");
  }
  if (params.contains ("as_default") && !params["as_default"].is_boolean ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "'as_default' must be a boolean");
  }

  std::string command = "scene save";
  if (params.contains ("path")) {
    const std::string path = params["path"].get<std::string> ();
    if (path.empty ())
      throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                "'path' must not be empty");
    command += " " + quote_if_needed (path);
  }
  if (params.contains ("as_default") && params["as_default"].get<bool> ())
    command += " --as-default";

  mcp::json result = execute_cli_command (command, resolve_attach (params),
                                          resolve_project (params),
                                          resolve_timeout (params));
  result["success"] = result["exit_code"].get<int> () == 0
                      && !result["timed_out"].get<bool> ();
  return result;
}

mcp::json
handle_entity_add (const mcp::json &params)
{
  require_confirm (params);
  if (!params.contains ("name") || !params["name"].is_string ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Missing 'name' string parameter");
  }
  const std::string name = params["name"].get<std::string> ();
  if (name.empty ())
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "'name' must not be empty");
  if (params.contains ("empty") && !params["empty"].is_boolean ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "'empty' must be a boolean");
  }

  std::string command = "ent new";
  if (params.contains ("empty") && params["empty"].get<bool> ())
    command += " --empty";
  command += " " + quote_if_needed (name);

  mcp::json result = execute_cli_command (command, resolve_attach (params),
                                          resolve_project (params),
                                          resolve_timeout (params));
  result["success"] = result["exit_code"].get<int> () == 0
                      && !result["timed_out"].get<bool> ();
  return result;
}

mcp::json
handle_entity_remove (const mcp::json &params)
{
  require_confirm (params);
  if (!params.contains ("id") || !params["id"].is_string ()) {
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "Missing 'id' string parameter (entity id or "
                              "name)");
  }
  const std::string id = params["id"].get<std::string> ();
  if (id.empty ())
    throw mcp::mcp_exception (mcp::error_code::invalid_params,
                              "'id' must not be empty");

  const std::string command = "ent rm " + quote_if_needed (id);

  mcp::json result = execute_cli_command (command, resolve_attach (params),
                                          resolve_project (params),
                                          resolve_timeout (params));
  result["success"] = result["exit_code"].get<int> () == 0
                      && !result["timed_out"].get<bool> ();
  return result;
}

mcp::json
handle_component_set (const mcp::json &params)
{
  require_confirm (params);
  for (const char *field : { "entity", "component", "property", "value" }) {
    if (!params.contains (field) || !params[field].is_string ()) {
      throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                std::string ("Missing '") + field
                                    + "' string parameter");
    }
    if (params[field].get<std::string> ().empty ()) {
      throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                std::string ("'") + field
                                    + "' must not be empty");
    }
  }

  const std::string command
      = "comp set " + quote_if_needed (params["entity"].get<std::string> ())
        + " " + quote_if_needed (params["component"].get<std::string> ()) + " "
        + quote_if_needed (params["property"].get<std::string> ()) + " "
        + quote_if_needed (params["value"].get<std::string> ());

  mcp::json result = execute_cli_command (command, resolve_attach (params),
                                          resolve_project (params),
                                          resolve_timeout (params));
  result["success"] = result["exit_code"].get<int> () == 0
                      && !result["timed_out"].get<bool> ();
  return result;
}

} // namespace wsl::mcp_server
