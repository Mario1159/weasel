#include "runtime_project_module.hpp"
#include "dynamic_library.hpp"
#include "../das/das_engine.hpp"
#include "../das/das_registration.hpp"

#include "../rsc/project.hpp"
#include "runtime_project_module_api.hpp"
#include "../comp/singl/runtime_context.hpp"

#include <cctype>
#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>
#include <cstddef>
#include <cstdint>
#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <filesystem>
#include <iterator>

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

/**
 * Returns a pointer to the engine's meta context locator handle.
 *
 * This is exported as a C symbol so that compiled shared libraries can
 * look it up dynamically rather than hardcoding an absolute address.
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
  runtime_ctx.component_registry ().clear_runtime_components ();
  runtime_ctx.singleton_registry ().clear_runtime_singletons (
      runtime_ctx.world ());
  runtime_ctx.system_factory_registry ().clear_runtime_systems ();
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
  writer.Key ("is_das_component");
  writer.Bool (registration.is_das_component);
  writer.Key ("das_struct_size");
  writer.Int (registration.das_struct_size);
  writer.Key ("das_fields");
  writer.StartArray ();
  for (const auto &f : registration.das_fields) {
    writer.StartObject ();
    writer.Key ("name");
    writer.String (f.name.c_str (),
                   static_cast<rapidjson::SizeType> (f.name.size ()));
    writer.Key ("type_name");
    writer.String (f.type_name.c_str (),
                   static_cast<rapidjson::SizeType> (f.type_name.size ()));
    writer.Key ("offset");
    writer.Int (f.offset);
    writer.Key ("size");
    writer.Int (f.size);
    writer.Key ("kind");
    writer.Int (f.kind);
    writer.EndObject ();
  }
  writer.EndArray ();
  writer.EndObject ();
}

runtime_project_module::cached_registration
make_cached_registration (const component_registry::descriptor &descriptor)
{
  runtime_project_module::cached_registration registration{};
  registration.type_id = descriptor.type_id;
  registration.type_name = descriptor.type_name;
  registration.display_name = descriptor.display_name;
  registration.is_das_component = descriptor.is_das_component;
  registration.das_struct_size = descriptor.das_struct_size;
  for (const auto &f : descriptor.das_fields) {
    registration.das_fields.push_back (
        { f.name, f.type_name, f.offset, f.size, static_cast<int> (f.kind) });
  }
  return registration;
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

  // Optional das fields (backward-compatible with old caches)
  out.is_das_component = value.HasMember ("is_das_component")
                                 && value["is_das_component"].IsBool ()
                             ? value["is_das_component"].GetBool ()
                             : false;
  out.das_struct_size
      = value.HasMember ("das_struct_size") && value["das_struct_size"].IsInt ()
            ? value["das_struct_size"].GetInt ()
            : 0;
  if (value.HasMember ("das_fields") && value["das_fields"].IsArray ()) {
    for (const auto &f : value["das_fields"].GetArray ()) {
      if (!f.IsObject () || !f.HasMember ("name") || !f["name"].IsString ()
          || !f.HasMember ("type_name") || !f["type_name"].IsString ()
          || !f.HasMember ("offset") || !f["offset"].IsInt ()
          || !f.HasMember ("size") || !f["size"].IsInt ()
          || !f.HasMember ("kind") || !f["kind"].IsInt ()) {
        continue;
      }
      out.das_fields.push_back (
          { f["name"].GetString (), f["type_name"].GetString (),
            f["offset"].GetInt (), f["size"].GetInt (), f["kind"].GetInt () });
    }
  }
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

// Member function definitions

bool
runtime_project_module::is_das_file (const fs::path &path)
{
  return path.extension () == ".das";
}

runtime_project_module::runtime_project_module (
    comp::singl::runtime_context *runtime_ctx)
    : m_runtime_ctx (runtime_ctx)
{
}

runtime_project_module::~runtime_project_module ()
{
  // Ensure we unload runtime registrations before destruction to
  // avoid registry destructors invoking std::function targets that reference
  // shared library code which may already be freed.
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
      out.cpp_sources.push_back (path);
    } else if (is_header_file (path)) {
      out.headers.push_back (path);
    } else if (is_das_file (path)) {
      out.das_sources.push_back (path);
    }
  }

  auto by_path = [] (const fs::path &lhs, const fs::path &rhs) {
    return lhs.generic_string () < rhs.generic_string ();
  };

  std::sort (out.headers.begin (), out.headers.end (), by_path);
  std::sort (out.cpp_sources.begin (), out.cpp_sources.end (), by_path);
  std::sort (out.das_sources.begin (), out.das_sources.end (), by_path);
}

std::size_t
runtime_project_module::compute_source_hash (const source_set &sources)
{
  std::size_t seed = 0;

  auto hash_combine = [] (std::size_t s, std::size_t v) {
    return s ^ (v + 0x9e3779b9 + (s << 6) + (s >> 2));
  };

  seed = hash_combine (seed, sources.headers.size ());
  seed = hash_combine (seed, sources.cpp_sources.size ());
  seed = hash_combine (seed, sources.das_sources.size ());

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

  for (const fs::path &path : sources.cpp_sources) {
    seed
        = hash_combine (seed, std::hash<std::string>{}(path.generic_string ()));
    std::error_code ec;
    auto ftime = fs::last_write_time (path, ec);
    if (!ec) {
      seed = hash_combine (
          seed, static_cast<std::size_t> (ftime.time_since_epoch ().count ()));
    }
  }

  for (const fs::path &path : sources.das_sources) {
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
       m_runtime_ctx->component_registry ().get_world_components (
           world_component_order::type_id)) {
    if (desc != nullptr && desc->runtime_registered) {
      cache.components.push_back (make_cached_registration (*desc));
    }
  }

  for (const singleton_registry::descriptor *desc :
       m_runtime_ctx->singleton_registry ().get_singleton_components (
           singleton_component_order::type_id)) {
    if (desc != nullptr && desc->runtime_registered) {
      cache.singletons.push_back (make_cached_registration (*desc));
    }
  }

  for (const system_factory_registry::system_descriptor *desc :
       m_runtime_ctx->system_factory_registry ().get_systems (
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
    std::vector<component_registry::descriptor::das_field> fields;
    for (const auto &f : entry.das_fields) {
      fields.push_back (
          { f.name, f.type_name, f.offset, f.size,
            static_cast<wsl::das::das_engine::field_type_kind> (f.kind) });
    }
    m_runtime_ctx->component_registry ()
        .register_cached_runtime_world_component (
            static_cast<entt::id_type> (entry.type_id), entry.type_name,
            entry.display_name,
            entry.is_das_component ? entry.das_struct_size : 0,
            std::move (fields));

    // Also register in the lookup table for generic dispatch.
    if (entry.is_das_component) {
      m_runtime_ctx->component_registry ().register_component_type_info (
          entry.type_name, static_cast<uint64_t> (entry.type_id),
          reg::ComponentKind::DAS_SCRIPT,
          static_cast<size_t> (entry.das_struct_size));
      // Also register with the full qualified name that typeinfo typename
      // returns Format: "module_name::TypeName const" (e.g.,
      // "mouse_rotate::MouseRotate const")
      std::string qualified_name
          = entry.type_name + "::" + entry.type_name + " const";
      // Convert stem to PascalCase for the type name part
      std::string pascal_name;
      bool capitalize_next = true;
      for (char ch : entry.type_name) {
        if (ch == '_' || ch == '-') {
          capitalize_next = true;
        } else if (capitalize_next) {
          auto upper = static_cast<char> (
              std::toupper (static_cast<unsigned char> (ch)));
          pascal_name += upper;
          capitalize_next = false;
        } else {
          pascal_name += ch;
        }
      }
      qualified_name = entry.type_name + "::" + pascal_name + " const";
      m_runtime_ctx->component_registry ().register_component_type_info (
          qualified_name, static_cast<uint64_t> (entry.type_id),
          reg::ComponentKind::DAS_SCRIPT,
          static_cast<size_t> (entry.das_struct_size));
    } else {
      m_runtime_ctx->component_registry ().register_component_type_info (
          entry.type_name, static_cast<uint64_t> (entry.type_id),
          reg::ComponentKind::CPP_PROJECT,
          static_cast<size_t> (entry.das_struct_size));
    }
  }

  for (const cached_registration &entry : cache.singletons) {
    m_runtime_ctx->singleton_registry ()
        .register_cached_runtime_singleton_component (
            static_cast<entt::id_type> (entry.type_id), entry.type_name,
            entry.display_name);
  }

  for (const cached_registration &entry : cache.systems) {
    m_runtime_ctx->system_factory_registry ().register_cached_runtime_system (
        static_cast<entt::id_type> (entry.type_id), entry.type_name,
        entry.display_name);
  }
}

void
runtime_project_module::load_das_registrations_from_cache (
    const registration_cache &cache)
{
  m_das_registrations.clear ();

  auto convert_fields = [] (const std::vector<cached_das_field> &cached)
      -> std::vector<wsl::das::das_engine::field_info> {
    std::vector<wsl::das::das_engine::field_info> fields;
    fields.reserve (cached.size ());
    for (const auto &f : cached) {
      fields.push_back (
          { f.name, f.type_name, f.offset, f.size,
            static_cast<wsl::das::das_engine::field_type_kind> (f.kind) });
    }
    return fields;
  };

  for (const cached_registration &entry : cache.components) {
    if (entry.is_das_component) {
      m_das_registrations.push_back ({
          das_registration::component,
          entry.type_name,
          entry.display_name,
          entry.type_id,
          entry.das_struct_size,
          {},
          convert_fields (entry.das_fields),
      });
    }
  }

  for (const cached_registration &entry : cache.singletons) {
    if (entry.is_das_component) {
      m_das_registrations.push_back ({
          das_registration::singleton,
          entry.type_name,
          entry.display_name,
          entry.type_id,
          entry.das_struct_size,
          {},
          convert_fields (entry.das_fields),
      });
    }
  }

  for (const cached_registration &entry : cache.systems) {
    m_das_registrations.push_back ({
        das_registration::system,
        entry.type_name,
        entry.display_name,
        entry.type_id,
        entry.das_struct_size,
        {},
        convert_fields (entry.das_fields),
    });
  }

  wsl::log::cmake ()->debug (
      "load_das_registrations_from_cache: Restored {} das registrations",
      m_das_registrations.size ());
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
  output << "#include \""
         << (fs::path (WEASEL_SOURCE_DIR) / "src/wsl/user_hooks.hpp")
                .generic_string ()
         << "\"\n";
  output << "#include <cstdint>\n\n";
  write_runtime_meta_context_sync (output);

  for (const fs::path &header : sources.headers) {
    output << "#include \"" << header.generic_string () << "\"\n";
  }

  output << '\n';
  for (const fs::path &source : sources.cpp_sources) {
    output << "#include \"" << source.generic_string () << "\"\n";
  }

  // Provide weak default implementations so linking never fails.
  // User code can override these by defining them in their .cpp files.
  output << R"(
extern "C" {
__attribute__((weak)) void wsl_on_project_init(wsl::app&) {}
__attribute__((weak)) void wsl_on_project_update(wsl::app&, double) {}
__attribute__((weak)) void wsl_on_project_shutdown(wsl::app&) {}
}
)";

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
  gather_files (project_root / "src", sources);

  const std::size_t current_hash = compute_source_hash (sources);
  registration_cache cache{};
  if (!read_registration_cache (registration_cache_path (project_root),
                                current_hash, cache)) {
    m_last_status = "Runtime metadata cache is missing or stale.";
    return false;
  }

  apply_registration_cache (cache);
  load_das_registrations_from_cache (cache);
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
  clear_runtime_registries (*m_runtime_ctx);
  runtime_registrar::set_active_runtime_context (m_runtime_ctx);

  runtime_module_registration_context ctx{
    m_runtime_ctx->component_registry (),
    m_runtime_ctx->singleton_registry (),
    m_runtime_ctx->system_factory_registry (),
  };

  const std::size_t registered_count = apply_interpreted_registrations (ctx);

  // Re-apply stored daslang registrations
  std::size_t das_count = 0;
  for (const auto &reg : m_das_registrations) {
    // Convert field_info to descriptor::das_field
    std::vector<wsl::reg::component_registry::descriptor::das_field> fields;
    for (const auto &fi : reg.fields) {
      fields.push_back ({ fi.name, fi.type_name, fi.offset, fi.size, fi.kind,
                          fi.default_value });
    }

    switch (reg.kind) {
    case das_registration::component:
      m_runtime_ctx->component_registry ()
          .register_cached_runtime_world_component (
              static_cast<entt::id_type> (reg.type_id), reg.type_name,
              reg.display_name, reg.struct_size, std::move (fields));

      // Also register in the lookup table for generic dispatch
      m_runtime_ctx->component_registry ().register_component_type_info (
          reg.type_name, static_cast<uint64_t> (reg.type_id),
          reg::ComponentKind::DAS_SCRIPT,
          static_cast<size_t> (reg.struct_size));
      // Also register with the full qualified name that typeinfo typename
      // returns (e.g., "mouse_rotate::MouseRotate const")
      {
        std::string pascal_name;
        bool capitalize_next = true;
        for (char ch : reg.type_name) {
          if (ch == '_' || ch == '-') {
            capitalize_next = true;
          } else if (capitalize_next) {
            auto upper = static_cast<char> (
                std::toupper (static_cast<unsigned char> (ch)));
            pascal_name += upper;
            capitalize_next = false;
          } else {
            pascal_name += ch;
          }
        }
        std::string qualified_name
            = reg.type_name + "::" + pascal_name + " const";
        m_runtime_ctx->component_registry ().register_component_type_info (
            qualified_name, static_cast<uint64_t> (reg.type_id),
            reg::ComponentKind::DAS_SCRIPT,
            static_cast<size_t> (reg.struct_size));
      }

      ++das_count;
      break;
    case das_registration::singleton:
      m_runtime_ctx->singleton_registry ()
          .register_cached_runtime_singleton_component (
              static_cast<entt::id_type> (reg.type_id), reg.type_name,
              reg.display_name);
      ++das_count;
      break;
    case das_registration::system:
      m_runtime_ctx->system_factory_registry ().register_cached_runtime_system (
          static_cast<entt::id_type> (reg.type_id), reg.type_name,
          reg.display_name, reg.script_path, *get_das_engine ());
      ++das_count;
      break;
    }
  }

  m_metadata_cache_loaded = false;
  write_registration_cache ();
}

void
runtime_project_module::unload ()
{
  if (!m_module_loaded && !m_metadata_cache_loaded) {
    return;
  }

  wsl::log::cmake ()->trace ("Unloading module");

  // Clear runtime registries before unloading the library. Some registration
  // targets (std::function, entt meta descriptors, etc.) may require
  // shared library code to run their destructors; clearing them while the
  // library is still loaded avoids calling into freed code during shutdown.
  if (m_runtime_ctx != nullptr) {
    clear_runtime_registries (*m_runtime_ctx);
  }

  // Clear any pending registrations
  runtime_registrar::component_registrations ().clear ();
  runtime_registrar::singleton_registrations ().clear ();
  runtime_registrar::system_registrations ().clear ();

  // Unbind active runtime context
  runtime_registrar::set_active_runtime_context (nullptr);

  // Shutdown daslang engine
  if (m_das_engine) {
    m_das_engine->shutdown ();
    m_das_engine.reset ();
  }

  // Now unload the shared library (clear hooks first to avoid dangling
  // pointers)
  m_hook_init = nullptr;
  m_hook_update = nullptr;
  m_hook_shutdown = nullptr;
  m_loaded_library.reset ();

  m_module_loaded = false;
  m_metadata_cache_loaded = false;
  m_loaded_project_root.clear ();
  m_source_hash = 0;
  m_last_status = "Module unloaded.";
}

wsl::das::das_engine *
runtime_project_module::get_das_engine ()
{
  if (!m_das_engine) {
    m_das_engine = std::make_unique<wsl::das::das_engine> ();
  }
  return m_das_engine.get ();
}

bool
runtime_project_module::compile_and_load (const rsc::project &project)
{
  const fs::path project_root = fs::weakly_canonical (project.root_path);

  wsl::log::cmake ()->trace ("Compile and load started for project root: {}",
                             project_root.string ());

  source_set sources;
  wsl::log::cmake ()->trace (
      "Gathering files from components, systems, singletons and src paths...");
  wsl::log::cmake ()->trace (
      "  components_path: {}",
      (project_root / project.components_path).string ());
  wsl::log::cmake ()->trace ("  systems_path: {}",
                             (project_root / project.systems_path).string ());
  wsl::log::cmake ()->trace (
      "  singletons_path: {}",
      (project_root / project.singletons_path).string ());
  wsl::log::cmake ()->trace ("  src_path: {}",
                             (project_root / "src").string ());
  gather_files (project_root / project.components_path, sources);
  gather_files (project_root / project.systems_path, sources);
  gather_files (project_root / project.singletons_path, sources);
  gather_files (project_root / "src", sources);
  wsl::log::cmake ()->trace (
      "Gathered {} headers, {} cpp sources, and {} das sources",
      sources.headers.size (), sources.cpp_sources.size (),
      sources.das_sources.size ());

  const std::size_t current_hash = compute_source_hash (sources);

  if (m_module_loaded && current_hash == m_source_hash) {
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

    // Restore das registrations from the JSON cache so finalize_load
    // can re-apply them without re-executing .das files.
    registration_cache cache{};
    if (read_registration_cache (registration_cache_path (project_root),
                                 current_hash, cache)) {
      load_das_registrations_from_cache (cache);
    }

    // Initialize the daslang engine so das_system instances can call
    // das functions at runtime.
    auto *das_eng = get_das_engine ();
    if (das_eng && !das_eng->initialize ()) {
      wsl::log::cmake ()->warn (
          "Failed to initialize daslang engine on cache path: {}",
          das_eng->last_error ());
    }

    // Execute the daslang files so the persistent context has compiled
    // programs available for runtime calls (on_init, on_update, etc.).
    for (const auto &reg : m_das_registrations) {
      if (!reg.script_path.empty ()) {
        if (!das_eng->execute_file (reg.script_path)) {
          wsl::log::cmake ()->warn (
              "Failed to execute daslang file on cache path: {} - {}",
              reg.script_path, das_eng->last_error ());
        }
      } else {
        // Reconstruct script path from project structure
        fs::path dir;
        switch (reg.kind) {
        case das_registration::component:
          dir = project_root / project.components_path;
          break;
        case das_registration::singleton:
          dir = project_root / project.singletons_path;
          break;
        case das_registration::system:
          dir = project_root / project.systems_path;
          break;
        }
        auto das_path = dir / (reg.type_name + ".das");
        if (fs::exists (das_path)) {
          if (!das_eng->execute_file (das_path)) {
            wsl::log::cmake ()->warn (
                "Failed to execute daslang file on cache path: {} - {}",
                das_path.string (), das_eng->last_error ());
          }
        }
      }
    }

    // Register all cached systems in the factory so scene deserialization
    // can find them.
    finalize_load ();

    m_last_status = "Runtime module loaded from shared library cache.";
    return true;
  }

  // Cache miss - compile to shared library
  if (sources.cpp_sources.empty () && sources.das_sources.empty ()) {
    m_last_status = "No source files found to compile.";
    wsl::log::cmake ()->debug ("{}", m_last_status);
    m_module_loaded = true;
    m_source_hash = current_hash;
    return true;
  }

  const bool has_native_code
      = !sources.cpp_sources.empty () || !sources.headers.empty ();
  const fs::path build_dir = project_root / "build" / "weasel_runtime";

  if (has_native_code) {
    fs::create_directories (build_dir);

    const fs::path generated_path = build_dir / k_generated_module_file;

    wsl::log::cmake ()->debug ("Writing generated translation unit to: {}",
                               generated_path.string ());
    if (!write_generated_translation_unit (generated_path, sources)) {
      m_last_status = "Failed to generate the runtime module source file.";
      wsl::log::cmake ()->error ("{}", m_last_status);
      return false;
    }

    const fs::path so_path = shared_library_path (project_root);
    wsl::log::cmake ()->trace ("Compiling shared library...");
    if (!compile_to_shared_library (generated_path, so_path)) {
      m_last_status = "Failed to compile runtime module to shared library.";
      wsl::log::cmake ()->error ("{}", m_last_status);
      return false;
    }

    // Write the source hash so try_load_cached_shared_library can find it.
    write_source_hash (source_hash_path (project_root), current_hash);

    // Load the compiled shared library
    if (!try_load_cached_shared_library (current_hash)) {
      m_last_status = "Failed to load compiled shared library.";
      wsl::log::cmake ()->error ("{}", m_last_status);
      return false;
    }
  } else {
    wsl::log::cmake ()->debug (
        "No native C++ sources found, skipping shared library compilation.");
  }

  m_module_loaded = true;
  m_metadata_cache_loaded = false;
  m_source_hash = current_hash;
  wsl::log::cmake ()->debug ("Shared library compiled and loaded.");

  // Execute daslang files and store registrations for finalize_load
  m_das_registrations.clear ();
  if (!sources.das_sources.empty ()) {
    wsl::log::cmake ()->debug ("Executing {} daslang files...",
                               sources.das_sources.size ());
    auto *das_engine = get_das_engine ();
    if (!das_engine->initialize ()) {
      m_last_status
          = "Failed to initialize daslang engine: " + das_engine->last_error ();
      wsl::log::cmake ()->error ("{}", m_last_status);
      return false;
    }

    const auto comp_dir
        = fs::weakly_canonical (project_root / project.components_path)
              .string ();
    const auto singl_dir
        = fs::weakly_canonical (project_root / project.singletons_path)
              .string ();
    const auto sys_dir
        = fs::weakly_canonical (project_root / project.systems_path).string ();

    // Register source directories as daScript extra roots so that
    // bare `require` statements (e.g. `require mouse_rotate`) resolve
    // across directories — matching AOT behaviour where CMake copies
    // all component files alongside each system file during compilation.
    das_engine->addFsRoot ("components", comp_dir);
    das_engine->addFsRoot ("singletons", singl_dir);
    das_engine->addFsRoot ("systems", sys_dir);

    for (const auto &das_file : sources.das_sources) {
      wsl::log::cmake ()->debug ("Executing daslang file: {}",
                                 das_file.string ());
      if (!das_engine->execute_file (das_file)) {
        m_last_status = "Failed to execute daslang file: " + das_file.string ()
                        + "\n" + das_engine->last_error ();
        wsl::log::cmake ()->error ("{}", m_last_status);
        return false;
      }

      // Store the registration for apply in finalize_load
      const auto canonical = fs::weakly_canonical (das_file).generic_string ();
      const auto stem = das_file.stem ().string ();

      // Convert snake_case stem to Title Case for display and PascalCase for
      // lookup
      std::string titled;
      std::string pascal;
      bool capitalize_next = true;
      for (char ch : stem) {
        if (ch == '_' || ch == '-') {
          titled += ' ';
          capitalize_next = true;
        } else if (capitalize_next) {
          auto upper = static_cast<char> (
              std::toupper (static_cast<unsigned char> (ch)));
          titled += upper;
          pascal += upper;
          capitalize_next = false;
        } else {
          titled += ch;
          pascal += ch;
        }
      }

      const std::uint64_t type_id
          = static_cast<std::uint64_t> (entt::hashed_string{ stem.c_str () });

      // Extract struct fields from the compiled das file
      // Try PascalCase first (e.g. "MouseRotate"), then stem (e.g.
      // "mouse_rotate")
      std::string struct_name = pascal;
      auto si = das_engine->get_struct_info (das_file, struct_name);
      if (si.fields.empty () && struct_name != stem) {
        si = das_engine->get_struct_info (das_file, stem);
      }

      if (canonical.compare (0, comp_dir.size (), comp_dir) == 0) {
        m_das_registrations.push_back (
            { das_registration::component, stem, titled, type_id, si.size_of,
              das_file.string (), std::move (si.fields) });
      } else if (canonical.compare (0, singl_dir.size (), singl_dir) == 0) {
        m_das_registrations.push_back (
            { das_registration::singleton, stem, titled, type_id, si.size_of,
              das_file.string (), std::move (si.fields) });
      } else if (canonical.compare (0, sys_dir.size (), sys_dir) == 0) {
        m_das_registrations.push_back (
            { das_registration::system, stem, titled, type_id, si.size_of,
              das_file.string (), std::move (si.fields) });
      }
    }
  }

  m_last_status = "Runtime systems/components compiled and registered.";
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

  // Load compile command to get include paths
  const auto compile_command = load_compile_command ();
  if (compile_command) {
    const auto command_base
        = make_shared_command_base (compile_command->second);
    if (command_base) {
      std::vector<std::string> all_args = split_command_line (*command_base);
      if (all_args.size () > 1) {
        for (std::size_t i = 1; i < all_args.size (); ++i) {
          cmd << "\"" << all_args[i] << "\" ";
        }
      }
    }
  }

  // Add entt include path
  cmd << "\"-isystem\" ";
  cmd << "\"" << std::string (WEASEL_BUILD_DIR) + "/_deps/entt-src/src\" ";

  // Platform-specific shared library flags
#if defined(_MSC_VER)
  cmd << "/LD /EHsc /MD ";
#elif defined(__APPLE__)
  cmd << "-shared -fPIC ";
#else
  cmd << "-shared -fPIC ";

  // Link against engine and SDL so dlopen can resolve symbols.
  const std::string build_dir = std::string (WEASEL_BUILD_DIR);
  cmd << "\"-L" << build_dir << "\" ";
  cmd << "\"-L" << build_dir << "/_deps/sdl3-build\" ";
  cmd << "-lwsl -lSDL3 -Wl,--no-as-needed ";
  cmd << "\"-Wl,-rpath," << build_dir << "\" ";
  cmd << "\"-Wl,-rpath," << build_dir << "/_deps/sdl3-build\" ";
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
        "Shared library compilation failed (exit code {}).", result);
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

  // Clear any previous registrations
  runtime_registrar::component_registrations ().clear ();
  runtime_registrar::singleton_registrations ().clear ();
  runtime_registrar::system_registrations ().clear ();

  wsl::log::cmake ()->debug ("Loading cached shared library: {}",
                             so_path.string ());

  // Load the shared library
  m_loaded_library = std::make_unique<dynamic_library> (so_path);
  if (!m_loaded_library->is_loaded ()) {
    wsl::log::cmake ()->warn ("Failed to load shared library: {} ({})",
                              so_path.string (),
                              m_loaded_library->last_error ());
    m_loaded_library.reset ();
    return false;
  }

  wsl::log::cmake ()->debug (
      "Cached shared library loaded. Buckets: comp={}, singl={}, sys={}",
      runtime_registrar::component_registrations ().size (),
      runtime_registrar::singleton_registrations ().size (),
      runtime_registrar::system_registrations ().size ());

  resolve_user_hooks ();

  return true;
}

void
runtime_project_module::resolve_user_hooks ()
{
  m_hook_init = nullptr;
  m_hook_update = nullptr;
  m_hook_shutdown = nullptr;

  if (!m_loaded_library || !m_loaded_library->is_loaded ()) {
    return;
  }

  m_hook_init = reinterpret_cast<hook_init_fn> (
      m_loaded_library->get_symbol ("wsl_on_project_init"));
  m_hook_update = reinterpret_cast<hook_update_fn> (
      m_loaded_library->get_symbol ("wsl_on_project_update"));
  m_hook_shutdown = reinterpret_cast<hook_shutdown_fn> (
      m_loaded_library->get_symbol ("wsl_on_project_shutdown"));

  wsl::log::cmake ()->debug (
      "User hooks resolved: init={} update={} shutdown={}",
      m_hook_init != nullptr, m_hook_update != nullptr,
      m_hook_shutdown != nullptr);
}

} // namespace runtime
} // namespace reg
} // namespace wsl
