// Linux loader for librenderdoc.so. Detects an already-loaded module
// with RTLD_NOLOAD; never opens the .so itself.
#include "renderdoc.hpp"

#if defined(__linux__)
#include <dlfcn.h>

namespace wsl::gfx::rdoc::detail
{

void *
try_load_module (void *&out_module)
{
  out_module = nullptr;

  // Prefer librenderdoc.so (desktop). On Android the injected layer is
  // libVkLayer_GLES_RenderDoc.so, but Weasel targets desktop Linux so we
  // do not probe that path here.
  void *mod = dlopen ("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
  if (mod == nullptr) {
    return nullptr;
  }

  void *sym = dlsym (mod, "RENDERDOC_GetAPI");
  if (sym == nullptr) {
    // The module is present but the entry point is missing - very
    // unlikely, but treat the same as "not loaded" so we don't hold
    // a dangling reference.
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

// Stub: this translation unit is built on non-Linux platforms only to
// satisfy the linker. The actual loader lives in
// renderdoc_load_windows.cpp / renderdoc_load_apple.cpp. Marked inline
// so the per-platform stubs (which are identical) do not collide at
// link time.
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
