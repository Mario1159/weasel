// macOS loader for librenderdoc.dylib. Same strategy as Linux: probe
// for an already-loaded module with RTLD_NOLOAD.
#include "renderdoc.hpp"

#if defined(__APPLE__)
#include <dlfcn.h>

namespace wsl::gfx::rdoc::detail
{

void *
try_load_module (void *&out_module)
{
  out_module = nullptr;

  void *mod = dlopen ("librenderdoc.dylib", RTLD_NOW | RTLD_NOLOAD);
  if (mod == nullptr) {
    return nullptr;
  }

  void *sym = dlsym (mod, "RENDERDOC_GetAPI");
  if (sym == nullptr) {
    dlclose (mod);
    return nullptr;
  }

  out_module = mod;
  return sym;
}

void
release_module (void *module)
{
  if (module != nullptr) {
    dlclose (module);
  }
}

} // namespace wsl::gfx::rdoc::detail

#else

// Stub on non-Apple platforms. inline so the three per-platform stubs
// can coexist without link-time symbol collisions.
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
