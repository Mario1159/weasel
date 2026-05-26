#include "cli_handler.hpp"
#include "repl_handler.hpp"
#include "wsl/log/log.hpp"
#include <iostream>

int main(int argc, char** argv) {
    wsl::log::init();

    wsl::cli::cli_handler cli;
    auto const result = cli.parse(argc, argv);

    if (result.should_exit) return result.exit_code;

    if (result.interactive) {
        wsl::cli::repl_handler repl(wsl::cli::cli_handler::default_engine_resource_path(), result.attach);
        repl.run(result.project_to_load, result.scene_to_load);
        return 0;
    }

    if (result.command) {
        wsl::cli::repl_handler repl(wsl::cli::cli_handler::default_engine_resource_path(), result.attach);
        if (!repl.prepare(result.project_to_load, result.scene_to_load)) {
            return 1;
        }
        repl.execute_command(*result.command);
        return 0;
    }

    return result.exit_code;
}
