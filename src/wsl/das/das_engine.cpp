#include "das_engine.hpp"

#include <cstdlib>
#include <cstring>

#if WEASEL_HAS_DASLANG
#include "daScript/ast/dyn_modules.h"
#include "daScript/daScript.h"
#include "wsl_api_module.hpp"
#include "../log/log.hpp"

using namespace das;

DECLARE_ALL_DEFAULT_MODULES;

namespace
{

bool
initialize_modules_for_engine (TextPrinter &tout,
                               smart_ptr<FsFileAccess> &faccess,
                               ModuleGroup &module_group)
{
  // Set up the daslang source tree path and file access.
  setDasRoot (std::string (WEASEL_BUILD_DIR) + "/_deps/daslang-src");

  faccess = smart_ptr<FsFileAccess> (new FsFileAccess);
  PULL_ALL_DEFAULT_MODULES;
  vector<string> empty_modules;
  require_dynamic_modules (faccess, getDasRoot (), "./", empty_modules, tout);

  // Register the Weasel API module (ECS, transforms, scene queries).
  wsl::das::register_wsl_api_module (module_group);

  // Module::Initialize() must be called on the main thread (see
  // das_engine::initialize_global). On worker threads we only need the
  // per-thread daScriptEnvironment to exist and have its
  // g_modulesInitialized flag set so that compileDaScript's assertion
  // passes. The module list itself is global and already populated.
  ::das::daScriptEnvironment::ensure ();
  ::das::daScriptEnvironment::getBound ()->g_modulesInitialized = true;

  return true;
}

} // anonymous namespace

// Defined outside namespace wsl::das because PULL_ALL_DEFAULT_MODULES
// expands to code using das::ModuleKarma which fails inside a nested namespace.

static bool s_das_global_initialized = false;

static void
das_global_atexit ()
{
  if (s_das_global_initialized) {
    ::das::Module::Shutdown ();
  }
}

bool
wsl::das::das_engine::initialize_global ()
{
  if (s_das_global_initialized) {
    return true;
  }
  // Register all default built-in modules and initialize the module registry.
  // This MUST happen on the main thread because Module::Shutdown() (called
  // via atexit, which always runs on the main thread) destroys the global
  // module list and thread-local daScriptEnvironment.
  ::das::register_builtin_modules ();
  ::das::Module::Initialize ();
  s_das_global_initialized = true;
  // atexit runs LIFO — our handler runs before daScript's own atexit handler
  // (which checks g_envTotal and calls _Exit(1) if unbalanced).
  std::atexit (das_global_atexit);
  return true;
}

namespace wsl::das
{

struct das_engine::impl
{
  ::das::TextPrinter tout;
  ::das::smart_ptr<::das::FsFileAccess> faccess;
  ::das::ModuleGroup module_group;
  ::das::CodeOfPolicies policies;
  std::unordered_map<std::string, ::das::ProgramPtr> compiled_programs;
  std::unique_ptr<::das::Context> persistent_ctx;
  // Merged function lookup across all compiled programs.  The context's own
  // tabMnLookup is replaced by each program->simulate(), so we maintain our
  // own map that survives multiple simulate() calls.
  std::unordered_map<std::string, ::das::SimFunction *> fn_lookup;

  impl ()
  {
    policies.aot = true;
    policies.fail_on_no_aot = false;
  }

  bool
  initialize ()
  {
    return initialize_modules_for_engine (tout, faccess, module_group);
  }

