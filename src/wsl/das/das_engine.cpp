#include "das_engine.hpp"

#include <cstdlib>
#include <cstring>

#if WEASEL_HAS_DASLANG
#include "daScript/ast/dyn_modules.h"
#include "daScript/ast/ast.h"
#include "daScript/daScript.h"
#include "wsl_api_module.hpp"
#include "das_ecs_binds.hpp"
#include "../log/log.hpp"

#include <signal.h>
#include <setjmp.h>

using namespace das;

DECLARE_ALL_DEFAULT_MODULES;

namespace
{

// Custom FsFileAccess that searches project source directories when
// resolving bare `require` names.  Without this, daScript's built-in
// resolution only checks the same directory as the requiring file,
// which breaks cross-directory requires in interpret mode.
class ProjectFsFileAccess : public FsFileAccess
{
public:
  void
  addProjectRoot (const std::string &name, const std::string &path)
  {
    m_project_roots[name] = path;
  }

  ModuleInfo
  getModuleInfo (const string &req, const string &from) const override
  {
    // For bare names (no '.' or '/'), search project directories first.
    if (req.find_first_of ("./") == string::npos) {
      for (const auto &[name, root] : m_project_roots) {
        std::string candidate = root + "/" + req + ".das";
        if (std::filesystem::exists (candidate)) {
          ModuleInfo info;
          info.moduleName = req;
          info.fileName = candidate;
          return info;
        }
      }
    }
    // Fall back to default FsFileAccess resolution.
    return FsFileAccess::getModuleInfo (req, from);
  }

private:
  std::map<std::string, std::string> m_project_roots;
};

bool
initialize_modules_for_engine (TextPrinter &tout,
                               smart_ptr<FsFileAccess> &faccess,
                               ModuleGroup &module_group)
{
  // Ensure the thread-local daScriptEnvironment exists.  Module::Module()
  // accesses daScriptEnvironment::getBound()->modules to insert itself into
  // the per-thread module list.  If the environment hasn't been bound on
  // this thread yet, getBound() returns null and the dereference segfaults.
  // This may be called from a worker thread (async reload), so we must
  // ensure the environment here rather than relying on the main thread.
  ::das::daScriptEnvironment::ensure ();

  // Set up the daslang source tree path and file access.
  setDasRoot (std::string (WEASEL_BUILD_DIR) + "/_deps/daslang-src");

  faccess = smart_ptr<FsFileAccess> (new ProjectFsFileAccess);
  faccess->introduceDaslib ();

  // Add the engine's module directory to ProjectFsFileAccess so that
  // `require weasel_helpers` can find weasel_helpers.das in interpreted mode.
  auto engine_modules
      = std::string (WEASEL_SOURCE_DIR) + "/src/wsl/das/modules";
  static_cast<ProjectFsFileAccess *> (faccess.get ())
      ->addProjectRoot ("engine_modules", engine_modules);

  // Register all default builtin modules (BuiltIn, Math, Strings, etc.)
  // into this thread's local ModuleKarma.  ModuleLibrary::addBuiltInModule()
  // calls Module::require("$") which searches the thread-local module list,
  // so the "$" module MUST be present before any Module_WeaselApi or
  // Module_Ecs constructor runs (they create a ModuleLibrary with
  // addBuiltInModule).
  PULL_ALL_DEFAULT_MODULES;

  // Register the Weasel API module (ECS, transforms, scene queries).
  wsl::das::register_wsl_api_module (module_group);

  // Register the Weasel ECS module (engine component types).
  wsl::das::register_ecs_module (module_group);

  // NOTE: We intentionally do NOT call require_dynamic_modules here.
  // It compiles .das_module files and stores them in thread-local
  // ModuleKarma.  When the worker thread later compiles user .das files,
  // the compiled modules may be in an inconsistent state across threads,
  // causing corruption (hash set corruption in buildAccessFlags, etc.).
  // Instead, modules like builtin.das are compiled on-demand by
  // compileDaScript on the worker thread.

  return true;
}

// Ensure the calling thread has a valid daScriptEnvironment with modules
// and g_modulesInitialized set.  Called from execute_file so that
// compileDaScript does not crash on worker threads (async reload).
//
// This function creates ALL modules (builtin + weasel) on the calling
// thread's thread-local daScriptEnvironment.  Module::Module() inserts
// into daScriptEnvironment::getBound()->modules, which is thread-local.
// If modules were created on a different thread (e.g. main thread during
// initialize()), they would NOT be visible on this worker thread.
void
ensure_thread_das_environment (TextPrinter &tout,
                               smart_ptr<FsFileAccess> &faccess,
                               ModuleGroup &module_group)
{
  auto *env = ::das::daScriptEnvironment::getBound ();
  if (env && env->g_modulesInitialized) {
    return; // Already initialized on this thread.
  }
  // Initialize ALL modules on this thread's thread-local environment.
  // This creates the "$", Math, Strings, etc. builtin modules AND the
  // weasel_api/weasel_ecs modules on THIS thread, so they are visible
  // to Module::require() and compileDaScript on this thread.
  initialize_modules_for_engine (tout, faccess, module_group);
  env = ::das::daScriptEnvironment::getBound ();
  env->g_modulesInitialized = true;
}

} // anonymous namespace

