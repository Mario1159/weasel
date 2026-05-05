#include "cli_handler.hpp"
#include "repl_handler.hpp"
#include <SDL3/SDL_init.h>
#include <iostream>

int main(int argc, char** argv) {
    wsl::cli::cli_handler cli;
    auto const result = cli.parse(argc, argv);

    if (result.should_exit) return result.exit_code;

    // Only init SDL if not attaching to editor (editor handles rendering)
    // Also need SDL for local command execution that uses runtime_context
    if (!result.attach) {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }

    if (result.interactive) {
        wsl::cli::repl_handler repl(wsl::cli::cli_handler::default_engine_resource_path(), result.attach);
        repl.run(result.project_to_load);
        if (!result.attach) SDL_Quit();
        return 0;
    }

    if (result.command) {
        wsl::cli::repl_handler repl(wsl::cli::cli_handler::default_engine_resource_path(), result.attach);
        if (result.project_to_load && !result.attach) {
            repl.execute_command("proj load " + *result.project_to_load);
        }
        repl.execute_command(*result.command);
        if (!result.attach) SDL_Quit();
        return 0;
    }

    if (!result.attach) SDL_Quit();
    return result.exit_code;
}
