#include "runtime_project_module.hpp"

#include "../rsc/project.hpp"
#include "runtime_project_module_api.hpp"
#include "../comp/singl/runtime_context.hpp"

#include <cctype>
#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <cstddef>
#include <cstdint>
#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <filesystem>
#include <iterator>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "../log/log.hpp"
#include "clang/Lex/HeaderSearchOptions.h"

namespace wsl
{

namespace reg
{

namespace runtime
{

namespace fs = std::filesystem;

namespace
{

constexpr std::string_view k_generated_module_file
    = "runtime_module.generated.cpp";
constexpr std::string_view k_registration_cache_file
    = "runtime_registration_cache.json";

bool
is_cpp_file (const fs::path &path)
{
  const std::string ext = path.extension ().string ();
  return ext == ".cpp" || ext == ".cc" || ext == ".cxx";
}

bool
is_header_file (const fs::path &path)
{
  const std::string ext = path.extension ().string ();
  return ext == ".hpp" || ext == ".hh" || ext == ".hxx" || ext == ".h";
}

std::optional<std::pair<std::string, std::string>>
load_compile_command ()
{
  const fs::path compile_commands_path
      = fs::path (WEASEL_BUILD_DIR) / "compile_commands.json";

  std::ifstream input (compile_commands_path);
  if (!input) {
    wsl::log::cmake ()->error ("Runtime build could not open {}",
                               compile_commands_path.string ());
    return std::nullopt;
  }

  std::string const content ((std::istreambuf_iterator<char> (input)),
                             std::istreambuf_iterator<char> ());

  rapidjson::Document doc;
  if (doc.Parse (content.c_str ()).HasParseError ()) {
    wsl::log::cmake ()->error ("Runtime build could not parse {}",
                               compile_commands_path.string ());
    return std::nullopt;
  }

  if (!doc.IsArray () || doc.Empty ()) {
    wsl::log::cmake ()->error ("Runtime build: {} is empty or not an array",
                               compile_commands_path.string ());
    return std::nullopt;
  }

  const std::string src_dir = std::string (WEASEL_SOURCE_DIR) + "/src/";
  wsl::log::cmake ()->trace ("Looking for files starting with: {}", src_dir);

  int checked = 0;
  for (const auto &entry : doc.GetArray ()) {
    if (!entry.IsObject () || !entry.HasMember ("file")
        || !entry.HasMember ("directory") || !entry.HasMember ("command")) {
      continue;
    }

    const std::string file = entry["file"].GetString ();
    checked++;
    if (checked <= 3) {
      wsl::log::cmake ()->trace ("Checking file: {}", file);
    }
    if (file.compare (0, src_dir.length (), src_dir) == 0) {
      wsl::log::cmake ()->trace ("Found compile command for: {}", file);
      return std::make_pair (entry["directory"].GetString (),
                             entry["command"].GetString ());
    }
  }
  wsl::log::cmake ()->trace ("Checked {} entries, found none matching",
                             checked);

  wsl::log::cmake ()->error (
      "Runtime build: no suitable compile command found in {}",
      compile_commands_path.string ());
  return std::nullopt;
}

std::optional<std::string>
make_shared_command_base (const std::string &command)
{
  static const std::regex output_re (R"(\s-o\s+\S+)");
  static const std::regex compile_re (R"(\s-c\s+\S+)");
  static const std::regex source_re (R"(\s\S+\.(?:cpp|c|cc|cxx)\s*$)");

  std::string stripped = std::regex_replace (command, output_re, "");
  stripped = std::regex_replace (stripped, compile_re, "");
  stripped = std::regex_replace (stripped, source_re, "");

  if (stripped == command) {
    return std::nullopt;
  }

  return stripped;
}

std::vector<std::string>
split_command_line (const std::string &command)
{
  std::vector<std::string> out;
  std::string current;
  bool escaping = false;
  char quote = '\0';

  for (const char ch : command) {
    if (escaping) {
      current.push_back (ch);
      escaping = false;
      continue;
    }

    if (ch == '\\' && quote != '\'') {
      escaping = true;
      continue;
    }

    if (quote != '\0') {
      if (ch == quote) {
        quote = '\0';
      } else {
        current.push_back (ch);
      }
      continue;
    }

    if (ch == '\'' || ch == '"') {
      quote = ch;
      continue;
    }

    if (std::isspace (static_cast<unsigned char> (ch)) != 0) {
      if (!current.empty ()) {
        out.push_back (std::move (current));
        current.clear ();
      }
      continue;
    }

    current.push_back (ch);
  }

  if (!current.empty ()) {
    out.push_back (std::move (current));
  }

  return out;
}

/*!
 * \brief Returns a pointer to the engine's meta context locator handle.
 *
 * This is exported as a C symbol so that interpreted/JIT code and shared
 * libraries can look it up dynamically rather than hardcoding an absolute
 * address, which breaks when the library is loaded in a different process.
 */
extern "C" void *
wsl_get_meta_ctx_handle ()
{
  static entt::locator<entt::meta_ctx>::node_type handle
      = entt::locator<entt::meta_ctx>::handle ();
  return &handle;
}

void
write_runtime_meta_context_sync (std::ostream &output)
{
  wsl::log::cmake ()->trace ("write_runtime_meta_context_sync");

  output << "extern \"C\" void *wsl_get_meta_ctx_handle ();\n";
  output << "namespace {\n";
  output << "[[maybe_unused]] const bool weasel_runtime_meta_context_synced = "
            "[]() {\n";
  output << "  using node_type = entt::locator<entt::meta_ctx>::node_type;\n";
  output << "  node_type *handle = static_cast<node_type *> (\n";
  output << "      wsl_get_meta_ctx_handle ());\n";
  output << "  entt::locator<entt::meta_ctx>::reset (*handle);\n";
  output << "  return true;\n";
  output << "}();\n";
  output << "} // namespace\n\n";
}

void
clear_runtime_registries (comp::singl::runtime_context &runtime_ctx)
{
  runtime_ctx.component_registry.clear_runtime_components ();
  runtime_ctx.singleton_registry.clear_runtime_singletons (runtime_ctx.world);
  runtime_ctx.system_factory_registry.clear_runtime_systems ();
}

void
write_cached_registration (
    rapidjson::Writer<rapidjson::StringBuffer> &writer,
    const runtime_project_module::cached_registration &registration)
{
  writer.StartObject ();
  writer.Key ("type_id");
  writer.Uint64 (registration.type_id);
  writer.Key ("type_name");
  writer.String (
      registration.type_name.c_str (),
      static_cast<rapidjson::SizeType> (registration.type_name.size ()));
  writer.Key ("display_name");
  writer.String (
      registration.display_name.c_str (),
      static_cast<rapidjson::SizeType> (registration.display_name.size ()));
  writer.EndObject ();
}

template <typename Descriptor>
runtime_project_module::cached_registration
make_cached_registration (const Descriptor &descriptor)
{
  runtime_project_module::cached_registration registration{};
  registration.type_id = descriptor.type_id;
  registration.type_name = descriptor.type_name;
  registration.display_name = descriptor.display_name;
  return registration;
}

bool
read_cached_registration (const rapidjson::Value &value,
                          runtime_project_module::cached_registration &out)
{
  if (!value.IsObject () || !value.HasMember ("type_id")
      || !value.HasMember ("type_name") || !value.HasMember ("display_name")
      || !value["type_id"].IsUint64 () || !value["type_name"].IsString ()
      || !value["display_name"].IsString ()) {
    return false;
  }

  out.type_id = value["type_id"].GetUint64 ();
  out.type_name = value["type_name"].GetString ();
  out.display_name = value["display_name"].GetString ();
  return true;
}

bool
read_cached_registration_array (
    const rapidjson::Document &doc, const char *name,
    std::vector<runtime_project_module::cached_registration> &out)
{
  if (!doc.HasMember (name) || !doc[name].IsArray ()) {
    return false;
  }

  for (const rapidjson::Value &value : doc[name].GetArray ()) {
    runtime_project_module::cached_registration registration{};
    if (!read_cached_registration (value, registration)) {
      return false;
    }
    out.push_back (std::move (registration));
  }

  return true;
}

std::size_t
apply_interpreted_registrations (runtime_module_registration_context &ctx)
{
  std::size_t registered_count = 0;

  for (runtime_registrar::registration_fn fn :
       runtime_registrar::component_registrations ()) {
    if (fn != nullptr) {
      fn (ctx);
      ++registered_count;
    }
  }

  for (runtime_registrar::registration_fn fn :
       runtime_registrar::singleton_registrations ()) {
    if (fn != nullptr) {
      fn (ctx);
      ++registered_count;
    }
  }

  for (runtime_registrar::registration_fn fn :
       runtime_registrar::system_registrations ()) {
    if (fn != nullptr) {
      fn (ctx);
      ++registered_count;
    }
  }

  return registered_count;
}

} // anonymous namespace

// Member function definitions - inside namespace runtime, outside anonymous
// namespace

runtime_project_module::runtime_project_module (
    comp::singl::runtime_context *runtime_ctx)
    : m_runtime_ctx (runtime_ctx)
{
}

runtime_project_module::~runtime_project_module ()
{
  // Ensure we unload interpreted/runtime registrations before destruction to
  // avoid registry destructors invoking std::function targets that reference
  // interpreter-owned memory which may already be freed.
  unload ();
}

void
runtime_project_module::gather_files (const fs::path &base, source_set &out)
{
  if (base.empty () || !fs::exists (base)) {
    return;
  }

  for (const auto &entry : fs::recursive_directory_iterator (base)) {
    if (!entry.is_regular_file ()) {
      continue;
    }

    const fs::path path = fs::weakly_canonical (entry.path ());

    if (is_cpp_file (path)) {
      out.sources.push_back (path);
    } else if (is_header_file (path)) {
      out.headers.push_back (path);
    }
  }

  auto by_path = [] (const fs::path &lhs, const fs::path &rhs) {
    return lhs.generic_string () < rhs.generic_string ();
  };

  std::sort (out.headers.begin (), out.headers.end (), by_path);
  std::sort (out.sources.begin (), out.sources.end (), by_path);
}

std::size_t
runtime_project_module::compute_source_hash (const source_set &sources)
{
  std::size_t seed = 0;

  auto hash_combine = [] (std::size_t s, std::size_t v) {
    return s ^ (v + 0x9e3779b9 + (s << 6) + (s >> 2));
  };

  seed = hash_combine (seed, sources.headers.size ());
  seed = hash_combine (seed, sources.sources.size ());

  for (const fs::path &path : sources.headers) {
    seed
        = hash_combine (seed, std::hash<std::string>{}(path.generic_string ()));
    std::error_code ec;
    auto ftime = fs::last_write_time (path, ec);
    if (!ec) {
      seed = hash_combine (
          seed, static_cast<std::size_t> (ftime.time_since_epoch ().count ()));
    }
  }

  for (const fs::path &path : sources.sources) {
    seed
        = hash_combine (seed, std::hash<std::string>{}(path.generic_string ()));
    std::error_code ec;
    auto ftime = fs::last_write_time (path, ec);
    if (!ec) {
      seed = hash_combine (
          seed, static_cast<std::size_t> (ftime.time_since_epoch ().count ()));
    }
  }

  return seed;
}

fs::path
runtime_project_module::registration_cache_path (const fs::path &project_root)
{
  return project_root / "build" / "weasel_runtime" / k_registration_cache_file;
}

bool
runtime_project_module::read_registration_cache (const fs::path &path,
                                                 std::size_t source_hash,
                                                 registration_cache &out)
{
  std::ifstream input (path);
  if (!input) {
    return false;
  }

  std::string const content ((std::istreambuf_iterator<char> (input)),
                             std::istreambuf_iterator<char> ());
  rapidjson::Document doc;
  if (doc.Parse (content.c_str ()).HasParseError () || !doc.IsObject ()) {
    return false;
  }

  if (!doc.HasMember ("version") || !doc["version"].IsUint ()
      || doc["version"].GetUint () != 1 || !doc.HasMember ("source_hash")
      || !doc["source_hash"].IsUint64 ()) {
    return false;
  }

  if (static_cast<std::size_t> (doc["source_hash"].GetUint64 ())
      != source_hash) {
    return false;
  }

  registration_cache cache{};
  cache.source_hash = source_hash;
  if (!read_cached_registration_array (doc, "components", cache.components)
      || !read_cached_registration_array (doc, "singletons", cache.singletons)
      || !read_cached_registration_array (doc, "systems", cache.systems)) {
    return false;
  }

  out = std::move (cache);
  return true;
}

bool
runtime_project_module::write_registration_cache () const
{
  if (m_runtime_ctx == nullptr || m_loaded_project_root.empty ()
      || m_source_hash == 0) {
    return false;
  }

  registration_cache cache{};
  cache.source_hash = m_source_hash;

  for (const component_registry::descriptor *desc :
       m_runtime_ctx->component_registry.get_world_components (
           world_component_order::type_id)) {
    if (desc != nullptr && desc->runtime_registered) {
      cache.components.push_back (make_cached_registration (*desc));
    }
  }

  for (const singleton_registry::descriptor *desc :
       m_runtime_ctx->singleton_registry.get_singleton_components (
           singleton_component_order::type_id)) {
    if (desc != nullptr && desc->runtime_registered) {
      cache.singletons.push_back (make_cached_registration (*desc));
    }
  }

  for (const system_factory_registry::system_descriptor *desc :
       m_runtime_ctx->system_factory_registry.get_systems (
           system_order::type_id)) {
    if (desc != nullptr && desc->runtime_registered) {
      cache.systems.push_back (make_cached_registration (*desc));
    }
  }

  const fs::path path = registration_cache_path (m_loaded_project_root);
  std::error_code ec;
  fs::create_directories (path.parent_path (), ec);
  if (ec) {
    wsl::log::cmake ()->warn ("Could not create runtime cache directory: {}",
                              ec.message ());
    return false;
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer (buffer);
  writer.StartObject ();
  writer.Key ("version");
  writer.Uint (1);
  writer.Key ("source_hash");
  writer.Uint64 (static_cast<std::uint64_t> (cache.source_hash));

  auto write_array
      = [&writer] (const char *name,
                   const std::vector<cached_registration> &entries) {
          writer.Key (name);
          writer.StartArray ();
          for (const cached_registration &entry : entries) {
            write_cached_registration (writer, entry);
          }
          writer.EndArray ();
        };

  write_array ("components", cache.components);
  write_array ("singletons", cache.singletons);
  write_array ("systems", cache.systems);
  writer.EndObject ();

  // Write atomically: write to a temp file then rename
  const fs::path tmp_path
      = path.parent_path () / (path.filename ().string () + ".tmp");
  {
    std::ofstream out (tmp_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      wsl::log::cmake ()->warn ("Could not write runtime cache temp file: {}",
                                tmp_path.string ());
      return false;
    }
    out << buffer.GetString ();
    out.flush ();
    if (!out.good ()) {
      wsl::log::cmake ()->warn ("Failed writing to runtime cache temp file: {}",
                                tmp_path.string ());
      // attempt to remove temp file
      std::error_code rem_ec;
      fs::remove (tmp_path, rem_ec);
      return false;
    }
  }

  std::error_code rename_ec;
  fs::rename (tmp_path, path, rename_ec);
  if (rename_ec) {
    wsl::log::cmake ()->warn (
        "Could not move runtime cache into place: {} -> {} ({})",
        tmp_path.string (), path.string (), rename_ec.message ());
    // best-effort cleanup
    std::error_code rem_ec;
    fs::remove (tmp_path, rem_ec);
    return false;
  }

  return true;
}

void
runtime_project_module::apply_registration_cache (
    const registration_cache &cache)
{
  clear_runtime_registries (*m_runtime_ctx);

  for (const cached_registration &entry : cache.components) {
    m_runtime_ctx->component_registry.register_cached_runtime_world_component (
        static_cast<entt::id_type> (entry.type_id), entry.type_name,
        entry.display_name);
  }

  for (const cached_registration &entry : cache.singletons) {
    m_runtime_ctx->singleton_registry
        .register_cached_runtime_singleton_component (
            static_cast<entt::id_type> (entry.type_id), entry.type_name,
            entry.display_name);
  }

  for (const cached_registration &entry : cache.systems) {
    m_runtime_ctx->system_factory_registry.register_cached_runtime_system (
        static_cast<entt::id_type> (entry.type_id), entry.type_name,
        entry.display_name);
  }
}

bool
runtime_project_module::write_generated_translation_unit (
    const fs::path &generated_path, const source_set &sources)
{
  std::ofstream output (generated_path);
  if (!output) {
    wsl::log::cmake ()->error ("Failed to write runtime module source: {}",
                               generated_path.string ());
    return false;
  }

  output << "#include \""
         << (fs::path (WEASEL_SOURCE_DIR)
             / "src/wsl/reg/runtime_project_module_api.hpp")
                .generic_string ()
         << "\"\n";
  output << "#include <cstdint>\n\n";
  write_runtime_meta_context_sync (output);

  for (const fs::path &header : sources.headers) {
    output << "#include \"" << header.generic_string () << "\"\n";
  }

  output << '\n';
  for (const fs::path &source : sources.sources) {
    output << "#include \"" << source.generic_string () << "\"\n";
  }

  return true;
}

bool
runtime_project_module::initialize_interpreter ()
{
  static bool llvm_initialized = false;
  if (!llvm_initialized) {
    ::llvm::InitializeNativeTarget ();
    ::llvm::InitializeNativeTargetAsmPrinter ();
    ::llvm::InitializeNativeTargetAsmParser ();
    llvm_initialized = true;
  }

  const auto compile_command = load_compile_command ();
  if (!compile_command) {
    return false;
  }

  const auto command_base = make_shared_command_base (compile_command->second);
  if (!command_base) {
    wsl::log::cmake ()->error (
        "Runtime interpretation could not derive compiler "
        "arguments from compile_commands.json");
    return false;
  }

  std::vector<std::string> all_args = split_command_line (*command_base);
  if (all_args.empty ()) {
    wsl::log::cmake ()->error (
        "Runtime interpretation could not parse compiler arguments");
    return false;
  }

  m_interpreter_args_storage.clear ();
  if (all_args.size () > 1) {
    m_interpreter_args_storage.insert (m_interpreter_args_storage.end (),
                                       all_args.begin () + 1, all_args.end ());
  }

  // Always add entt include path as separate arguments
  m_interpreter_args_storage.push_back ("-isystem");
  m_interpreter_args_storage.push_back (std::string (WEASEL_BUILD_DIR)
                                        + "/_deps/entt-src/src");

#ifdef WEASEL_CLANG_RESOURCE_DIR
  m_interpreter_args_storage.push_back ("-resource-dir");
  m_interpreter_args_storage.push_back (WEASEL_CLANG_RESOURCE_DIR);
#endif

  m_interpreter_args.clear ();
  m_interpreter_args.reserve (m_interpreter_args_storage.size ());
  for (const std::string &arg : m_interpreter_args_storage) {
    m_interpreter_args.push_back (arg.c_str ());
  }

  std::string args_str;
  for (const auto &a : m_interpreter_args_storage) {
    args_str += a + " ";
  }
  wsl::log::cmake ()->trace ("Compiler args: {}", args_str);

  // Check if entt is in the args
  bool has_entt = false;
  for (size_t i = 0; i < m_interpreter_args_storage.size (); ++i) {
    if (m_interpreter_args_storage[i] == "-isystem"
        && i + 1 < m_interpreter_args_storage.size ()
        && m_interpreter_args_storage[i + 1].find ("entt")
               != std::string::npos) {
      has_entt = true;
      break;
    }
    if (m_interpreter_args_storage[i].find ("entt") != std::string::npos) {
      has_entt = true;
      break;
    }
  }
  wsl::log::cmake ()->trace ("entt in args: {}", has_entt);

  ::clang::IncrementalCompilerBuilder builder;
  builder.SetCompilerArgs (m_interpreter_args);

  auto ci = builder.CreateCpp ();

  // Check for fatal errors before proceeding
  if (ci && *ci && (*ci)->getDiagnostics ().hasFatalErrorOccurred ()) {
    wsl::log::cmake ()->error (
        "Fatal error occurred during compiler initialization");
    return false;
  }
  if (!ci) {
    wsl::log::cmake ()->error ("Failed to create IncrementalCompiler: {}",
                               ::llvm::toString (ci.takeError ()));
    return false;
  }

  if (!*ci || (*ci)->getDiagnostics ().hasErrorOccurred ()) {
    wsl::log::cmake ()->error (
        "Failed to initialize compiler instance (diagnostics errors occurred)");
    return false;
  }

  auto expected_interpreter = ::clang::Interpreter::create (std::move (*ci));
  if (!expected_interpreter) {
    wsl::log::cmake ()->error (
        "Failed to create Interpreter: {}",
        ::llvm::toString (expected_interpreter.takeError ()));
    return false;
  }
  m_interpreter = std::move (*expected_interpreter);

  const std::string weasel_src_include
      = (fs::path (WEASEL_SOURCE_DIR) / "src").string ();

  auto &ci_ref = *m_interpreter->getCompilerInstance ();
  auto &header_search_opts = ci_ref.getHeaderSearchOpts ();

  header_search_opts.AddPath (WEASEL_SOURCE_DIR, ::clang::frontend::Angled,
                              false, false);
  header_search_opts.AddPath (weasel_src_include, ::clang::frontend::Angled,
                              false, false);
  header_search_opts.AddPath (WEASEL_BUILD_DIR, ::clang::frontend::Angled,
                              false, false);

  const fs::path entt_include
      = fs::path (WEASEL_BUILD_DIR) / "_deps" / "entt-src" / "src";
  header_search_opts.AddPath (entt_include.string (), ::clang::frontend::Angled,
                              false, false);

  if (!m_loaded_project_root.empty ()) {
    header_search_opts.AddPath (m_loaded_project_root.string (),
                                ::clang::frontend::Angled, false, false);
  }

  return true;
}

bool
runtime_project_module::interpret (const fs::path &generated_path)
{
  wsl::log::cmake ()->trace ("Clearing previous registrations");
  runtime_registrar::component_registrations ().clear ();
  runtime_registrar::singleton_registrations ().clear ();
  runtime_registrar::system_registrations ().clear ();

  wsl::log::cmake ()->trace ("Initializing interpreter");
  if (!initialize_interpreter ()) {
    wsl::log::cmake ()->error ("Failed to initialize interpreter");
    return false;
  }

  wsl::log::cmake ()->trace ("Loading file {}", generated_path.string ());

  std::ifstream file (generated_path);
  std::string const code ((std::istreambuf_iterator<char> (file)),
                          std::istreambuf_iterator<char> ());

  wsl::log::cmake ()->trace ("Generated code content:\n{}", code);

  auto error = m_interpreter->ParseAndExecute (code);
  if (error) {
    wsl::log::cmake ()->error ("Clang Interpreter failed to interpret {}",
                               generated_path.string ());
    llvm::consumeError (std::move (error));
    return false;
  }

  return true;
}

bool
runtime_project_module::load_cached_metadata (const rsc::project &project)
{
  if (m_module_loaded || m_metadata_cache_loaded) {
    m_last_status = "Runtime metadata is already loaded.";
    return true;
  }

  if (m_runtime_ctx == nullptr) {
    m_last_status = "Runtime metadata cache has no runtime context.";
    return false;
  }

  const fs::path project_root = fs::weakly_canonical (project.root_path);

  source_set sources;
  gather_files (project_root / project.components_path, sources);
  gather_files (project_root / project.systems_path, sources);
  gather_files (project_root / project.singletons_path, sources);

  const std::size_t current_hash = compute_source_hash (sources);
  registration_cache cache{};
  if (!read_registration_cache (registration_cache_path (project_root),
                                current_hash, cache)) {
    m_last_status = "Runtime metadata cache is missing or stale.";
    return false;
  }

  apply_registration_cache (cache);
  m_loaded_project_root = project_root;
  m_source_hash = current_hash;
  m_metadata_cache_loaded = true;
  m_last_status = "Runtime metadata loaded from cache.";
  wsl::log::cmake ()->debug ("{}", m_last_status);
  return true;
}

void
runtime_project_module::finalize_load ()
{
  wsl::log::cmake ()->debug ("finalize_load: Clearing runtime registries");
  clear_runtime_registries (*m_runtime_ctx);
  runtime_registrar::set_active_runtime_context (m_runtime_ctx);

  runtime_module_registration_context ctx{
    m_runtime_ctx->component_registry,
    m_runtime_ctx->singleton_registry,
    m_runtime_ctx->system_factory_registry,
  };

  wsl::log::cmake ()->debug (
      "finalize_load: Applying interpreted registrations");
  const std::size_t registered_count = apply_interpreted_registrations (ctx);

  wsl::log::cmake ()->debug ("finalize_load: Registered {} interpreted entries",
                             registered_count);
  m_metadata_cache_loaded = false;
  if (write_registration_cache ()) {
    wsl::log::cmake ()->debug ("finalize_load: Registration cache updated.");
  }
  wsl::log::cmake ()->debug ("finalize_load: Done");
}

void
runtime_project_module::unload ()
{
  if (!m_module_loaded && !m_metadata_cache_loaded) {
    return;
  }

  wsl::log::cmake ()->trace ("Unloading module");

  // Clear runtime registries and pending interpreted registrations before
  // destroying the interpreter. Some registration targets (std::function,
  // entt meta descriptors, etc.) may require interpreter-owned code to run
  // their destructors; clearing them while the interpreter is still alive
  // avoids calling into freed code during shutdown.
  if (m_runtime_ctx != nullptr) {
    clear_runtime_registries (*m_runtime_ctx);
  }

  // Clear any pending interpreted registrations
  runtime_registrar::component_registrations ().clear ();
  runtime_registrar::singleton_registrations ().clear ();
  runtime_registrar::system_registrations ().clear ();

  // Unbind active runtime context
  runtime_registrar::set_active_runtime_context (nullptr);

  // Now destroy the interpreter and any Clang/LLVM-owned resources.
  m_interpreter.reset ();

  m_module_loaded = false;
  m_metadata_cache_loaded = false;
  m_loaded_project_root.clear ();
  m_source_hash = 0;
  m_last_status = "Module unloaded.";
}

bool
runtime_project_module::compile_and_load (const rsc::project &project)
{
  const fs::path project_root = fs::weakly_canonical (project.root_path);

  wsl::log::cmake ()->trace ("Compile and load started for project root: {}",
                             project_root.string ());

  source_set sources;
  wsl::log::cmake ()->trace (
      "Gathering files from components, systems and singletons paths...");
  wsl::log::cmake ()->trace (
      "  components_path: {}",
      (project_root / project.components_path).string ());
  wsl::log::cmake ()->trace ("  systems_path: {}",
                             (project_root / project.systems_path).string ());
  wsl::log::cmake ()->trace (
      "  singletons_path: {}",
      (project_root / project.singletons_path).string ());
  gather_files (project_root / project.components_path, sources);
  gather_files (project_root / project.systems_path, sources);
  gather_files (project_root / project.singletons_path, sources);
  wsl::log::cmake ()->trace ("Gathered {} headers and {} sources",
                             sources.headers.size (), sources.sources.size ());

  const std::size_t current_hash = compute_source_hash (sources);

  if (m_module_loaded && current_hash == m_source_hash && m_interpreter) {
    m_loaded_project_root = project_root;
    m_last_status
        = "Runtime module is already up to date (no changes detected).";
    wsl::log::cmake ()->trace ("{}", m_last_status);
    return true;
  }

  if (m_metadata_cache_loaded) {
    clear_runtime_registries (*m_runtime_ctx);
    m_metadata_cache_loaded = false;
  }

  m_loaded_project_root = project_root;

  // Try loading from shared library cache (fast path)
  if (try_load_cached_shared_library (current_hash)) {
    m_module_loaded = true;
    m_metadata_cache_loaded = false;
    m_source_hash = current_hash;
    m_last_status = "Runtime module loaded from shared library cache.";
    wsl::log::cmake ()->debug ("{}", m_last_status);
    return true;
  }

  // Cache miss — full Clang-REPL interpretation
  const fs::path build_dir = project_root / "build" / "weasel_runtime";
  fs::create_directories (build_dir);

  const fs::path generated_path = build_dir / k_generated_module_file;

  wsl::log::cmake ()->debug ("Writing generated translation unit to: {}",
                             generated_path.string ());
  if (!write_generated_translation_unit (generated_path, sources)) {
    m_last_status = "Failed to generate the runtime module source file.";
    wsl::log::cmake ()->error ("{}", m_last_status);
    return false;
  }

  wsl::log::cmake ()->trace ("Starting runtime interpretation...");
  if (!interpret (generated_path)) {
    m_last_status
        = "Runtime interpretation failed. Check the console for details.";
    wsl::log::cmake ()->error ("{}", m_last_status);
    return false;
  }

  m_module_loaded = true;
  m_metadata_cache_loaded = false;
  m_source_hash = current_hash;

  // Compile to shared library for next time
  const fs::path so_path = shared_library_path (project_root);
  if (compile_to_shared_library (generated_path, so_path)) {
    write_source_hash (source_hash_path (project_root), current_hash);
    wsl::log::cmake ()->debug ("Shared library cache updated.");
  }

  m_last_status = "Runtime systems/components interpreted and registered.";
  wsl::log::cmake ()->trace ("{}", m_last_status);
  return true;
}

// ── Async reload ──

void
runtime_project_module::compile_and_load_async (const rsc::project &project)
{
  // If a previous async reload already finished but hasn't been polled yet,
  // finalize it now so we don't lose the result before starting a new one.
  if (m_async_reload_future.valid ()
      && m_async_reload_future.wait_for (std::chrono::seconds (0))
             == std::future_status::ready) {
    poll_async_reload ();
  }

  if (is_reloading ()) {
    wsl::log::cmake ()->warn (
        "Async reload already in progress, ignoring duplicate request");
    return;
  }

  wsl::log::cmake ()->debug ("Starting async runtime reload for project: {}",
                             project.name);
  m_async_reload_future = std::async (std::launch::async, [this, project] () {
    return this->compile_and_load (project);
  });
}

bool
runtime_project_module::poll_async_reload ()
{
  if (!m_async_reload_future.valid ()) {
    return false;
  }

  if (m_async_reload_future.wait_for (std::chrono::seconds (0))
      == std::future_status::ready) {
    const bool success = m_async_reload_future.get ();
    if (success) {
      wsl::log::cmake ()->debug (
          "Async reload complete, calling finalize_load on main thread");
      finalize_load ();
    } else {
      wsl::log::cmake ()->warn ("Async reload failed: {}", m_last_status);
    }
    return true;
  }
  return false;
}

bool
runtime_project_module::is_reloading () const
{
  if (!m_async_reload_future.valid ()) {
    return false;
  }
  return m_async_reload_future.wait_for (std::chrono::seconds (0))
         == std::future_status::timeout;
}

// ── Shared library cache ──

fs::path
runtime_project_module::shared_library_path (const fs::path &project_root)
{
#if defined(_WIN32)
  constexpr std::string_view lib_name = "runtime_module_cached.dll";
#elif defined(__APPLE__)
  constexpr std::string_view lib_name = "runtime_module_cached.dylib";
#else
  constexpr std::string_view lib_name = "runtime_module_cached.so";
#endif
  return project_root / "build" / "weasel_runtime" / lib_name;
}

fs::path
runtime_project_module::source_hash_path (const fs::path &project_root)
{
  return project_root / "build" / "weasel_runtime"
         / "runtime_module_cached.hash";
}

bool
runtime_project_module::read_source_hash (const fs::path &path,
                                          std::size_t &out_hash) const
{
  std::ifstream input (path);
  if (!input) {
    return false;
  }
  input >> out_hash;
  return !input.fail ();
}

void
runtime_project_module::write_source_hash (const fs::path &path,
                                           std::size_t hash) const
{
  std::ofstream output (path, std::ios::trunc);
  if (output) {
    output << hash;
  }
}

bool
runtime_project_module::compile_to_shared_library (
    const fs::path &generated_path, const fs::path &output_path)
{
  // Derive compiler from the build system
  std::string compiler;
#if defined(WEASEL_CXX_COMPILER)
  compiler = WEASEL_CXX_COMPILER;
#elif defined(_MSC_VER)
  compiler = "cl";
#elif defined(__clang__)
  compiler = "clang++";
#elif defined(__GNUC__)
  compiler = "g++";
#else
  compiler = "c++";
#endif

  // Build the compilation command
  std::ostringstream cmd;
  cmd << "\"" << compiler << "\" ";

  // Add the same include paths and flags the interpreter uses.
  // Skip -resource-dir because the standalone compiler should use its own
  // builtin headers; forcing the interpreter's resource dir can cause
  // mismatches (e.g. LLVM 20 headers with an LLVM 21 compiler).
  bool skip_next = false;
  for (const auto &arg : m_interpreter_args_storage) {
    if (skip_next) {
      skip_next = false;
      continue;
    }
    if (arg == "-resource-dir") {
      skip_next = true;
      continue;
    }
    cmd << "\"" << arg << "\" ";
  }

  // Platform-specific shared library flags
#if defined(_MSC_VER)
  cmd << "/LD /EHsc /MD ";
#elif defined(__APPLE__)
  cmd << "-shared -fPIC ";
#else
  cmd << "-shared -fPIC ";
#endif

  // Input and output
  cmd << "\"" << generated_path.generic_string () << "\" ";
#if defined(_MSC_VER)
  cmd << "/Fe:" << output_path.generic_string ();
#else
  cmd << "-o \"" << output_path.generic_string () << "\"";
#endif

  wsl::log::cmake ()->debug ("Compiling shared library: {}", cmd.str ());

  int result = std::system (cmd.str ().c_str ());
  if (result != 0) {
    wsl::log::cmake ()->warn (
        "Shared library compilation failed (exit code {}). "
        "Falling back to interpretation.",
        result);
    return false;
  }

  wsl::log::cmake ()->debug ("Shared library compiled: {}",
                             output_path.string ());
  return true;
}

bool
runtime_project_module::try_load_cached_shared_library (
    std::size_t current_hash)
{
  if (m_runtime_ctx == nullptr || m_loaded_project_root.empty ()) {
    return false;
  }

  const fs::path so_path = shared_library_path (m_loaded_project_root);
  const fs::path hash_path = source_hash_path (m_loaded_project_root);

  std::size_t cached_hash = 0;
  if (!read_source_hash (hash_path, cached_hash)
      || cached_hash != current_hash) {
    wsl::log::cmake ()->trace (
        "Shared library cache miss (hash mismatch or missing)");
    return false;
  }

  if (!fs::exists (so_path)) {
    wsl::log::cmake ()->trace ("Shared library cache miss (file missing)");
    return false;
  }

  // Ensure interpreter is initialized so LoadDynamicLibrary works
  if (!m_interpreter) {
    if (!initialize_interpreter ()) {
      wsl::log::cmake ()->warn (
          "Failed to initialize interpreter for shared library load");
      return false;
    }
  }

  // Clear any previous registrations
  runtime_registrar::component_registrations ().clear ();
  runtime_registrar::singleton_registrations ().clear ();
  runtime_registrar::system_registrations ().clear ();

  wsl::log::cmake ()->debug ("Loading cached shared library: {}",
                             so_path.string ());
  auto error = m_interpreter->LoadDynamicLibrary (so_path.string ().c_str ());
  if (error) {
    wsl::log::cmake ()->warn ("Failed to load cached shared library: {}",
                              toString (std::move (error)));
    return false;
  }

  wsl::log::cmake ()->debug (
      "Cached shared library loaded. Buckets: comp={}, singl={}, sys={}",
      runtime_registrar::component_registrations ().size (),
      runtime_registrar::singleton_registrations ().size (),
      runtime_registrar::system_registrations ().size ());
  return true;
}

} // namespace runtime
} // namespace reg
} // namespace wsl