  bool
  execute_file (const std::string &path, std::string &error)
  {
    auto program = compileDaScript (path.c_str (), faccess, tout, module_group,
                                    policies);
    if (program->failed ()) {
      std::ostringstream oss;
      for (const auto &err : program->errors) {
        oss << reportError (err.at, err.what, err.extra, err.fixme, err.cerr)
            << "\n";
      }
      error = oss.str ();
      return false;
    }

    // Run in a temporary context to execute initialization code.
    ::das::Context ctx (program->getContextStackSize ());
    if (!program->simulate (ctx, tout)) {
      std::ostringstream oss;
      for (const auto &err : program->errors) {
        oss << reportError (err.at, err.what, err.extra, err.fixme, err.cerr)
            << "\n";
      }
      error = oss.str ();
      return false;
    }

    compiled_programs[path] = program;

    // Ensure the persistent context is large enough for all compiled programs.
    uint32_t needed = program->getContextStackSize ();
    bool ctx_recreated = false;
    if (!persistent_ctx || persistent_ctx->stack.size () < needed) {
      persistent_ctx = std::make_unique<::das::Context> (needed);
      ctx_recreated = true;
    }

    // When the context is recreated, re-simulate ALL previously compiled
    // programs so their functions remain available for runtime calls.
    if (ctx_recreated) {
      for (auto &[p_path, p_prog] : compiled_programs) {
        if (!p_prog->simulate (*persistent_ctx, tout)) {
          error = "Failed to re-simulate program: " + p_path;
          return false;
        }
      }
    } else {
      // Just simulate the current program on the existing context.
      if (!program->simulate (*persistent_ctx, tout)) {
        std::ostringstream oss;
        for (const auto &err : program->errors) {
          oss << reportError (err.at, err.what, err.extra, err.fixme, err.cerr)
              << "\n";
        }
        error = oss.str ();
        return false;
      }
    }

    // After each program->simulate(), the context's tabMnLookup is replaced
    // with only that program's functions.  Merge into our persistent fn_lookup
    // so all previously compiled programs' functions remain findable.
    if (persistent_ctx->tabMnLookup) {
      wsl::log::cmake ()->debug ("execute_file: tabMnLookup has {} entries",
                                 persistent_ctx->tabMnLookup->size ());
      for (auto &kv : *persistent_ctx->tabMnLookup) {
        auto *sf = kv.second;
        if (sf && sf->name) {
          fn_lookup[sf->name] = sf;
          wsl::log::cmake ()->debug ("  fn_lookup: '{}'", sf->name);
        }
      }
    } else {
      wsl::log::cmake ()->debug (
          "execute_file: tabMnLookup is NULL after simulate!");
    }

    return true;
  }

  void *
  allocate_instance (const std::string &path, const std::string &struct_name,
                     std::string &error)
  {
    if (!persistent_ctx) {
      error = "No persistent context available";
      return nullptr;
    }

    auto it = compiled_programs.find (path);
    if (it == compiled_programs.end ()) {
      error = "Program not found for path: " + path;
      return nullptr;
    }
    auto program = it->second;
    if (!program || !program->thisModule) {
      error = "Invalid program";
      return nullptr;
    }
    auto structs = program->findStructure (struct_name.c_str ());
    if (structs.empty ()) {
      error = "Struct not found: " + struct_name;
      return nullptr;
    }
    auto st = structs[0];
    int bytes = st->getSizeOf ();
    if (bytes <= 0) {
      error = "Struct has zero size: " + struct_name;
      return nullptr;
    }

    // Allocate zero-initialized memory on the VM heap.
    char *ptr
        = persistent_ctx->allocate (static_cast<uint64_t> (bytes), nullptr);
    memset (ptr, 0, static_cast<std::size_t> (bytes));
    return ptr;
  }

  das_engine::field_type_kind
  classify_field_type (const ::das::TypeDecl *td)
  {
    if (!td) {
      return das_engine::field_type_kind::unsupported;
    }
    if (td->isBool ()) {
      return das_engine::field_type_kind::boolean;
    }
    if (td->isString ()) {
      return das_engine::field_type_kind::string;
    }
    if (td->isSignedInteger ()) {
      return das_engine::field_type_kind::integer;
    }
    if (td->isUnsignedInteger ()) {
      return das_engine::field_type_kind::unsigned_integer;
    }
    if (td->isFloatOrDouble ()) {
      return das_engine::field_type_kind::floating;
    }
    return das_engine::field_type_kind::unsupported;
  }

