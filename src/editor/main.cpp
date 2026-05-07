#include "cli/cli_handler.hpp"
#include "editor_app.hpp"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_filesystem.h>
#include <filesystem>

namespace {
std::string
default_engine_resource_path ()
{
  // Try to locate resources relative to the executable at runtime.
  // This allows the binary to work from any install prefix (e.g. /usr/local,
  // ~/.local, or an extracted archive) without recompilation.
  const char *base_path = SDL_GetBasePath ();
  if (base_path != nullptr)
    {
      std::filesystem::path exe_dir (base_path);

      std::filesystem::path share_dir = exe_dir / ".." / "share" / "weasel";
      if (std::filesystem::exists (share_dir / "compiled_shaders"))
        return share_dir.string ();
    }

  // Fallback to compile-time paths for development builds.
#ifdef WEASEL_BUILD_DIR
  return WEASEL_BUILD_DIR;
#elif defined(WEASEL_SOURCE_DIR)
  return WEASEL_SOURCE_DIR;
#else
  return ".";
#endif
}
}

int
main (int argc, char **argv)
{
  wsl::cli::cli_handler cli;
  auto const result = cli.parse(argc, argv);

  if (result.should_exit) {
    return result.exit_code;
  }

  SDL_Init (SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  
  editor::editor_app g ("Incantation", 1280, 720,
                        default_engine_resource_path ());
  
  if (result.project_to_load) {
      g.set_project_path(*result.project_to_load);
  }

  return g.run ();
}
