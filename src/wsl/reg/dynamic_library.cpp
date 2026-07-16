#include "dynamic_library.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace wsl::reg
{

dynamic_library::dynamic_library (const std::filesystem::path &path)
{
#if defined(_WIN32)
  m_handle = static_cast<void *> (LoadLibraryW (path.wstring ().c_str ()));
#else
  m_handle = dlopen (path.c_str (), RTLD_LAZY | RTLD_LOCAL);
#endif
}

dynamic_library::~dynamic_library () { unload (); }

std::string
dynamic_library::last_error () const
{
#if defined(_WIN32)
  DWORD err = GetLastError ();
  if (err == 0)
    return {};
  char buf[256];
  FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                  sizeof (buf), nullptr);
  return buf;
#else
  const char *err = dlerror ();
  return err ? std::string (err) : std::string ();
#endif
}

void *
dynamic_library::get_symbol (const char *name)
{
  if (m_handle == nullptr || name == nullptr) {
    return nullptr;
  }

#if defined(_WIN32)
  return reinterpret_cast<void *> (
      GetProcAddress (static_cast<HMODULE> (m_handle), name));
#else
  return dlsym (m_handle, name);
#endif
}

bool
dynamic_library::unload ()
{
  if (m_handle == nullptr) {
    return false;
  }

#if defined(_WIN32)
  bool result = FreeLibrary (static_cast<HMODULE> (m_handle)) != 0;
#else
  bool result = dlclose (m_handle) == 0;
#endif

  if (result) {
    m_handle = nullptr;
  }

  return result;
}

} // namespace wsl::reg