  int
  field_type_size (const ::das::TypeDecl *td)
  {
    if (!td) {
      return 0;
    }
    if (td->isBool ()) {
      return sizeof (bool);
    }
    if (td->isString ()) {
      return 0;
    }
    if (td->isSignedInteger () || td->isUnsignedInteger ()
        || td->isFloatOrDouble ()) {
      switch (td->baseType) {
      case ::das::Type::tInt8:
      case ::das::Type::tUInt8:
        return 1;
      case ::das::Type::tInt16:
      case ::das::Type::tUInt16:
        return 2;
      case ::das::Type::tInt:
      case ::das::Type::tUInt:
      case ::das::Type::tFloat:
        return 4;
      case ::das::Type::tInt64:
      case ::das::Type::tUInt64:
      case ::das::Type::tDouble:
        return 8;
      default:
        return 4;
      }
    }
    return 0;
  }

  das_engine::struct_info
  get_struct_info (const std::string &path, const std::string &struct_name)
  {
    das_engine::struct_info result;
    auto it = compiled_programs.find (path);
    if (it == compiled_programs.end ()) {
      return result;
    }
    auto program = it->second;
    if (!program || !program->thisModule) {
      return result;
    }
    auto structs = program->findStructure (struct_name.c_str ());
    if (structs.empty ()) {
      return result;
    }
    auto st = structs[0];
    result.size_of = st->getSizeOf ();
    for (auto &field : st->fields) {
      das_engine::field_info fi;
      fi.name = field.name;
      fi.type_name = field.type ? field.type->describe () : "unknown";
      fi.offset = field.offset;
      fi.size = field.type ? field_type_size (field.type) : 0;
      fi.kind = classify_field_type (field.type);
      result.fields.push_back (std::move (fi));
    }
    return result;
  }

  bool
  call_void_function (const char *func_name, ::das::Context &ctx,
                      std::string &error)
  {
    auto it = fn_lookup.find (func_name);
    if (it == fn_lookup.end () || !it->second) {
      error = "Function '";
      error += func_name;
      error += "' not found (fn_lookup size="
               + std::to_string (fn_lookup.size ()) + ").";
      if (!fn_lookup.empty ()) {
        error += " Available:";
        for (auto &kv : fn_lookup) {
          error += " '";
          error += kv.first;
          error += "'";
        }
      }
      return false;
    }

    ctx.evalWithCatch (it->second, nullptr);
    return true;
  }

  bool
  call_int_function (const char *func_name, ::das::Context &ctx, int &out_value,
                     std::string &error)
  {
    auto it = fn_lookup.find (func_name);
    if (it == fn_lookup.end () || !it->second) {
      error = "Function '";
      error += func_name;
      error += "' not found.";
      return false;
    }

    ctx.evalWithCatch (it->second, nullptr);
    return true;
  }

  void
  shutdown ()
  {
    // Release engine-owned daScript resources (programs, context, file access).
    // Do NOT call Module::Shutdown() here — it was never called per-engine
    // instance. Module::Initialize/Shutdown is managed globally on the main
    // thread via initialize_global().
    persistent_ctx.reset ();
    compiled_programs.clear ();
    faccess.reset ();
  }
};

das_engine::das_engine () : m_impl (std::make_unique<impl> ()) {}

das_engine::~das_engine () { shutdown (); }

bool
das_engine::initialize ()
{
  if (m_initialized) {
    return true;
  }

  if (!m_impl->initialize ()) {
    m_last_error = "Failed to initialize daslang engine";
    return false;
  }

  m_initialized = true;
  return true;
}

bool
das_engine::execute_file (const std::filesystem::path &path)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  if (!m_impl->execute_file (path.string (), m_last_error)) {
    return false;
  }

  m_executed_files.push_back (path.string ());
  return true;
}

das_engine::struct_info
das_engine::get_struct_info (const std::filesystem::path &path,
                             const std::string &struct_name)
{
  if (!m_initialized) {
    return {};
  }
  return m_impl->get_struct_info (path.string (), struct_name);
}