// ── SIGSEGV protection for daScript execution ──
// These live in namespace wsl::das so das_system_adapter.cpp can link to them.

namespace wsl::das::das_signal
{

thread_local sigjmp_buf *tls_jmp = nullptr;
thread_local struct sigaction tls_prev_segv{};
thread_local bool tls_installed = false;
thread_local ::std::vector<char> tls_sigstack;
thread_local bool tls_sigstack_ready = false;

void
ensure_sigstack ()
{
  if (tls_sigstack_ready) {
    return;
  }
  tls_sigstack.resize (SIGSTKSZ);
  stack_t ss{};
  ss.ss_sp = tls_sigstack.data ();
  ss.ss_size = tls_sigstack.size ();
  ss.ss_flags = 0;
  sigaltstack (&ss, nullptr);
  tls_sigstack_ready = true;
}

namespace
{
void
segv_handler (int sig, siginfo_t *info, void *ucontext)
{
  (void)sig;
  (void)info;
  (void)ucontext;
  if (tls_jmp) {
    longjmp (*tls_jmp, 1);
  }
  signal (SIGSEGV, SIG_DFL);
  raise (SIGSEGV);
}
} // anonymous namespace

void
install ()
{
  if (tls_installed) {
    return;
  }
  ensure_sigstack ();
  struct sigaction sa{};
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGSEGV, &sa, &tls_prev_segv);
  tls_installed = true;
}

