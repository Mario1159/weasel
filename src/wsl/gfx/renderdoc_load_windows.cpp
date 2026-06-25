// Windows loader for renderdoc.dll. Uses GetModuleHandleA to detect a
// process that was launched under renderdocui.
#include "renderdoc.hpp"

#if defined(_WIN32)
#include <windows.h>

namespace wsl::gfx::rdoc::detail
{

void *
try_load_module (void *&out_module)
{
  out_module = nullptr;

  HMODULE mod = GetModuleHandleA ("renderdoc.dll");
  if (mod == nullptr) {
    return nullptr;
  }

  void *sym
      = reinterpret_cast<void *> (GetProcAddress (mod, "RENDERDOC_GetAPI"));
  if (sym == nullptr) {
    return nullptr;
  }

  // We do not call FreeLibrary - the module is owned by the loader.
  out_module = mod;
  return sym;
}

void
release_module (void * /*module*/)
{
  // Intentionally empty. The renderdoc.dll is injected by renderdocui
  // and we should not unload it.
}

} // namespace wsl::gfx::rdoc::detail

#else

// Stub on non-Windows platforms. inline so the three per-platform
// stubs can coexist without link-time symbol collisions.
namespace wsl::gfx::rdoc::detail
{
inline void *
try_load_module (void *&out_module)
{
  out_module = nullptr;
  return nullptr;
}
inline void
release_module (void * /*module*/)
{
}
} // namespace wsl::gfx::rdoc::detail

#endif