void *
das_engine::allocate_instance (const std::filesystem::path &path,
                               const std::string &struct_name)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return nullptr;
  }
  void *ptr
      = m_impl->allocate_instance (path.string (), struct_name, m_last_error);
  return ptr;
}

void *
das_engine::get_field_ptr (void *instance, int offset)
{
  if (!instance) {
    return nullptr;
  }
  return static_cast<char *> (instance) + offset;
}

void
das_engine::get_field (void *instance, int offset, int field_size, void *out,
                       int out_size)
{
  if (!instance || field_size <= 0 || !out || out_size <= 0) {
    return;
  }
  int copy_size = field_size < out_size ? field_size : out_size;
  std::memcpy (out, static_cast<char *> (instance) + offset,
               static_cast<std::size_t> (copy_size));
}

void
das_engine::set_field (void *instance, int offset, int field_size,
                       const void *value, int value_size)
{
  if (!instance || field_size <= 0 || !value || value_size <= 0) {
    return;
  }
  int copy_size = field_size < value_size ? field_size : value_size;
  std::memcpy (static_cast<char *> (instance) + offset, value,
               static_cast<std::size_t> (copy_size));
}

bool
das_engine::call_void_function (const char *func_name)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  if (!m_impl->persistent_ctx) {
    m_last_error = "No persistent context available";
    return false;
  }

  return m_impl->call_void_function (func_name, *m_impl->persistent_ctx,
                                     m_last_error);
}

bool
das_engine::call_int_function (const char *func_name, int *out_value)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  if (!m_impl->persistent_ctx) {
    m_last_error = "No persistent context available";
    return false;
  }

  int dummy = 0;
  return m_impl->call_int_function (func_name, *m_impl->persistent_ctx,
                                    out_value ? *out_value : dummy,
                                    m_last_error);
}

void
das_engine::shutdown ()
{
  if (m_initialized) {
    m_impl->shutdown ();
    m_initialized = false;
  }
}

} // namespace wsl::das

#else

namespace wsl::das
{

struct das_engine::impl
{
};

das_engine::das_engine () : m_impl (std::make_unique<impl> ()) {}
das_engine::~das_engine () = default;

bool
das_engine::initialize ()
{
  m_last_error = "daslang support not enabled";
  return false;
}

bool
das_engine::execute_file (const std::filesystem::path &)
{
  m_last_error = "daslang support not enabled";
  return false;
}

bool
das_engine::call_void_function (const char *)
{
  m_last_error = "daslang support not enabled";
  return false;
}

bool
das_engine::call_int_function (const char *, int *)
{
  m_last_error = "daslang support not enabled";
  return false;
}

das_engine::struct_info
das_engine::get_struct_info (const std::filesystem::path &, const std::string &)
{
  return {};
}

void *
das_engine::allocate_instance (const std::filesystem::path &,
                               const std::string &)
{
  m_last_error = "daslang support not enabled";
  return nullptr;
}

void *
das_engine::get_field_ptr (void *instance, int offset)
{
  if (!instance) {
    return nullptr;
  }
  return static_cast<char *> (instance) + offset;
}

void
das_engine::get_field (void *instance, int offset, int field_size, void *out,
                       int out_size)
{
  if (!instance || field_size <= 0 || !out || out_size <= 0) {
    return;
  }
  int copy_size = field_size < out_size ? field_size : out_size;
  std::memcpy (out, static_cast<char *> (instance) + offset,
               static_cast<std::size_t> (copy_size));
}

void
das_engine::set_field (void *instance, int offset, int field_size,
                       const void *value, int value_size)
{
  if (!instance || field_size <= 0 || !value || value_size <= 0) {
    return;
  }
  int copy_size = field_size < value_size ? field_size : value_size;
  std::memcpy (static_cast<char *> (instance) + offset, value,
               static_cast<std::size_t> (copy_size));
}

void
das_engine::shutdown ()
{
}

} // namespace wsl::das

#endif
