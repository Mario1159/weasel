#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace clang
{
class Interpreter;
}

namespace wsl
{

namespace comp::singl
{
class runtime_context;
}

namespace rsc
{
struct project;
}

namespace reg
{
/*!
 * \brief Runtime module loading, code interpretation, and registration helpers.
 *
 * The \c runtime sub-namespace provides:
 * - \c runtime_project_module: builds and interprets user-authored
 *   runtime code (headers + sources) via Clang-Repl.
 * - \c runtime_registrar (and its alias \c runtime_detail): helper
 *   functions and macro support for registering components, singletons,
 *   and systems from interpreted/compiled runtime code.
 * - \c runtime_module_registration_context: bundles the three
 *   registration contexts (\c component_registry, \c singleton_registry,
 *   \c system_factory_registry) passed to runtime registration callbacks.
 */
namespace runtime
{

/*!
 * \brief Builds and loads user-authored runtime code for a project.
 *
 * A runtime project module discovers project headers and source files,
 * generates a translation unit that wires up registration hooks, and then
 * interprets that code using Clang-Repl.
 */
class runtime_project_module
{
public:
  /*!
   * \brief Creates a runtime module bound to a runtime context.
   * \param runtime_ctx Runtime context that owns the registries to populate.
   */
  explicit runtime_project_module (comp::singl::runtime_context *runtime_ctx);
  ~runtime_project_module ();

  /*!
   * \brief Rebuilds and loads the runtime code for a project.
   * \param project Project description that provides source directories.
   * \return \c true on success, otherwise \c false.
   */
  bool compile_and_load (const rsc::project &project);

  /*!
   * \brief Loads cached runtime registration metadata for a project.
   *
   * This is a fast path for commands that only need runtime names and system
   * placeholders. It does not load user C++ types.
   */
  bool load_cached_metadata (const rsc::project &project);

  /*!
   * \brief Finalizes the loading process on the main thread.
   *
   * This clears the registries and applies the interpreted registrations.
   */
  void finalize_load ();

  /*!
   * \brief Clears runtime-loaded registrations and resets runtime-module state.
   */
  void unload ();

  /*!
   * \brief Returns the latest human-readable status message.
   * \return The most recent status string.
   */
  const std::string &
  last_status () const
  {
    return m_last_status;
  }

  /*!
   * \brief Reports whether a runtime module has already been loaded.
   * \return \c true if runtime code is active for this session.
   */
  bool
  has_loaded_module () const
  {
    return m_module_loaded;
  }

  /*!
   * \brief Reports whether cached runtime metadata is active.
   */
  bool
  has_loaded_cached_metadata () const
  {
    return m_metadata_cache_loaded;
  }

  struct cached_registration
  {
    std::uint64_t type_id = 0;
    std::string type_name;
    std::string display_name;
  };

  struct registration_cache
  {
    std::size_t source_hash = 0;
    std::vector<cached_registration> components;
    std::vector<cached_registration> singletons;
    std::vector<cached_registration> systems;
  };

private:
  struct source_set
  {
    std::vector<std::filesystem::path> headers;
    std::vector<std::filesystem::path> sources;
  };

  static void gather_files (const std::filesystem::path &base, source_set &out);

  static std::size_t compute_source_hash (const source_set &sources);

  static std::filesystem::path
  registration_cache_path (const std::filesystem::path &project_root);

  static bool read_registration_cache (const std::filesystem::path &path,
                                       std::size_t source_hash,
                                       registration_cache &out);

  bool write_registration_cache () const;

  void apply_registration_cache (const registration_cache &cache);

  static bool
  write_generated_translation_unit (const std::filesystem::path &generated_path,
                                    const source_set &sources);

  bool initialize_interpreter ();
  bool interpret (const std::filesystem::path &generated_path);

  comp::singl::runtime_context *m_runtime_ctx = nullptr;
  std::filesystem::path m_loaded_project_root;
  std::string m_last_status;
  bool m_module_loaded = false;
  bool m_metadata_cache_loaded = false;
  std::size_t m_source_hash = 0;

  std::vector<std::string> m_interpreter_args_storage;
  std::vector<const char *> m_interpreter_args;
  std::unique_ptr<clang::Interpreter> m_interpreter;
};

} // namespace runtime
} // namespace reg

} // namespace wsl
