#pragma once

#include <filesystem>
#include <string>

namespace wsl::reg
{

/**
 * Platform-specific dynamic library loader.
 *
 * Wraps dlopen/LoadLibrary for loading shared libraries (.so, .dylib, .dll)
 * at runtime.
 */
class dynamic_library
{
public:
  /**
 * Loads a shared library from the given path.
 * :param path: Path to the shared library file.
 */
  explicit dynamic_library (const std::filesystem::path &path);

  /** Unloads the library if loaded. */
  ~dynamic_library ();

  // Non-copyable
  dynamic_library (const dynamic_library &) = delete;
  dynamic_library &operator= (const dynamic_library &) = delete;

  /**
 * Reports whether the library was loaded successfully.
 * :return: \c true if the library is loaded and ready.
 */
  bool
  is_loaded () const
  {
    return m_handle != nullptr;
  }

  /**
 * Returns the last error message from the platform loader.
 * :return: Human-readable error string, or empty if no error.
 */
  std::string last_error () const;

  /**
 * Returns a pointer to the named symbol.
 * :param name: Symbol name to look up.
 * :return: Pointer to the symbol, or \c nullptr if not found.
 */
  void *get_symbol (const char *name);

  /**
 * Unloads the library.
 * :return: \c true on success, \c false if already unloaded.
 */
  bool unload ();

private:
  void *m_handle = nullptr;
};

} // namespace wsl::reg
