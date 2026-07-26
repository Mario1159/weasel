#pragma once

#include <string>
#include <optional>

namespace wsl::cli
{

/**
 * Handles command-line arguments for the Weasel Engine.
 *
 * This class encapsulates CLI11 parsing for both headless utility commands
 * and non-interactive equivalents of the REPL command families.
 */
class cli_handler
{
public:
  struct result
  {
    bool should_exit = false;
    int exit_code = 0;
    std::optional<std::string> project_to_load;
    std::optional<std::string> scene_to_load;
    bool interactive = false;
    std::optional<std::string> command;
    bool attach = false;
    std::optional<std::string> aot_file;   // --aot mode: .das file to compile
    std::optional<std::string> aot_output; // --aot mode: output .cpp path
  };

  /**
   * Parses the command-line arguments and executes subcommands if necessary.
   *
   * :param argc: Argument count.
   * :param argv: Argument vector.
   * :return: result Information about whether the application should exit or
   * continue.
   */
  result parse (int argc, char **argv);

  static std::string default_engine_resource_path ();

private:
};

} // namespace wsl::cli
