#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>

namespace wsl::das
{

/*!
 * \brief daslang virtual machine wrapper.
 *
 * Provides an interface for compiling and executing daslang files,
 * as well as finding and calling daslang functions.
 */
class das_engine
{
public:
  das_engine ();
  ~das_engine ();

  // Non-copyable
  das_engine (const das_engine &) = delete;
  das_engine &operator= (const das_engine &) = delete;

  /*!
   * \brief One-time global daScript initialization on the main thread.
   *
   * Calls Module::Initialize() exactly once. Registered an atexit handler
   * for Module::Shutdown(). Must be called from the main thread before any
   * worker thread creates a das_engine.
   */
  static bool initialize_global ();

  /*!
   * \brief Initializes a das engine instance (compilation infrastructure).
   * \return \c true on success, \c false on failure.
   */
  bool initialize ();

  /*!
   * \brief Compiles and executes a .das file.
   * \param path Path to the daslang file.
   * \return \c true on success, \c false on failure.
   */
  bool execute_file (const std::filesystem::path &path);

  enum class field_type_kind
  {
    integer,
    unsigned_integer,
    floating,
    boolean,
    string,
    unsupported
  };

  struct field_info
  {
    std::string name;
    std::string type_name;
    int offset = 0;
    int size = 0;
    field_type_kind kind = field_type_kind::unsupported;
  };

  struct struct_info
  {
    std::vector<field_info> fields;
    int size_of = 0;
  };

  /*!
   * \brief Returns the fields and size of a struct defined in the given file.
   *
   * Must be called after execute_file for the same path.
   */
  struct_info get_struct_info (const std::filesystem::path &path,
                               const std::string &struct_name);

  /*!
   * \brief Allocates a zero-initialized instance of a daslang struct on the
   *        VM heap. The pointer remains valid for the lifetime of the engine.
   *
   * Must be called after execute_file for the same path.
   */
  void *allocate_instance (const std::filesystem::path &path,
                           const std::string &struct_name);

  /*!
   * \brief Returns a pointer to a field within a VM-allocated instance.
   */
  static void *get_field_ptr (void *instance, int offset);

  /*!
   * \brief Copies `field_size` bytes from a field within an instance
   *        into `out`.
   */
  static void get_field (void *instance, int offset, int field_size, void *out,
                         int out_size);

  /*!
   * \brief Copies `field_size` bytes from `value` into a field within
   *        an instance.
   */
  static void set_field (void *instance, int offset, int field_size,
                         const void *value, int value_size);

  /*!
   * \brief Finds and calls a void function by name with no arguments.
   * \param func_name Name of the function to call.
   * \return \c true if the function was found and called, \c false otherwise.
   */
  bool call_void_function (const char *func_name);

  /*!
   * \brief Finds and calls a function returning int by name with no arguments.
   * \param func_name Name of the function to call.
   * \param out_value Pointer to store the return value (may be nullptr).
   * \return \c true if the function was found and called, \c false otherwise.
   */
  bool call_int_function (const char *func_name, int *out_value = nullptr);

  /*!
   * \brief Returns the error message from the last failed operation.
   */
  const std::string &
  last_error () const
  {
    return m_last_error;
  }

  /*!
   * \brief Returns the list of executed file paths.
   */
  const std::vector<std::string> &
  executed_files () const
  {
    return m_executed_files;
  }

  /*!
   * \brief Shuts down the daslang VM.
   */
  void shutdown ();

private:
  struct impl;
  std::unique_ptr<impl> m_impl;
  std::string m_last_error;
  std::vector<std::string> m_executed_files;
  bool m_initialized = false;
};

} // namespace wsl::das
