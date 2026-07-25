#pragma once

#include "../das/das_engine.hpp"
#include <filesystem>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace wsl
{

class app;

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

class dynamic_library;

/*!
 * \brief Runtime module loading, code compilation, and registration helpers.
 *
 * The \c runtime sub-namespace provides:
 * - \c runtime_project_module: builds and loads user-authored
 *   runtime code (headers + sources) by compiling to shared libraries,
 *   and executes daslang files.
 * - \c runtime_registrar (and its alias \c runtime_detail): helper
 *   functions and macro support for registering components, singletons,
 *   and systems from compiled runtime code.
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
 * compiles it to a shared library (.so/.dylib/.dll) for loading.
 * It also discovers and executes daslang (.das) files.
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

  /*! \brief Starts an asynchronous reload of the runtime code.
   *
   * Use \c poll_async_reload() in the main loop to check completion and
   * call \c finalize_load() on the main thread when ready.
   */
  void compile_and_load_async (const rsc::project &project);

  /*! \brief Polls an in-progress async reload.
   *
   * If the background compilation finished, this calls \c finalize_load()
   * on the calling (main) thread and returns \c true.
   * \return \c true if a reload just completed this call, otherwise \c false.
   */
  bool poll_async_reload ();

  /*! \brief Reports whether an async reload is currently in progress.
   *
   * \return \c true if compilation is still running in the background.
   */
  bool is_reloading () const;

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
   * This clears the registries and applies the registrations.
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

  /*!
   * \brief Returns the daslang engine instance.
   *
   * The engine is created on first use. Returns nullptr if daslang
   * is not enabled.
   */
  wsl::das::das_engine *get_das_engine ();

  // User C++ hook function pointer types
  using hook_init_fn = void (*) (wsl::app &);
  using hook_update_fn = void (*) (wsl::app &, double);
  using hook_shutdown_fn = void (*) (wsl::app &);

  /*!
   * \brief Returns the resolved user init hook, or nullptr if not available.
   */
  hook_init_fn
  get_hook_init () const
  {
    return m_hook_init;
  }

  /*!
   * \brief Returns the resolved user update hook, or nullptr if not available.
   */
  hook_update_fn
  get_hook_update () const
  {
    return m_hook_update;
  }

  /*!
   * \brief Returns the resolved user shutdown hook, or nullptr if not
   * available.
   */
  hook_shutdown_fn
  get_hook_shutdown () const
  {
    return m_hook_shutdown;
  }

  struct cached_das_field
  {
    std::string name;
    std::string type_name;
    int offset = 0;
    int size = 0;
    int kind = 0;
  };

  struct cached_registration
  {
    std::uint64_t type_id = 0;
    std::string type_name;
    std::string display_name;
    bool is_das_component = false;
    int das_struct_size = 0;
    std::vector<cached_das_field> das_fields;
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
    std::vector<std::filesystem::path> cpp_sources;
    std::vector<std::filesystem::path> das_sources;
  };

  struct das_registration
  {
    enum kind_t
    {
      component,
      singleton,
      system
    };
    kind_t kind;
    std::string type_name;
    std::string display_name;
    std::uint64_t type_id;
    int struct_size = 0;
    std::string script_path;
    std::vector<wsl::das::das_engine::field_info> fields;
  };

  static void gather_files (const std::filesystem::path &base, source_set &out);

  static bool is_das_file (const std::filesystem::path &path);

  static std::size_t compute_source_hash (const source_set &sources);

  static std::filesystem::path
  registration_cache_path (const std::filesystem::path &project_root);

  static bool read_registration_cache (const std::filesystem::path &path,
                                       std::size_t source_hash,
                                       registration_cache &out);

  bool write_registration_cache () const;

  void apply_registration_cache (const registration_cache &cache);

  void load_das_registrations_from_cache (const registration_cache &cache);

  static bool
  write_generated_translation_unit (const std::filesystem::path &generated_path,
                                    const source_set &sources);

  // ── Shared library cache ──
  static std::filesystem::path
  shared_library_path (const std::filesystem::path &project_root);

  static std::filesystem::path
  source_hash_path (const std::filesystem::path &project_root);

  bool compile_to_shared_library (const std::filesystem::path &generated_path,
                                  const std::filesystem::path &output_path);

  bool try_load_cached_shared_library (std::size_t current_hash);

  bool read_source_hash (const std::filesystem::path &path,
                         std::size_t &out_hash) const;

  void write_source_hash (const std::filesystem::path &path,
                          std::size_t hash) const;

  comp::singl::runtime_context *m_runtime_ctx = nullptr;
  std::filesystem::path m_loaded_project_root;
  std::string m_last_status;
  bool m_module_loaded = false;
  bool m_metadata_cache_loaded = false;
  std::size_t m_source_hash = 0;

  std::unique_ptr<dynamic_library> m_loaded_library;
  std::unique_ptr<wsl::das::das_engine> m_das_engine;
  std::vector<das_registration> m_das_registrations;

  // User C++ hook function pointers resolved from the loaded shared library
  hook_init_fn m_hook_init = nullptr;
  hook_update_fn m_hook_update = nullptr;
  hook_shutdown_fn m_hook_shutdown = nullptr;

  void resolve_user_hooks ();

  std::future<bool> m_async_reload_future;
};

} // namespace runtime
} // namespace reg

} // namespace wsl
