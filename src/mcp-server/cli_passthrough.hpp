#pragma once

#include <mcp_server.h>
#include <string>

namespace wsl::mcp_server
{
/**
 * Resolves the weasel-cli executable used for command passthrough.
 *
 * Order: WEASEL_CLI_PATH environment variable, a "weasel-cli" binary next to
 * this server's own executable, then bare "weasel-cli" (PATH lookup).
 */
std::string resolve_weasel_cli ();

/**
 * Runs a weasel-cli command in a child process and returns its outcome.
 *
 * The command string is tokenized with shell-style quoting rules but is NOT
 * passed through a shell, so no shell metacharacters are interpreted.
 *
 * :param command: Full weasel-cli command line, e.g. ``scene ls`` or
 *   ``sys add "Ball Controller System"``. The leading ``weasel-cli`` word is
 *   optional and stripped when present.
 * :param attach: When true (default), run in attach mode (``-a``) so the
 *   first running editor instance executes the command against its open
 *   project; standalone execution is used as fallback by the CLI itself.
 * :param project: Optional project path forwarded as ``--project``.
 * :param timeout_seconds: Kill the child after this many seconds.
 * :return: JSON object with ``command`` (full argv), ``exit_code``,
 *   ``stdout``, ``stderr``, ``timed_out``, and ``cli_path``.
 */
mcp::json execute_cli_command (const std::string &command, bool attach,
                               const std::string &project, int timeout_seconds);

/** MCP handler for the ``script_reload`` tool (no confirmation needed). */
mcp::json handle_script_reload (const mcp::json &params);

/**
 * MCP handler for the ``run_cli_command`` passthrough tool.
 *
 * Requires ``confirm == true`` so callers state intent before mutating.
 */
mcp::json handle_run_cli_command (const mcp::json &params);

/**
 * Structured mutation tools (Phase 4 / E3 tier 2). Each maps to a single
 * weasel-cli command with validated, typed inputs instead of a free-form
 * command line, and each requires ``confirm == true``.
 */
mcp::json handle_scene_save (const mcp::json &params);
mcp::json handle_entity_add (const mcp::json &params);
mcp::json handle_entity_remove (const mcp::json &params);
mcp::json handle_component_set (const mcp::json &params);

} // namespace wsl::mcp_server