void
restore ()
{
  if (!tls_installed) {
    return;
  }
  sigaction (SIGSEGV, &tls_prev_segv, nullptr);
  tls_installed = false;
}

} // namespace wsl::das::das_signal

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
  // Per-program context.  Each program gets its own Context because
  // Context::relocateCode relocates ALL sim nodes (including shared module
  // nodes).  If two programs that share modules (via `require`) are
  // simulated into the same Context, the second program's relocateCode
  // tries to copy already-relocated shared nodes, causing the
  // prefix->magic==0xdeadc0de assertion failure in SimNode::copyNode.
  std::unordered_map<std::string, std::unique_ptr<::das::Context>>
      program_contexts;
  struct fn_entry
  {
    ::das::SimFunction *sf = nullptr;
    ::das::Context *ctx = nullptr;
  };
  // Per-file function lookup.  Each compiled file has its own map of
  // function name → (SimFunction*, Context*).  This prevents different
  // systems from overwriting each other's functions (e.g. two files
  // both defining "on_update").
  std::unordered_map<std::string, std::unordered_map<std::string, fn_entry>>
      file_fn_lookup;
  // Legacy flat lookup — kept for backwards compatibility.  Points to the
  // last-registered function with each name.  Only used when callers
  // don't specify a file path.
  std::unordered_map<std::string, fn_entry> fn_lookup;
  // Persistent DebugInfoHelper for making StructInfo after simulate() returns
  // (ctx->thisHelper is cleared to null at the end of simulate).
  ::das::DebugInfoHelper debug_info_helper;

  impl ()
  {
    policies.aot = true;
    policies.aot_module = true;
    policies.fail_on_no_aot = false;
    // Keep all functions in compiled programs so the C++ side can call
    // them at runtime (on_init, on_update, etc.) even if the das code
    // doesn't reference them from within the script.
    policies.export_all = true;
    // compileDaScript asserts that the thread-local daScriptEnvironment
    // has g_modulesInitialized set. Since compileDaScript may be called
    // from worker threads (async reload), skip this check — the modules
    // are globally initialized on the main thread and the ModuleGroup
    // (passed via compileDaScript) has all required modules.
    policies.no_init_check = true;
    // Use v2 syntax for all .das files (class fields, curly braces, etc.)
    policies.version_2_syntax = true;
  }

  bool
  initialize ()
  {
    // Module creation is deferred to ensure_thread_das_environment() which
    // runs on the worker thread.  Module::Module() inserts into the
    // calling thread's thread-local daScriptEnvironment, so modules created
    // on the main thread are invisible to the worker thread's Module::require.
    // We only do minimal global setup here (non-thread-local state).
    ::das::daScriptEnvironment::ensure ();
    setDasRoot (std::string (WEASEL_BUILD_DIR) + "/_deps/daslang-src");
    return true;
  }

  void
  addFsRoot (const std::string &name, const std::string &path)
  {
    ensure_thread_das_environment (tout, faccess, module_group);
    if (auto *proj = static_cast<ProjectFsFileAccess *> (faccess.get ())) {
      proj->addProjectRoot (name, path);
    }
  }

  bool
  execute_file (const std::string &path, std::string &error)
  {
    // Ensure thread-local daScriptEnvironment is set up on this thread,
    // including the module list needed by compileDaScript and Module::require.
    ensure_thread_das_environment (tout, faccess, module_group);

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

    // Each program gets its OWN Context.  Context::relocateCode relocates
    // ALL sim nodes including shared module nodes.  If two programs that
    // share modules (via `require`) are simulated into the same Context,
    // the second program's relocateCode tries to copy already-relocated
    // shared nodes, causing the prefix->magic==0xdeadc0de assertion
    // failure in SimNode::copyNode.
    uint32_t needed = program->getContextStackSize ();
    uint32_t initial = needed > 1024 * 1024 ? needed : 1024 * 1024;
    auto ctx = std::make_unique<::das::Context> (initial);

    if (!program->simulate (*ctx, tout)) {
      std::ostringstream oss;
      for (const auto &err : program->errors) {
        oss << reportError (err.at, err.what, err.extra, err.fixme, err.cerr)
            << "\n";
      }
      error = oss.str ();
      return false;
    }

    // Merge this program's functions into fn_lookup, each paired with its
    // owning context.
    auto *raw_ctx = ctx.get ();
    program_contexts[path] = std::move (ctx);
    compiled_programs[path] = program;

    if (raw_ctx->tabMnLookup) {
      for (auto &kv : *raw_ctx->tabMnLookup) {
        auto *sf = kv.second;
        if (sf && sf->name) {
          file_fn_lookup[path][sf->name] = { sf, raw_ctx };
          fn_lookup[sf->name] = { sf, raw_ctx };
        }
      }
    }

    return true;
  }

  void *
  allocate_instance (const std::string &path, const std::string &struct_name,
                     std::string &error)
  {
    auto ctx_it = program_contexts.find (path);
    if (ctx_it == program_contexts.end ()) {
      error = "No context available for path: " + path;
      return nullptr;
    }
    auto &ctx = ctx_it->second;

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
    char *ptr = ctx->allocate (static_cast<uint64_t> (bytes), nullptr);
    memset (ptr, 0, static_cast<std::size_t> (bytes));
    return ptr;
  }

  das_engine::class_instance
  instantiate_class (const std::string &path, const std::string &class_name,
                     std::string &error)
  {
    auto ctx_it = program_contexts.find (path);
    if (ctx_it == program_contexts.end ()) {
      error = "No context available for path: " + path;
      return {};
    }
    auto &ctx = ctx_it->second;

    auto it = compiled_programs.find (path);
    if (it == compiled_programs.end ()) {
      error = "Program not found for path: " + path;
      return {};
    }
    auto program = it->second;
    if (!program || !program->thisModule) {
      error = "Invalid program";
      return {};
    }

    // Find the class in the compiled program
    auto structs = program->findStructure (class_name.c_str ());
    if (structs.empty ()) {
      error = "Class not found: " + class_name;
      return {};
    }
    auto st = structs[0];
    int bytes = st->getSizeOf ();
    if (bytes <= 0) {
      error = "Class has zero size: " + class_name;
      return {};
    }

    // Allocate zero-initialized memory on the VM heap.
    char *ptr = ctx->allocate (static_cast<uint64_t> (bytes), nullptr);
    memset (ptr, 0, static_cast<std::size_t> (bytes));

    // Find the generated default constructor.
    // The constructor has the same name as the class (no backtick) and
    // debugInfo->count == 0 (zero explicit arguments — the implicit
    // 'this' pointer is not counted).
    SimFunction *ctor_fn = nullptr;
    int32_t nfunc = ctx->getTotalFunctions ();
    for (int32_t i = 0; i < nfunc; ++i) {
      auto *sf = ctx->getFunction (i);
      if (sf && sf->name && class_name == sf->name) {
        if (sf->debugInfo && sf->debugInfo->count == 0) {
          ctor_fn = sf;
          break;
        }
      }
    }

    if (ctor_fn) {
      // Pass the pre-allocated instance as the cmres (copy-mode result)
      // buffer. The constructor writes __rtti, __finalize, virtual-method
      // function pointers, and field defaults directly into ptr.
      ctx->evalWithCatch (ctor_fn, nullptr, ptr);

      const char *ex = ctx->getException ();
      if (ex) {
        error = "Constructor call failed: ";
        error += ex;
        if (ctx->exceptionAt.fileInfo) {
          error += " at ";
          error += ctx->exceptionAt.fileInfo->name;
          error += ":";
          error += std::to_string (ctx->exceptionAt.line);
        }
        ctx->clearException ();
        return {};
      }

      das_engine::class_instance inst;
      inst.ptr = ptr;
      inst.ctx = ctx.get ();
      inst.info = debug_info_helper.makeStructureDebugInfo (*st);
      return inst;
    }

    // No constructor found — fall back to raw zeroed allocation.
    // The instance has zeroed function pointers; the class adapter
    // will silently skip every method call.
    das_engine::class_instance result;
    result.ptr = ptr;
    result.ctx = ctx.get ();
    result.info = debug_info_helper.makeStructureDebugInfo (*st);
    return result;
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
      // Extract default value from the init expression if available.
      // After type inference, init is folded to an ExprConst for literal
      // defaults like `sensitivity : float = 0.1`.
      if (field.init && fi.size > 0) {
        auto *expr_const = static_cast<ExprConst *> (field.init);
        if (expr_const && expr_const->type && !expr_const->type->isRef ()) {
          fi.default_value.resize (static_cast<std::size_t> (fi.size));
          memcpy (fi.default_value.data (), &expr_const->value,
                  static_cast<std::size_t> (fi.size));
        }
      }
      result.fields.push_back (std::move (fi));
    }
    return result;
  }

  bool
  call_void_function (const char *func_name, std::string &error,
                      das_error &das_err)
  {
    auto it = fn_lookup.find (func_name);
    if (it == fn_lookup.end () || !it->second.sf) {
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

    auto &entry = it->second;
    entry.ctx->evalWithCatch (entry.sf, nullptr);

    const char *ex = entry.ctx->getException ();
    if (ex) {
      das_err.message = ex;
      if (entry.ctx->exceptionAt.fileInfo) {
        das_err.file = entry.ctx->exceptionAt.fileInfo->name;
      }
      das_err.line = entry.ctx->exceptionAt.line;
      das_err.column = entry.ctx->exceptionAt.column;
      entry.ctx->clearException ();
      error = das_err.message;
      return false;
    }
    return true;
  }

  bool
  call_void_function (const std::string &path, const char *func_name,
                      std::string &error, das_error &das_err)
  {
    auto file_it = file_fn_lookup.find (path);
    if (file_it == file_fn_lookup.end ()) {
      error = "No compiled functions for path: " + path;
      return false;
    }
    auto it = file_it->second.find (func_name);
    if (it == file_it->second.end () || !it->second.sf) {
      error = "Function '";
      error += func_name;
      error += "' not found in file '";
      error += path;
      error += "'.";
      return false;
    }

    auto &entry = it->second;
    entry.ctx->evalWithCatch (entry.sf, nullptr);

    const char *ex = entry.ctx->getException ();
    if (ex) {
      das_err.message = ex;
      if (entry.ctx->exceptionAt.fileInfo) {
        das_err.file = entry.ctx->exceptionAt.fileInfo->name;
      }
      das_err.line = entry.ctx->exceptionAt.line;
      das_err.column = entry.ctx->exceptionAt.column;
      entry.ctx->clearException ();
      error = das_err.message;
      return false;
    }
    return true;
  }

  bool
  call_int_function (const char *func_name, int &out_value, std::string &error,
                     das_error &das_err)
  {
    auto it = fn_lookup.find (func_name);
    if (it == fn_lookup.end () || !it->second.sf) {
      error = "Function '";
      error += func_name;
      error += "' not found.";
      return false;
    }

    auto &entry = it->second;
    vec4f result = entry.ctx->evalWithCatch (entry.sf, nullptr);

    const char *ex = entry.ctx->getException ();
    if (ex) {
      das_err.message = ex;
      if (entry.ctx->exceptionAt.fileInfo) {
        das_err.file = entry.ctx->exceptionAt.fileInfo->name;
      }
      das_err.line = entry.ctx->exceptionAt.line;
      das_err.column = entry.ctx->exceptionAt.column;
      entry.ctx->clearException ();
      error = das_err.message;
      return false;
    }

    out_value = cast<int32_t>::to (result);
    return true;
  }

  // Safe call wrappers — install a SIGSEGV handler before calling into
  // the VM so that null dereferences, out-of-bounds accesses, and
  // use-after-free crashes are caught and turned into error messages
  // instead of crashing the entire engine.
  //
  // We bypass evalWithCatch (which replaces throwBuf with its own jmp_buf)
  // and call callWithCopyOnReturn directly, setting ctx->throwBuf to our
  // jmp_buf. This way both SIGSEGV (via signal handler) and daScript
  // panic() (via throw_error_at → longjmp) jump to the same recovery point.

  bool
  call_void_function_safe (const char *func_name, std::string &error,
                           das_error &das_err)
  {
    auto it = fn_lookup.find (func_name);
    if (it == fn_lookup.end () || !it->second.sf) {
      error = "Function '";
      error += func_name;
      error += "' not found.";
      return false;
    }

    auto &entry = it->second;
    auto *ctx = entry.ctx;

    // Save context state (same as evalWithCatch does)
    auto aa = ctx->abiArg;
    auto acm = ctx->abiCMRES;
    auto atba = ctx->abiThisBlockArg;
    char *EP, *SP;
    ctx->stack.watermark (EP, SP);
    auto *saved_throwBuf = ctx->throwBuf;

    // Install SIGSEGV handler and set throwBuf to our jmp_buf
    das_signal::install ();
    jmp_buf sj;
    das_signal::tls_jmp = &sj;
    ctx->throwBuf = &sj;

    bool caught = false;
    if (setjmp (sj) == 0) {
      // Normal path — execute the function directly
      ctx->callWithCopyOnReturn (entry.sf, nullptr, nullptr, nullptr);
    } else {
      // Longjmp path — either SIGSEGV (signal handler) or daScript panic()
      caught = true;
    }

    // Restore context state
    ctx->throwBuf = saved_throwBuf;
    das_signal::tls_jmp = nullptr;
    das_signal::restore ();

    if (caught) {
      // Extract error info from the context
      const char *ex = ctx->getException ();
      if (ex) {
        // daScript panic — message already set by throw_fatal_error
        das_err.message = ex;
        if (ctx->exceptionAt.fileInfo) {
          das_err.file = ctx->exceptionAt.fileInfo->name;
        }
        das_err.line = ctx->exceptionAt.line;
        das_err.column = ctx->exceptionAt.column;
        ctx->clearException ();
      } else {
        // SIGSEGV — no daScript exception, just the signal
        das_err.message = "Segmentation fault in daslang script";
      }
      // Restore context state after longjmp (stack/abi may be corrupted)
      ctx->abiArg = aa;
      ctx->abiCMRES = acm;
      ctx->abiThisBlockArg = atba;
      ctx->stack.pop (EP, SP);
      error = das_err.message;
      return false;
    }

    // Check for daScript panics that didn't longjmp (shouldn't happen, but
    // be defensive)
    const char *ex = ctx->getException ();
    if (ex) {
      das_err.message = ex;
      if (ctx->exceptionAt.fileInfo) {
        das_err.file = ctx->exceptionAt.fileInfo->name;
      }
      das_err.line = ctx->exceptionAt.line;
      das_err.column = ctx->exceptionAt.column;
      ctx->clearException ();
      error = das_err.message;
      return false;
    }
    return true;
  }

  bool
  call_void_function_safe (const std::string &path, const char *func_name,
                           std::string &error, das_error &das_err)
  {
    auto file_it = file_fn_lookup.find (path);
    if (file_it == file_fn_lookup.end ()) {
      error = "No compiled functions for path: " + path;
      return false;
    }
    auto it = file_it->second.find (func_name);
    if (it == file_it->second.end () || !it->second.sf) {
      error = "Function '";
      error += func_name;
      error += "' not found in file '";
      error += path;
      error += "'.";
      return false;
    }

    auto &entry = it->second;
    auto *ctx = entry.ctx;

    auto aa = ctx->abiArg;
    auto acm = ctx->abiCMRES;
    auto atba = ctx->abiThisBlockArg;
    char *EP, *SP;
    ctx->stack.watermark (EP, SP);
    auto *saved_throwBuf = ctx->throwBuf;

    das_signal::install ();
    jmp_buf sj;
    das_signal::tls_jmp = &sj;
    ctx->throwBuf = &sj;

    bool caught = false;
    if (setjmp (sj) == 0) {
      ctx->callWithCopyOnReturn (entry.sf, nullptr, nullptr, nullptr);
    } else {
      caught = true;
    }

    ctx->throwBuf = saved_throwBuf;
    das_signal::tls_jmp = nullptr;
    das_signal::restore ();

    if (caught) {
      const char *ex = ctx->getException ();
      if (ex) {
        das_err.message = ex;
        if (ctx->exceptionAt.fileInfo) {
          das_err.file = ctx->exceptionAt.fileInfo->name;
        }
        das_err.line = ctx->exceptionAt.line;
        das_err.column = ctx->exceptionAt.column;
        ctx->clearException ();
      } else {
        das_err.message = "Segmentation fault in daslang script";
      }
      ctx->abiArg = aa;
      ctx->abiCMRES = acm;
      ctx->abiThisBlockArg = atba;
      ctx->stack.pop (EP, SP);
      error = das_err.message;
      return false;
    }

    const char *ex = ctx->getException ();
    if (ex) {
      das_err.message = ex;
      if (ctx->exceptionAt.fileInfo) {
        das_err.file = ctx->exceptionAt.fileInfo->name;
      }
      das_err.line = ctx->exceptionAt.line;
      das_err.column = ctx->exceptionAt.column;
      ctx->clearException ();
      error = das_err.message;
      return false;
    }
    return true;
  }

  void
  shutdown ()
  {
    // Release engine-owned daScript resources (programs, contexts, file
    // access). Do NOT call Module::Shutdown() here — it was never called
    // per-engine instance. Module::Initialize/Shutdown is managed globally on
    // the main thread via initialize_global().
    fn_lookup.clear ();
    file_fn_lookup.clear ();
    program_contexts.clear ();
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

void
wsl::das::das_engine::addFsRoot (const std::string &name,
                                 const std::string &path)
{
  m_impl->addFsRoot (name, path);
}

bool
wsl::das::das_engine::execute_file (const std::filesystem::path &path)
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

  if (m_impl->fn_lookup.empty ()) {
    m_last_error = "No functions loaded";
    return false;
  }

  return m_impl->call_void_function (func_name, m_last_error, m_last_das_error);
}

bool
das_engine::call_void_function (const std::filesystem::path &path,
                                const char *func_name)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  return m_impl->call_void_function (path.string (), func_name, m_last_error,
                                     m_last_das_error);
}

