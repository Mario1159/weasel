#pragma once

#include <string>
#include <optional>

namespace wsl::cli {

/**
 * @brief Handles command-line arguments for the Weasel Engine.
 * 
 * This class encapsulates all CLI parsing logic using CLI11 and executes
 * headless subcommands (like project/scene creation or validation).
 */
class cli_handler {
public:
    struct result {
        bool should_exit = false;
        int exit_code = 0;
        std::optional<std::string> project_to_load;
        bool interactive = false;
        std::optional<std::string> command;
        bool attach = false;  // Connect to running editor server
    };

    /**
     * @brief Parses the command-line arguments and executes subcommands if necessary.
     * 
     * @param argc Argument count.
     * @param argv Argument vector.
     * @return result Information about whether the application should exit or continue.
     */
    result parse(int argc, char** argv);

    static std::string default_engine_resource_path();

private:
};

} // namespace wsl::cli
