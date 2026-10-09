#include "cli_passthrough.hpp"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#else
// See editor_context.cpp: WIN32_LEAN_AND_MEAN + NOMINMAX, near/far undefined.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif

#include <chrono>
#include <filesystem>
#include <sstream>
#include <vector>

namespace wsl::mcp_server
{

namespace
{

#ifndef _WIN32
// Single non-blocking-friendly read; empty string signals EOF/EAGAIN end.
std::string
read_chunk (int fd)
{
  char buffer[4096];
  const ssize_t n = ::read (fd, buffer, sizeof (buffer));
  if (n <= 0)
    return {};
  return std::string (buffer, static_cast<std::size_t> (n));
}
#endif

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
// command tokenizer (and Windows CreateProcess parsing) keeps it as a single
// argument. Empty strings are left empty (the caller decides whether to emit
// the token at all).
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

#ifdef _WIN32
// File-local runners (defined below): execute_cli_command builds the argv,
// then dispatches to the platform implementation.
static mcp::json run_windows (std::vector<std::string> full_argv,
                              int timeout_seconds);

// Drain whatever the child has already written without blocking.
static void
drain_pipe_available (HANDLE pipe, std::string &out)
{
  DWORD available = 0;
  while (PeekNamedPipe (pipe, nullptr, 0, nullptr, &available, nullptr)
         && available != 0) {
    char buffer[4096];
    DWORD n = 0;
    if (!ReadFile (pipe, buffer, sizeof (buffer), &n, nullptr) || n == 0) {
      break;
    }
    out.append (buffer, n);
  }
}

// Read until EOF (child exited and closed its ends).
static void
drain_pipe_eof (HANDLE pipe, std::string &out)
{
  char buffer[4096];
  for (;;) {
    DWORD n = 0;
    if (!ReadFile (pipe, buffer, sizeof (buffer), &n, nullptr) || n == 0) {
      break;
    }
    out.append (buffer, n);
  }
}

static mcp::json
run_windows (std::vector<std::string> full_argv, int timeout_seconds)
{
  // CreateProcess takes a single mutable command line.
  std::string cmdline;
  for (const std::string &arg : full_argv) {
    if (!cmdline.empty ()) {
      cmdline += ' ';
    }
    cmdline += quote_if_needed (arg);
  }

  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof (sa);
  sa.bInheritHandle = TRUE;
  HANDLE out_read = nullptr, out_write = nullptr;
  HANDLE err_read = nullptr, err_write = nullptr;
  if (!CreatePipe (&out_read, &out_write, &sa, 0)
      || !CreatePipe (&err_read, &err_write, &sa, 0)) {
    throw mcp::mcp_exception (mcp::error_code::internal_error,
                              "CreatePipe() failed");
  }
  SetHandleInformation (out_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation (err_read, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si{};
  si.cb = sizeof (si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = out_write;
  si.hStdError = err_write;
  si.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA (nullptr, cmdline.data (), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle (out_read);
    CloseHandle (out_write);
    CloseHandle (err_read);
    CloseHandle (err_write);
    throw mcp::mcp_exception (mcp::error_code::internal_error,
                              "CreateProcess() failed");
  }
  CloseHandle (out_write);
  CloseHandle (err_write);

  const auto deadline = std::chrono::steady_clock::now ()
                        + std::chrono::seconds (timeout_seconds);
  bool timed_out = false;
  std::string stdout_data;
  std::string stderr_data;
  bool exited = false;
  while (!exited) {
    if (std::chrono::steady_clock::now () > deadline) {
      timed_out = true;
      TerminateProcess (pi.hProcess, 1);
      break;
    }
    const DWORD w = WaitForSingleObject (pi.hProcess, 200);
    drain_pipe_available (out_read, stdout_data);
    drain_pipe_available (err_read, stderr_data);
    if (w == WAIT_OBJECT_0) {
      exited = true;
    }
  }
  if (exited) {
    // Process is gone and our write ends are closed, so reads hit EOF.
    WaitForSingleObject (pi.hProcess, INFINITE);
    drain_pipe_eof (out_read, stdout_data);
    drain_pipe_eof (err_read, stderr_data);
  } else {
    // Timed out and killed: collect whatever was already written.
    drain_pipe_available (out_read, stdout_data);
    drain_pipe_available (err_read, stderr_data);
  }

  DWORD exit_code = 1;
  GetExitCodeProcess (pi.hProcess, &exit_code);
  CloseHandle (pi.hProcess);
  CloseHandle (pi.hThread);
  CloseHandle (out_read);
  CloseHandle (err_read);

  mcp::json result{
    { "command", full_argv },   { "exit_code", static_cast<int> (exit_code) },
    { "stdout", stdout_data },  { "stderr", stderr_data },
    { "timed_out", timed_out },
  };
  return result;
}
#else
static mcp::json run_posix (std::vector<std::string> full_argv,
                            int timeout_seconds);
#endif

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

#ifdef _WIN32
  return run_windows (full_argv, timeout_seconds);
#else
  return run_posix (full_argv, timeout_seconds);
#endif
}

#ifndef _WIN32
static mcp::json
run_posix (std::vector<std::string> full_argv, int timeout_seconds)
{
  // Build the C argv (first entry is the program path).
  std::vector<char *> c_argv;
  c_argv.reserve (full_argv.size () + 1);
  for (std::string &arg : full_argv)
    c_argv.push_back (arg.data ());
  c_argv.push_back (nullptr);

  int stdout_pipe[2];
  int stderr_pipe[2];
  if (::pipe (stdout_pipe) != 0 || ::pipe (stderr_pipe) != 0) {
    throw mcp::mcp_exception (mcp::error_code::internal_error, "pipe() failed");
  }

  const pid_t pid = ::fork ();
  if (pid < 0) {
    ::close (stdout_pipe[0]);
    ::close (stdout_pipe[1]);
    ::close (stderr_pipe[0]);
    ::close (stderr_pipe[1]);
    throw mcp::mcp_exception (mcp::error_code::internal_error, "fork() failed");
  }

  if (pid == 0) {
    ::dup2 (stdout_pipe[1], STDOUT_FILENO);
    ::dup2 (stderr_pipe[1], STDERR_FILENO);
    ::close (stdout_pipe[0]);
    ::close (stdout_pipe[1]);
    ::close (stderr_pipe[0]);
    ::close (stderr_pipe[1]);
    ::signal (SIGINT, SIG_DFL);
    ::execv (full_argv[0].c_str (), c_argv.data ());
    _exit (127);
  }

  ::close (stdout_pipe[1]);
  ::close (stderr_pipe[1]);

  // Poll both pipes until EOF on both or the deadline expires.
  const auto deadline = std::chrono::steady_clock::now ()
                        + std::chrono::seconds (timeout_seconds);
  bool timed_out = false;
  int open_fds = 2;
  std::string stdout_data;
  std::string stderr_data;

  while (open_fds > 0) {
    if (std::chrono::steady_clock::now () > deadline) {
      timed_out = true;
      ::kill (pid, SIGKILL);
      break;
    }

    struct pollfd fds[2]
        = { { stdout_pipe[0], POLLIN, 0 }, { stderr_pipe[0], POLLIN, 0 } };
    const int ready = ::poll (fds, 2, 200);
    if (ready > 0) {
      if ((fds[0].revents & (POLLIN | POLLHUP)) != 0) {
        std::string chunk = read_chunk (stdout_pipe[0]);
        if (chunk.empty ())
          --open_fds;
        else
          stdout_data += chunk;
      }
      if ((fds[1].revents & (POLLIN | POLLHUP)) != 0) {
        std::string chunk = read_chunk (stderr_pipe[0]);
        if (chunk.empty ())
          --open_fds;
        else
          stderr_data += chunk;
      }
    }
  }

  ::close (stdout_pipe[0]);
  ::close (stderr_pipe[0]);

  int status = 0;
  ::waitpid (pid, &status, 0);

  int exit_code = -1;
  if (WIFEXITED (status))
    exit_code = WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    exit_code = 128 + WTERMSIG (status);

  mcp::json result{
    { "command", full_argv },   { "exit_code", exit_code },
    { "stdout", stdout_data },  { "stderr", stderr_data },
    { "timed_out", timed_out },
  };
  return result;
}
#endif

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