bool
das_engine::call_int_function (const char *func_name, int *out_value)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  if (m_impl->fn_lookup.empty ()) {
    m_last_error = "No functions loaded";
    return false;
  }

  int dummy = 0;
  return m_impl->call_int_function (func_name, out_value ? *out_value : dummy,
                                    m_last_error, m_last_das_error);
}

bool
das_engine::call_void_function_safe (const char *func_name)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  if (m_impl->fn_lookup.empty ()) {
    m_last_error = "No functions loaded";
    return false;
  }

  return m_impl->call_void_function_safe (func_name, m_last_error,
                                          m_last_das_error);
}

bool
das_engine::call_void_function_safe (const std::filesystem::path &path,
                                     const char *func_name)
{
  if (!m_initialized) {
    m_last_error = "Engine not initialized";
    return false;
  }

  return m_impl->call_void_function_safe (path.string (), func_name,
                                          m_last_error, m_last_das_error);
}

das_engine::class_instance
das_engine::instantiate_class (const std::filesystem::path &path,
                               const std::string &class_name,
                               std::string &error)
{
  if (!m_initialized) {
    error = "Engine not initialized";
    return {};
  }

  if (m_impl->program_contexts.empty ()) {
    error = "No compiled programs available";
    return {};
  }

  auto result = m_impl->instantiate_class (path.string (), class_name, error);
  if (!result.ptr) {
    m_last_error = error;
  }
  return result;
}

bool
das_engine::has_class (const std::filesystem::path &path,
                       const std::string &class_name)
{
  if (!m_initialized) {
    return false;
  }
  if (m_impl->program_contexts.empty ()) {
    return false;
  }
  auto it = m_impl->compiled_programs.find (path.string ());
  if (it == m_impl->compiled_programs.end ()) {
    return false;
  }
  auto program = it->second;
  if (!program || !program->thisModule) {
    return false;
  }
  auto structs = program->findStructure (class_name.c_str ());
  return !structs.empty ();
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

bool
das_engine::call_void_function_safe (const char *)
{
  m_last_error = "daslang support not enabled";
  return false;
}

bool
das_engine::call_void_function_safe (const std::filesystem::path &,
                                     const char *)
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
