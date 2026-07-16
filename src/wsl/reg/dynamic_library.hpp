#pragma once

#include <filesystem>
#include <string>

namespace wsl::reg
{

/*!
 * \brief Platform-specific dynamic library loader.
 *
 * Wraps dlopen/LoadLibrary for loading shared libraries (.so, .dylib, .dll)
 * at runtime.
 */
class dynamic_library
{
public:
  /*!
   * \brief Loads a shared library from the given path.
   * \param path Path to the shared library file.
   */
  explicit dynamic_library (const std::filesystem::path &path);

  /*!
   * \brief Unloads the library if loaded.
   */
  ~dynamic_library ();

  // Non-copyable
  dynamic_library (const dynamic_library &) = delete;
  dynamic_library &operator= (const dynamic_library &) = delete;

  /*!
   * \brief Reports whether the library was loaded successfully.
   * \return \c true if the library is loaded and ready.
   */
  bool
  is_loaded () const
  {
    return m_handle != nullptr;
  }

  /*!
   * \brief Returns the last error message from the platform loader.
   * \return Human-readable error string, or empty if no error.
   */
  std::string last_error () const;

  /*!
   * \brief Returns a pointer to the named symbol.
   * \param name Symbol name to look up.
   * \return Pointer to the symbol, or \c nullptr if not found.
   */
  void *get_symbol (const char *name);

  /*!
   * \brief Unloads the library.
   * \return \c true on success, \c false if already unloaded.
   */
  bool unload ();

private:
  void *m_handle = nullptr;
};

} // namespace wsl::reg
