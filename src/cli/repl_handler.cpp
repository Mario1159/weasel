#include "repl_handler.hpp"
#include "wsl/log/log.hpp"

#include "comp/area3d.hpp"
#include "comp/camera.hpp"
#include "comp/character_body.hpp"
#include "comp/component_meta.hpp"
#include "comp/directional_light.hpp"
#include "comp/hierarchy.hpp"
#include "comp/model_instance_3d.hpp"
#include "comp/point_light.hpp"
#include "comp/rigid_body.hpp"
#include "comp/singl/physics_manager.hpp"
#include "comp/singl/rendering_manager.hpp"
#include "comp/spot_light.hpp"
#include "comp/transform.hpp"
#include "comp/world_transform.hpp"
#include "math/vector.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/rsc/project_loader.hpp"
#include "wsl/rsc/project.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/scene_snapshot_serializer.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/ui_manager.hpp"
#include "wsl/sys/system.hpp"
#include "wsl/comp/components.hpp"

#include <cereal/archives/json.hpp>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <filesystem>
#include <fstream>

using namespace entt::literals;
using namespace wsl::rsc;

namespace wsl::cli
{

namespace
{

void
register_repl_types (wsl::comp::singl::runtime_context &rtc)
{
  wsl::comp::singl::runtime_context::register_meta ();
  wsl::comp::singl::editor_context::register_meta ();
  wsl::comp::singl::ui_manager::register_meta ();

  wsl::comp::register_component_meta<
      wsl::comp::hierarchy, wsl::comp::world_transform, wsl::comp::transform,
      wsl::math::vec3f, wsl::math::quatf, wsl::math::mat33f, wsl::math::mat44f,
      wsl::rsc::model_id, wsl::comp::model_instance_3d, wsl::comp::camera,
      wsl::comp::point_light, wsl::comp::spot_light,
      wsl::comp::directional_light, wsl::comp::rigid_body, wsl::comp::area,
      wsl::comp::character_body, wsl::rsc::scene_manager,
      wsl::rsc::resource_manager_view> ();

  wsl::comp::for_each_type<wsl::comp::component_types>::apply (
      [&rtc]<typename T> () {
        rtc.component_registry.register_world_component<T> ();
      });

  rtc.singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
          { "Runtime Context", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::editor_context> (
          { "Editor Context", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::rsc::scene_manager> (
          { "Scene Manager", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::rsc::resource_manager_view> (
          { "Resource Manager", true });
  rtc.singleton_registry
      .register_bound_singleton_component<wsl::comp::singl::ui_manager> (
          { "UI Manager", true, false, true });
  rtc.singleton_registry
      .register_singleton_component<wsl::comp::singl::rendering_manager> (
          { "Rendering Manager", true });
  rtc.singleton_registry
      .register_singleton_component<wsl::comp::singl::physics_manager> (
          { "Physics Manager", true });
}

template <typename Descriptor>
void
write_registered_entry (std::ostringstream &output,
                        const Descriptor &descriptor)
{
  output << " - " << descriptor.display_name;
  if (!descriptor.type_name.empty ()
      && descriptor.type_name != descriptor.display_name) {
    output << " [" << descriptor.type_name << "]";
  }
  if (descriptor.runtime_registered) {
    output << " [runtime]";
  }
  output << "\n";
}

std::string
quote_repl_arg (std::string_view arg)
{
  const bool needs_quotes
      = arg.empty ()
        || std::any_of (arg.begin (), arg.end (), [] (unsigned char ch) {
             return std::isspace (ch) || ch == '"' || ch == '\'';
           });
  if (!needs_quotes) {
    return std::string (arg);
  }

  std::string out;
  out.reserve (arg.size () + 2);
  out.push_back ('"');
  for (char ch : arg) {
    if (ch == '"' || ch == '\\') {
      out.push_back ('\\');
    }
    out.push_back (ch);
  }
  out.push_back ('"');
  return out;
}

std::string
build_repl_command (const std::vector<std::string> &args)
{
  std::ostringstream output;
  for (std::size_t i = 0; i < args.size (); ++i) {
    if (i > 0) {
      output << ' ';
    }
    output << quote_repl_arg (args[i]);
  }
  return output.str ();
}

bool
command_failed (std::string_view output)
{
  return output.find ("Failed") != std::string_view::npos
         || output.find ("Error:") != std::string_view::npos;
}

// Set a meta_any value from a parsed nlohmann::json value.
// Modifies the field in-place via its void pointer.
// The optional resource_manager is used to resolve resource path strings
// (e.g. "builtin://sphere") into typed resource handles such as model_id.
// Find meta_data by hashed name or by lowercased display name.
entt::meta_data
find_meta_data (const entt::meta_type &meta, const std::string &name)
{
  entt::id_type hash = entt::hashed_string::value (name.c_str (), name.size ());
  auto data = meta.data (hash);
  if (data)
    return data;

  // Fallback: match by lowercased display name
  std::string lower;
  for (unsigned char c : name)
    lower.push_back (static_cast<char> (std::tolower (c)));
  for (auto [fid, fd] : meta.data ()) {
    (void)fid;
    auto mi = wsl::comp::get_meta_info (fd);
    if (mi) {
      std::string dl;
      for (unsigned char c : mi->display_name)
        dl.push_back (static_cast<char> (std::tolower (c)));
      if (dl == lower)
        return fd;
    }
  }
  return {};
}

bool
set_meta_from_json (entt::meta_any &field_inst, const nlohmann::json &j,
                    wsl::rsc::resource_manager *res_mgr = nullptr)
{
  auto meta = field_inst.type ();
  if (!meta)
    return false;
  void *ptr = const_cast<void *> (field_inst.base ().data ());
  if (!ptr)
    return false;

  auto type_id = meta.id ();

  // ── model_id: accept a string resource path ──
  if (type_id == entt::type_hash<wsl::rsc::model_id>::value ()) {
    if (j.is_string () && res_mgr) {
      std::string path = j.get<std::string> ();
      *static_cast<wsl::rsc::model_id *> (ptr) = res_mgr->register_model (path);
      return true;
    }
    return false;
  }

  // Arithmetic types
  if (meta.is_arithmetic ()) {
    if (type_id == entt::type_hash<float>::value ()) {
      *static_cast<float *> (ptr) = j.get<float> ();
      return true;
    }
    if (type_id == entt::type_hash<int>::value ()) {
      *static_cast<int *> (ptr) = j.get<int> ();
      return true;
    }
    if (type_id == entt::type_hash<bool>::value ()) {
      *static_cast<bool *> (ptr) = j.get<bool> ();
      return true;
    }
    if (type_id == entt::type_hash<uint32_t>::value ()) {
      *static_cast<uint32_t *> (ptr) = j.get<uint32_t> ();
      return true;
    }
    if (type_id == entt::type_hash<double>::value ()) {
      *static_cast<double *> (ptr) = j.get<double> ();
      return true;
    }
    return false;
  }

  // Enum types
  if (meta.is_enum ()) {
    if (j.is_string ()) {
      std::string name = j.get<std::string> ();
      for (auto [ev_id, ev_data] : meta.data ()) {
        (void)ev_id;
        if (const char *const *p = ev_data.custom (); p && *p) {
          if (name == *p) {
            entt::meta_any enum_val = ev_data.get ({});
            if (enum_val && enum_val.allow_cast<int> ()) {
              std::memcpy (ptr, enum_val.base ().data (), meta.size_of ());
              return true;
            }
          }
        }
      }
      return false;
    }
    if (j.is_number_integer ()) {
      int val = j.get<int> ();
      std::memcpy (ptr, &val, meta.size_of ());
      return true;
    }
    return false;
  }

  // String type
  if (type_id == entt::type_hash<std::string>::value ()) {
    *static_cast<std::string *> (ptr) = j.get<std::string> ();
    return true;
  }

  // Compound type with registered sub-fields
  if (meta.is_class ()) {
    if (j.is_array ()) {
      size_t idx = 0;
      for (auto [fid, fdata] : meta.data ()) {
        (void)fid;
        if (idx >= j.size ())
          break;
        entt::meta_any sub_inst = fdata.get (field_inst);
        if (sub_inst && set_meta_from_json (sub_inst, j[idx], res_mgr)) {
          fdata.set (field_inst, sub_inst);
        }
        ++idx;
      }
      return true;
    }
    if (j.is_object ()) {
      for (auto [fid, fdata] : meta.data ()) {
        (void)fid;
        auto mi = wsl::comp::get_meta_info (fdata);
        std::string fn = mi ? mi->display_name : "";
        if (fn.empty ())
          continue;
        std::string key;
        for (char c : fn)
          key.push_back (static_cast<char> (
              std::tolower (static_cast<unsigned char> (c))));
        if (j.contains (key)) {
          entt::meta_any sub_inst = fdata.get (field_inst);
          if (sub_inst && set_meta_from_json (sub_inst, j[key], res_mgr)) {
            fdata.set (field_inst, sub_inst);
          }
        }
      }
      return true;
    }
    // Single value fallback: try each sub-field
    if (!j.is_object () && !j.is_array ()) {
      for (auto [fid, fdata] : meta.data ()) {
        (void)fid;
        entt::meta_any sub_inst = fdata.get (field_inst);
        if (sub_inst && sub_inst.type ().is_arithmetic ()) {
          if (set_meta_from_json (sub_inst, j, res_mgr)) {
            fdata.set (field_inst, sub_inst);
            return true;
          }
        }
      }
    }
  }

  return false;
}

// Try to set a component property from a string value.
// Returns true on success and writes a description to out_msg.
// The optional resource_manager is forwarded to set_meta_from_json for
// resolving resource paths (model_id, etc.).
bool
set_component_property (entt::meta_any &instance, entt::meta_data prop_data,
                        const std::string &value_str, std::string &out_msg,
                        wsl::rsc::resource_manager *res_mgr = nullptr)
{
  auto field_type = prop_data.type ();
  if (!field_type) {
    out_msg = "Property has no reflected type";
    return false;
  }

  nlohmann::json j;
  try {
    j = nlohmann::json::parse (value_str);
  } catch (...) {
    j = value_str;
  }

  // Get current field value to modify in-place (always use this path
  // for reliability — avoids potential issues with meta_data::set and
  // from_void instances, and also handles enum types correctly).
  entt::meta_any field_inst = prop_data.get (instance);
  if (!field_inst) {
    out_msg = "Failed to access property value";
    return false;
  }

  // For enum types with string values, build the correct integer via enum
  // data lookup, then inject into the get-modify-set flow.
  if (field_type.is_enum () && j.is_string ()) {
    std::string name = j.get<std::string> ();
    for (auto [ev_id, ev_data] : field_type.data ()) {
      (void)ev_id;
      if (const char *const *p = ev_data.custom (); p && *p && *p == name) {
        entt::meta_any enum_val = ev_data.get ({});
        if (enum_val && enum_val.base ().data ()) {
          void *fp = const_cast<void *> (field_inst.base ().data ());
          if (fp) {
            std::memcpy (fp, enum_val.base ().data (), field_type.size_of ());
            prop_data.set (instance, field_inst);
            out_msg = "set to " + name;
            return true;
          }
        }
      }
    }
  }

  // Auto-descend into single-field wrapper structs (e.g. motion_type_ui →
  // value)
  if (field_type.is_class () && !field_type.is_enum () && j.is_string ()) {
    // Try the parent type first — resource handles (model_id, audio_id, etc.)
    // have their own handlers in set_meta_from_json that should take priority
    // over descending into the wrapper.
    if (set_meta_from_json (field_inst, j, res_mgr)) {
      prop_data.set (instance, field_inst);
      out_msg = "set";
      return true;
    }

    auto data_range = field_type.data ();
    auto it = data_range.begin ();
    if (it != data_range.end ()) {
      auto sub_data = it->second;
      ++it;
      if (it == data_range.end ()) {
        entt::meta_any sub_inst = sub_data.get (field_inst);
        if (sub_inst && set_meta_from_json (sub_inst, j, res_mgr)) {
          sub_data.set (field_inst, sub_inst);
          prop_data.set (instance, field_inst);
          out_msg = "set";
          return true;
        }
      }
    }
  }

  if (set_meta_from_json (field_inst, j, res_mgr)) {
    prop_data.set (instance, field_inst);
    out_msg = "set";
    return true;
  }

  // Fallback: try allow_cast from a basic meta_any
  entt::meta_any val;
  if (j.is_number_float ())
    val = j.get<float> ();
  else if (j.is_number_integer ())
    val = j.get<int> ();
  else if (j.is_boolean ())
    val = j.get<bool> ();
  else
    val = j.get<std::string> ();

  entt::meta_any converted = val.allow_cast (field_type);
  if (converted) {
    prop_data.set (instance, converted);
    out_msg = "set";
    return true;
  }

  out_msg = "Unsupported value type or conversion failed";
  return false;
}

// ── Value formatting for read-back ──

void
format_meta_value (std::ostringstream &output, const entt::meta_any &field_inst,
                   const std::string &indent)
{
  auto type = field_inst.type ();
  if (!type) {
    output << "<?>";
    return;
  }
  const void *ptr = field_inst.base ().data ();
  if (!ptr) {
    output << "<?>";
    return;
  }

  auto type_id = type.id ();

  // arithmetic
  if (type.is_arithmetic ()) {
    if (type_id == entt::type_hash<float>::value ())
      output << *static_cast<const float *> (ptr);
    else if (type_id == entt::type_hash<int>::value ())
      output << *static_cast<const int *> (ptr);
    else if (type_id == entt::type_hash<bool>::value ())
      output << (*static_cast<const bool *> (ptr) ? "true" : "false");
    else if (type_id == entt::type_hash<uint32_t>::value ())
      output << *static_cast<const uint32_t *> (ptr);
    else if (type_id == entt::type_hash<double>::value ())
      output << *static_cast<const double *> (ptr);
    else
      output << "<?>";
    return;
  }

  // string
  if (type_id == entt::type_hash<std::string>::value ()) {
    output << "\"" << *static_cast<const std::string *> (ptr) << "\"";
    return;
  }

  // model_id
  if (type_id == entt::type_hash<wsl::rsc::model_id>::value ()) {
    output << "model#" << static_cast<const wsl::rsc::model_id *> (ptr)->value;
    return;
  }

  // enum
  if (type.is_enum ()) {
    for (auto [ev_id, ev_data] : type.data ()) {
      (void)ev_id;
      entt::meta_any ev = ev_data.get ({});
      if (ev && ev.base ().data ()
          && std::memcmp (ev.base ().data (), ptr, type.size_of ()) == 0) {
        if (const char *const *p = ev_data.custom (); p && *p) {
          output << *p;
          return;
        }
      }
    }
    // fallback: show integer
    int iv = 0;
    std::memcpy (&iv, ptr, (std::min)(type.size_of (), sizeof (int)));
    output << iv;
    return;
  }

  // compound with sub-fields
  if (type.is_class ()) {
    bool has_sub = false;
    for (auto [fid, fdata] : type.data ()) {
      (void)fid;
      (void)fdata;
      has_sub = true;
      break;
    }
    if (has_sub) {
      output << "{";
      for (auto [fid, fdata] : type.data ()) {
        (void)fid;
        auto mi = wsl::comp::get_meta_info (fdata);
        std::string dn = mi ? mi->display_name : "?";
        entt::meta_any sub = fdata.get (field_inst);
        output << "\n" << indent << "  " << dn << " = ";
        if (sub)
          format_meta_value (output, sub, indent + "  ");
        else
          output << "<?>";
      }
      output << "\n" << indent << "}";
      return;
    }
  }

  output << "<?>";
}

void
format_component_properties (std::ostringstream &output,
                             const entt::meta_type &meta, void *instance,
                             const std::string &indent)
{
  entt::meta_any comp_any = meta.from_void (instance);
  if (!comp_any)
    return;
  for (auto [fid, fdata] : meta.data ()) {
    (void)fid;
    auto mi = wsl::comp::get_meta_info (fdata);
    std::string dn = mi ? mi->display_name : "?";
    entt::meta_any sub = fdata.get (comp_any);
    output << "\n" << indent << "  " << dn << " = ";
    if (sub)
      format_meta_value (output, sub, indent + "  ");
    else
      output << "<?>";
  }
}

} // namespace

namespace
{

// ── name conversion helpers (mirrors editor/file_list.cpp) ──

std::vector<std::string>
tokenize_name (std::string_view raw_name)
{
  std::vector<std::string> tokens;
  std::string current;
  char previous = '\0';

  auto flush = [&] () {
    if (!current.empty ()) {
      tokens.push_back (current);
      current.clear ();
    }
  };

  for (char const ch : raw_name) {
    const unsigned char uch = static_cast<unsigned char> (ch);
    if (std::isalnum (uch) == 0) {
      flush ();
      previous = '\0';
      continue;
    }

    if ((std::isupper (uch) != 0) && !current.empty ()
        && (std::islower (static_cast<unsigned char> (previous)) != 0)) {
      flush ();
    }

    current.push_back ((char)std::tolower (uch));
    previous = ch;
  }

  flush ();
  return tokens;
}

std::string
to_snake_case (const std::vector<std::string> &tokens)
{
  std::string out;
  for (std::size_t i = 0; i < tokens.size (); ++i) {
    if (i != 0)
      out += '_';
    out += tokens[i];
  }
  return out;
}

std::string
to_pascal_case (const std::vector<std::string> &tokens)
{
  std::string out;
  for (const std::string &token : tokens) {
    if (token.empty ())
      continue;
    std::string piece = token;
    piece[0] = (char)std::toupper (static_cast<unsigned char> (piece[0]));
    out += piece;
  }
  return out;
}

std::string
to_title_case (const std::vector<std::string> &tokens)
{
  std::string out;
  for (std::size_t i = 0; i < tokens.size (); ++i) {
    if (i != 0)
      out += ' ';
    std::string piece = tokens[i];
    if (!piece.empty ())
      piece[0] = (char)std::toupper (static_cast<unsigned char> (piece[0]));
    out += piece;
  }
  return out;
}

bool
write_text_file (const std::filesystem::path &path, std::string_view text)
{
  std::ofstream output (path, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;
  output.write (text.data (), (std::streamsize)text.size ());
  return output.good ();
}

// ── template generators (mirrors editor/file_list.cpp) ──

std::string
make_component_header_template (const std::string &class_name,
                                const std::string &display_name,
                                bool header_only)
{
  std::ostringstream output;
  output << "#pragma once\n\n";
  output << "#include \"wsl/comp/component_meta.hpp\"\n";
  output << "#include \"wsl/reg/runtime_project_module_api.hpp\"\n";
  output << "#include <cereal/cereal.hpp>\n\n";
  output << "#include <entt/meta/factory.hpp>\n\n";
  output << "namespace wsl::comp\n{\n\n";
  output << "struct " << class_name << " : world_component {\n";
  output << "  float value = 1.0f;\n\n";
  if (header_only) {
    output << "  static void register_meta() {\n";
    output << "    entt::meta_factory<" << class_name << "> factory\n";
    output << "        = reflect_type<" << class_name << ">(\n";
    output << "              entt::type_hash<" << class_name << ">::value(),\n";
    output << "              \"" << display_name << "\",\n";
    output << "              \"Describe what this component stores.\");\n";
    output << "    reflect_field<" << class_name << ",\n";
    output << "                 &" << class_name << "::value>(\n";
    output << "        factory, \"value\", {}, \"Example editable field.\");\n";
    output << "  }\n\n";
  } else {
    output << "  static void register_meta();\n\n";
  }
  output << "  template <class Archive> void serialize(Archive &archive) {\n";
  output << "    archive(cereal::make_nvp(\"value\", value));\n";
  output << "  }\n";
  output << "};\n\n";
  output << "} // namespace wsl::comp\n";
  if (header_only)
    output << "\nWEASEL_RUNTIME_COMPONENT(wsl::comp::" << class_name << ")\n";
  return output.str ();
}

std::string
make_component_source_template (const std::string &header_name,
                                const std::string &class_name,
                                const std::string &display_name,
                                bool is_singleton)
{
  std::ostringstream output;
  output << "#include \"" << header_name << "\"\n\n";
  output << "#include <entt/meta/factory.hpp>\n\n";
  output << "void wsl::comp::" << class_name << "::register_meta() {\n";
  output << "  entt::meta_factory<wsl::comp::" << class_name << "> factory\n";
  output << "      = wsl::comp::reflect_type<wsl::comp::" << class_name
         << ">(\n";
  output << "            entt::type_hash<wsl::comp::" << class_name
         << ">::value(),\n";
  output << "            \"" << display_name << "\",\n";
  output << "            \"Describe what this "
         << (is_singleton ? "singleton" : "component") << " stores.\");\n\n";
  output << "  wsl::comp::reflect_field<wsl::comp::" << class_name << ",\n";
  output << "                           &wsl::comp::" << class_name
         << "::value>(\n";
  output << "      factory, \"value\", {}, \"Example editable field.\");\n";
  output << "}\n\n";

  if (is_singleton)
    output << "WEASEL_RUNTIME_SINGLETON(wsl::comp::" << class_name << ", \""
           << display_name << "\")\n";
  else
    output << "WEASEL_RUNTIME_COMPONENT(wsl::comp::" << class_name << ")\n";

  return output.str ();
}

std::string
make_singleton_header_template (const std::string &class_name,
                                const std::string &display_name,
                                bool header_only)
{
  std::ostringstream output;
  output << "#pragma once\n\n";
  output << "#include \"wsl/comp/component_meta.hpp\"\n";
  output << "#include \"wsl/reg/runtime_project_module_api.hpp\"\n";
  output << "#include <cereal/cereal.hpp>\n\n";
  output << "#include <entt/meta/factory.hpp>\n\n";
  output << "namespace wsl::comp\n{\n\n";
  output << "struct " << class_name << " : singleton_component {\n";
  output << "  float value = 1.0f;\n\n";
  if (header_only) {
    output << "  static void register_meta() {\n";
    output << "    entt::meta_factory<" << class_name << "> factory\n";
    output << "        = reflect_type<" << class_name << ">(\n";
    output << "              entt::type_hash<" << class_name << ">::value(),\n";
    output << "              \"" << display_name << "\",\n";
    output << "              \"Describe what this singleton stores.\");\n";
    output << "    reflect_field<" << class_name << ",\n";
    output << "                 &" << class_name << "::value>(\n";
    output << "        factory, \"value\", {}, \"Example editable field.\");\n";
    output << "  }\n\n";
  } else {
    output << "  static void register_meta();\n\n";
  }
  output << "  template <class Archive> void serialize(Archive &archive) {\n";
  output << "    archive(cereal::make_nvp(\"value\", value));\n";
  output << "  }\n";
  output << "};\n\n";
  output << "} // namespace wsl::comp\n";
  if (header_only)
    output << "\nWEASEL_RUNTIME_SINGLETON(wsl::comp::" << class_name << ", \""
           << display_name << "\")\n";
  return output.str ();
}

std::string
make_system_header_template (const std::string &class_name,
                             const std::string &display_name, bool header_only)
{
  std::ostringstream output;
  output << "#pragma once\n\n";
  output << "#include \"wsl/reg/runtime_project_module_api.hpp\"\n";
  output << "#include \"wsl/sys/system.hpp\"\n\n";
  output << "namespace wsl::sys {\n\n";
  output << "class " << class_name << " : public ecs_system_t<" << class_name
         << "> {\n";
  output << "public:\n";
  output << "  using ecs_system_t::ecs_system_t;\n\n";
  output << "  void register_signals(wsl::reg::sig::signal_hub &hub) override "
            "{}\n";
  output << "  void register_event_handlers(wsl::reg::sig::signal_hub &hub) "
            "override {}\n";
  output << "  void register_iterations(wsl::reg::sig::signal_hub &hub) "
            "override {}\n";
  output << "};\n\n";
  output << "} // namespace wsl::sys\n";
  if (header_only)
    output << "\nWEASEL_RUNTIME_SYSTEM(wsl::sys::" << class_name << ", \""
           << display_name << "\")\n";
  return output.str ();
}

std::string
make_system_source_template (const std::string &header_name,
                             const std::string &class_name,
                             const std::string &display_name)
{
  std::ostringstream output;
  output << "#include \"" << header_name << "\"\n\n";
  output << "WEASEL_RUNTIME_SYSTEM(wsl::sys::" << class_name << ", \""
         << display_name << "\")\n";
  return output.str ();
}

} // namespace

// -------- command_executor implementation --------

command_executor::command_executor (wsl::comp::singl::runtime_context &rtc)
    : m_rtc (rtc)
{
}

void
command_executor::set_current_project (std::shared_ptr<wsl::rsc::project> proj)
{
  m_current_project = std::move (proj);
}

void
command_executor::ensure_runtime_module_loaded ()
{
  if (!m_current_project || m_rtc.runtime_project_module.has_loaded_module ())
    return;

  std::filesystem::path const root (m_current_project->root_path);
  bool has_runtime = false;
  for (auto const &sub :
       { m_current_project->systems_path, m_current_project->components_path,
         m_current_project->singletons_path }) {
    if (std::filesystem::exists (root / sub)) {
      has_runtime = true;
      break;
    }
  }
  if (!has_runtime)
    return;

  wsl::log::cli ()->info ("Compiling and loading runtime module...");
  if (!m_rtc.runtime_project_module.compile_and_load (*m_current_project)) {
    m_output << "Warning: runtime module compilation failed.\n"
             << "  Some user-defined types may not be available.\n";
    return;
  }
  m_rtc.runtime_project_module.finalize_load ();
  wsl::log::cli ()->info ("Runtime module loaded successfully.");
}

void
command_executor::auto_save_scene (bool verbose)
{
  if (!m_auto_save)
    return;

  auto *scene = get_active_scene ();
  if (!scene) {
    if (verbose)
      m_output << "Auto-save skipped: no active scene.\n";
    return;
  }
  if (!m_current_project) {
    if (verbose)
      m_output << "Auto-save skipped: no project loaded.\n";
    return;
  }

  std::filesystem::path scenes_dir (m_current_project->root_path);
  scenes_dir /= m_current_project->scenes_path;
  std::string sname
      = std::filesystem::path (scene->get_name ()).filename ().string ();
  if (sname.ends_with (".wscn.json"))
    sname.resize (sname.size () - 10);
  else if (sname.ends_with (".json"))
    sname.resize (sname.size () - 5);
  std::string save_path = (scenes_dir / (sname + ".wscn.json")).string ();

  std::error_code ec;
  std::filesystem::create_directories (scenes_dir, ec);

  wsl::rsc::io::scene_snapshot_serializer serializer (&m_rtc, *scene);
  if (serializer.save_json (save_path)) {
    if (verbose)
      m_output << "Auto-saved scene to " << save_path << "\n";
  } else {
    m_output << "Auto-save failed for scene to " << save_path << "\n";
  }
}

void
command_executor::auto_save_project ()
{
  if (!m_auto_save)
    return;

  if (!m_current_project) {
    m_output << "Auto-save skipped: no project loaded.\n";
    return;
  }

  std::filesystem::path manifest
      = std::filesystem::path (m_current_project->root_path)
        / wsl::rsc::project_loader::manifest_file;
  std::ofstream file (manifest);
  if (!file) {
    m_output << "Auto-save failed for project.\n";
    return;
  }
  cereal::JSONOutputArchive archive (file);
  archive (cereal::make_nvp ("project", *m_current_project));
}

std::string
command_executor::execute (const std::string &line)
{
  m_output.str ("");
  m_output.clear ();

  // Strip inline comments: remove everything from the first unquoted '#'.
  std::string cleaned;
  cleaned.reserve (line.size ());
  bool in_sq = false, in_dq = false;
  for (char ch : line) {
    if (ch == '\'' && !in_dq) {
      in_sq = !in_sq;
      cleaned.push_back (ch);
      continue;
    }
    if (ch == '"' && !in_sq) {
      in_dq = !in_dq;
      cleaned.push_back (ch);
      continue;
    }
    if (ch == '#' && !in_sq && !in_dq)
      break; // strip rest of line
    cleaned.push_back (ch);
  }

  auto tokens = tokenize (cleaned);
  if (tokens.empty ())
    return "";

  const std::string &family = tokens[0];
  if (family == "proj")
    cmd_proj (tokens);
  else if (family == "scene")
    cmd_scene (tokens);
  else if (family == "ent")
    cmd_ent (tokens);
  else if (family == "comp")
    cmd_comp (tokens);
  else if (family == "singl")
    cmd_singl (tokens);
  else if (family == "sig")
    cmd_sig (tokens);
  else if (family == "sys")
    cmd_sys (tokens);
  else if (family == "check")
    cmd_check (tokens);
  else if (family == "rsc")
    cmd_rsc (tokens);
  else if (family == "help")
    cmd_help ();
  else if (family == "exit" || family == "quit")
    m_output << "exit\n";
  else if (family == "cls")
    m_output << "\033[2J\033[1;1H";
  else
    m_output << "Unknown command family: " << family
             << ". Type 'help' for usage.\n";

  return m_output.str ();
}

std::vector<std::string>
command_executor::tokenize (const std::string &line)
{
  std::vector<std::string> tokens;
  std::string current;
  bool in_single_quotes = false;
  bool in_double_quotes = false;
  bool escape_next = false;

  for (char ch : line) {
    if (escape_next) {
      current.push_back (ch);
      escape_next = false;
      continue;
    }

    if (in_double_quotes && ch == '\\') {
      escape_next = true;
      continue;
    }

    if (!in_double_quotes && ch == '\'') {
      in_single_quotes = !in_single_quotes;
      continue;
    }

    if (!in_single_quotes && ch == '"') {
      in_double_quotes = !in_double_quotes;
      continue;
    }

    if (!in_single_quotes && !in_double_quotes
        && std::isspace (static_cast<unsigned char> (ch))) {
      if (!current.empty ()) {
        tokens.push_back (std::move (current));
        current.clear ();
      }
      continue;
    }

    current.push_back (ch);
  }

  if (escape_next) {
    current.push_back ('\\');
  }

  if (!current.empty ()) {
    tokens.push_back (std::move (current));
  }

  return tokens;
}

wsl::rsc::scene *
command_executor::get_active_scene ()
{
  return m_rtc.scene_manager.get_active ();
}

void
command_executor::cmd_proj (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2) {
    m_output << "Usage: proj <new|load|info|save|set> [args...]\n";
    return;
  }
  const std::string &action = tokens[1];
  if (action == "new") {
    if (tokens.size () < 4) {
      m_output << "Usage: proj new <path> <name>\n";
      return;
    }
    wsl::rsc::project proj;
    proj.name = tokens[3];
    proj.root_path = std::filesystem::absolute (tokens[2]).string ();
    proj.systems_path = "src/systems";
    proj.components_path = "src/components";
    proj.singletons_path = "src/singletons";
    proj.scenes_path = "rsc/scenes";
    proj.models_path = "rsc/models";
    proj.images_path = "rsc/textures";
    proj.cubemaps_path = "rsc/textures/cubemaps";
    proj.audio_path = "rsc/audio";
    proj.fonts_path = "rsc/fonts";
    proj.ui_layouts_path = "src/ui";
    proj.shaders_path = "rsc/shaders";

    wsl::rsc::project_loader loader (&m_rtc);
    if (loader.create (proj)) {
      m_current_project = std::make_shared<wsl::rsc::project> (proj);
      m_output << "Project '" << proj.name << "' created and loaded.\n";
    } else
      m_output << "Failed to create project.\n";
  } else if (action == "load") {
    if (tokens.size () < 3) {
      m_output << "Usage: proj load <path>\n";
      return;
    }
    m_current_project = wsl::rsc::project_loader::load (tokens[2]);
    if (m_current_project) {
      m_output << "Project '" << m_current_project->name << "' loaded.\n";
    } else
      m_output << "Failed to load project.\n";
  } else if (action == "info") {
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    m_output << "Project: " << m_current_project->name
             << "\n  Author: " << m_current_project->author
             << "\n  Root: " << m_current_project->root_path
             << "\n  Default Scene: " << m_current_project->default_scene_path
             << "\n  Systems: " << m_current_project->systems_path
             << "\n  Components: " << m_current_project->components_path
             << "\n  Singletons: " << m_current_project->singletons_path
             << "\n  Scenes: " << m_current_project->scenes_path
             << "\n  Models: " << m_current_project->models_path
             << "\n  Images: " << m_current_project->images_path
             << "\n  Cubemaps: " << m_current_project->cubemaps_path
             << "\n  Audio: " << m_current_project->audio_path
             << "\n  UI Layouts: " << m_current_project->ui_layouts_path
             << "\n  Fonts: " << m_current_project->fonts_path
             << "\n  Shaders: " << m_current_project->shaders_path << "\n";
  } else if (action == "set") {
    if (tokens.size () < 4) {
      m_output << "Usage: proj set <field> <value>\n";
      return;
    }
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    const std::string &field = tokens[2];
    const std::string &value = tokens[3];
    bool found = true;
    if (field == "name") {
      m_current_project->name = value;
    } else if (field == "author") {
      m_current_project->author = value;
    } else if (field == "default_scene_path") {
      m_current_project->default_scene_path = value;
    } else if (field == "systems_path") {
      m_current_project->systems_path = value;
    } else if (field == "components_path") {
      m_current_project->components_path = value;
    } else if (field == "singletons_path") {
      m_current_project->singletons_path = value;
    } else if (field == "scenes_path") {
      m_current_project->scenes_path = value;
    } else if (field == "models_path") {
      m_current_project->models_path = value;
    } else if (field == "images_path") {
      m_current_project->images_path = value;
    } else if (field == "cubemaps_path") {
      m_current_project->cubemaps_path = value;
    } else if (field == "audio_path") {
      m_current_project->audio_path = value;
    } else if (field == "ui_layouts_path") {
      m_current_project->ui_layouts_path = value;
    } else if (field == "fonts_path") {
      m_current_project->fonts_path = value;
    } else if (field == "shaders_path") {
      m_current_project->shaders_path = value;
    } else {
      found = false;
      m_output
          << "Unknown project field: " << field
          << ". Supported: name, author, default_scene_path, systems_path, "
             "components_path, singletons_path, scenes_path, models_path, "
             "images_path, cubemaps_path, audio_path, ui_layouts_path, "
             "fonts_path, shaders_path\n";
    }
    if (found) {
      m_output << "Project field '" << field << "' set.\n";
      auto_save_project ();
    }
  } else if (action == "save") {
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    std::filesystem::path manifest
        = std::filesystem::path (m_current_project->root_path)
          / wsl::rsc::project_loader::manifest_file;
    std::ofstream file (manifest);
    if (!file) {
      m_output << "Failed to open project file for writing.\n";
      return;
    }
    cereal::JSONOutputArchive archive (file);
    archive (cereal::make_nvp ("project", *m_current_project));
    m_output << "Project saved to " << manifest.string () << "\n";
  }
}

void
command_executor::cmd_scene (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2) {
    m_output << "Usage: scene <new|load|save|ls|status> [args...]\n";
    return;
  }
  const std::string &action = tokens[1];
  if (action == "new") {
    if (tokens.size () < 3) {
      m_output << "Usage: scene new <name>\n";
      return;
    }
    auto &scene = m_rtc.scene_manager.create_scene (tokens[2], true);
    m_output << "Scene '" << tokens[2] << "' created and set as active.\n";
  } else if (action == "load") {
    if (tokens.size () < 3) {
      m_output << "Usage: scene load <path>\n";
      return;
    }
    if (!m_current_project) {
      m_output << "Error: Load a project first.\n";
      return;
    }
    std::string load_path = tokens[2];

    // Resolve relative paths against the project root
    if (!std::filesystem::path (load_path).is_absolute ()) {
      std::string rooted
          = (std::filesystem::path (m_current_project->root_path) / load_path)
                .string ();
      if (std::filesystem::exists (rooted)) {
        load_path = rooted;
      }
    }

    // Try to resolve by scene name if the resolved path doesn't exist
    if (!std::filesystem::exists (load_path)) {
      std::filesystem::path scenes_dir (m_current_project->root_path);
      scenes_dir /= m_current_project->scenes_path;
      std::string candidate
          = (scenes_dir / (load_path + ".wscn.json")).string ();
      if (std::filesystem::exists (candidate)) {
        load_path = candidate;
      } else {
        candidate = (scenes_dir / load_path).string ();
        if (std::filesystem::exists (candidate)) {
          load_path = candidate;
        }
      }
    }

    if (!std::filesystem::exists (load_path)) {
      m_output << "Failed to load scene: file not found '" << tokens[2]
               << "'.\n";
      return;
    }

    // Ensure runtime component types are registered before loading
    ensure_runtime_module_loaded ();

    // Derive scene name from the path basename (strip .wscn.json / .json)
    std::string scene_name
        = std::filesystem::path (load_path).stem ().stem ().string ();

    try {
      auto &scene = m_rtc.scene_manager.create_scene (scene_name, true);
      wsl::rsc::io::scene_snapshot_serializer serializer (&m_rtc, scene);
      if (serializer.load_json (load_path)) {
        m_output << "Scene loaded from " << load_path << "\n";
      } else {
        m_output << "Failed to load scene from '" << load_path
                 << "' (could not open file).\n";
      }
    } catch (const std::exception &e) {
      m_output << "Failed to load scene from '" << load_path
               << "': " << e.what () << "\n";
    } catch (...) {
      m_output << "Failed to load scene from '" << load_path
               << "': unknown error.\n";
    }
  } else if (action == "save") {
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }
    std::string path;
    if (tokens.size () > 2) {
      path = tokens[2];
      // Resolve relative paths against the project root
      if (m_current_project && !std::filesystem::path (path).is_absolute ()) {
        path = (std::filesystem::path (m_current_project->root_path) / path)
                   .string ();
      }
    } else {
      // Default to project's scenes_path + scene_name.wscn.json
      if (m_current_project) {
        std::filesystem::path scenes_dir (m_current_project->root_path);
        scenes_dir /= m_current_project->scenes_path;
        std::string sname
            = std::filesystem::path (scene->get_name ()).filename ().string ();
        if (sname.ends_with (".wscn.json"))
          sname.resize (sname.size () - 10);
        else if (sname.ends_with (".json"))
          sname.resize (sname.size () - 5);
        path = (scenes_dir / (sname + ".wscn.json")).string ();
      } else {
        std::string sname
            = std::filesystem::path (scene->get_name ()).filename ().string ();
        if (sname.ends_with (".wscn.json"))
          sname.resize (sname.size () - 10);
        else if (sname.ends_with (".json"))
          sname.resize (sname.size () - 5);
        path = sname + ".wscn.json";
      }
    }
    wsl::rsc::io::scene_snapshot_serializer serializer (&m_rtc, *scene);
    std::filesystem::path save_path (path);
    std::error_code ec;
    std::filesystem::create_directories (save_path.parent_path (), ec);
    if (serializer.save_json (save_path.string ())) {
      m_output << "Scene saved to " << path << "\n";
    } else
      m_output << "Failed to save scene.\n";
  } else if (action == "ls") {
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    auto assets = wsl::rsc::project_loader::scan_assets (*m_current_project);
    m_output << "Scenes in project:\n";
    for (const auto &s : assets.scenes)
      m_output << " - " << s << "\n";
  } else if (action == "status") {
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }
    auto &reg = scene->get_registry ();
    m_output << "Active Scene: " << scene->get_name () << "\n";
    size_t entity_count = reg.storage<entt::entity> ().size ();
    m_output << "Entities: " << entity_count << "\n";
    m_output << "Systems: " << scene->systems.size () << "\n";
    for (const auto &sys : scene->systems) {
      if (sys)
        m_output << "  - " << sys->get_name () << "\n";
    }
    // Count how many entities have each component type
    m_output << "Components:\n";
    auto components = m_rtc.component_registry.get_world_components ();
    for (const auto *desc : components) {
      if (!desc)
        continue;
      size_t count = 0;
      for (auto [sid, storage] : reg.storage ()) {
        entt::id_type stable = m_rtc.component_registry.to_stable_id (sid);
        if (stable == desc->type_id) {
          count = storage.size ();
          break;
        }
      }
      if (count > 0) {
        m_output << "  - " << desc->display_name << ": " << count << "\n";
      }
    }
  }
}

void
command_executor::cmd_ent (const std::vector<std::string> &tokens)
{
  auto *scene = get_active_scene ();
  if (!scene) {
    m_output << "No active scene.\n";
    return;
  }

  if (tokens.size () < 2) {
    m_output << "Usage: ent <new|ls|rm|ren|inspect> [args...]\n";
    return;
  }

  const std::string &action = tokens[1];
  if (action == "new") {
    auto &reg = scene->get_registry ();
    auto e = reg.create ();

    std::size_t name_idx = 2;
    bool empty_entity = false;

    if (tokens.size () > 2 && tokens[2] == "--empty") {
      empty_entity = true;
      name_idx = 3;
    }

    if (!empty_entity) {
      reg.emplace<wsl::comp::transform> (e);
      reg.emplace<wsl::comp::world_transform> (e);
      reg.emplace<wsl::comp::hierarchy> (e);
    }

    bool has_name = tokens.size () > name_idx;
    if (has_name)
      scene->set_entity_name (e, tokens[name_idx]);

    if (empty_entity) {
      m_output << "Entity " << (uint32_t)e;
      if (has_name)
        m_output << " (" << tokens[name_idx] << ")";
      m_output << " created (empty).\n";
    } else {
      m_output << "Entity " << (uint32_t)e;
      if (has_name)
        m_output << " (" << tokens[name_idx] << ")";
      m_output << " created with Transform, WorldTransform, Hierarchy.\n";
    }
    auto_save_scene ();
  } else if (action == "ls") {
    for (auto [e] : scene->get_registry ().storage<entt::entity> ().each ()) {
      m_output << "ID: " << (uint32_t)e
               << " | Name: " << scene->get_entity_name (e) << "\n";
    }
  } else if (action == "rm") {
    if (tokens.size () < 3)
      return;
    entt::entity e = (entt::entity)std::stoul (tokens[2]);
    scene->get_registry ().destroy (e);
    m_output << "Entity " << tokens[2] << " destroyed.\n";
    auto_save_scene ();
  } else if (action == "ren") {
    if (tokens.size () < 4)
      return;
    entt::entity e = (entt::entity)std::stoul (tokens[2]);
    scene->set_entity_name (e, tokens[3]);
    m_output << "Entity " << tokens[2] << " renamed to '" << tokens[3]
             << "'.\n";
    auto_save_scene ();
  } else if (action == "inspect") {
    if (tokens.size () < 3)
      return;
    entt::entity e = (entt::entity)std::stoul (tokens[2]);
    m_output << "Inspecting Entity " << tokens[2] << ":\n";
    m_output << " Name: " << scene->get_entity_name (e) << "\n";
    m_output << " Components:\n";
    cmd_comp ({ "comp", "ls", tokens[2] });
  }
}

void
command_executor::cmd_comp (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2) {
    m_output << "Usage: comp <ls|avail|add|rm|set> [ent_id] [args...]\n";
    return;
  }
  const std::string &action = tokens[1];

  if ((action == "ls" || action == "avail") && tokens.size () == 2) {
    ensure_runtime_module_loaded ();
    auto components = m_rtc.component_registry.get_world_components ();
    m_output << "Registered Components (" << components.size () << "):\n";
    for (const auto *component : components) {
      if (component == nullptr) {
        continue;
      }
      write_registered_entry (m_output, *component);
    }
    return;
  }

  if (action == "create") {
    if (tokens.size () < 3) {
      m_output << "Usage: comp create <name> [--source]\n";
      return;
    }
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    bool header_only = true;
    size_t name_idx = 2;
    if (tokens.size () > 3 && tokens[3] == "--source") {
      header_only = false;
    } else if (tokens.size () > 3 && tokens[2] == "--source") {
      header_only = false;
      name_idx = 3;
    }
    const std::vector<std::string> raw_tokens
        = tokenize_name (tokens[name_idx]);
    if (raw_tokens.empty ()) {
      m_output << "Invalid name: use letters or numbers.\n";
      return;
    }
    const std::string file_stem = to_snake_case (raw_tokens);
    const std::string class_name = to_pascal_case (raw_tokens);
    const std::string display_name = to_title_case (raw_tokens);
    if (class_name.empty ()
        || (std::isdigit (static_cast<unsigned char> (class_name.front ()))
            != 0)) {
      m_output << "Class name must start with a letter.\n";
      return;
    }
    std::filesystem::path base_dir
        = std::filesystem::path (m_current_project->root_path)
          / m_current_project->components_path;
    std::filesystem::path header_path = base_dir / (file_stem + ".hpp");
    std::filesystem::path source_path = base_dir / (file_stem + ".cpp");

    std::error_code ec;
    std::filesystem::create_directories (base_dir, ec);
    if (ec) {
      m_output << "Could not create target directory.\n";
      return;
    }
    if (std::filesystem::exists (header_path)
        || (!header_only && std::filesystem::exists (source_path))) {
      m_output << "File already exists: " << (file_stem + ".hpp") << "\n";
      return;
    }

    std::string header_text = make_component_header_template (
        class_name, display_name, header_only);
    if (!write_text_file (header_path, header_text)) {
      m_output << "Failed to write header.\n";
      return;
    }
    m_output << "Created: " << header_path.string () << "\n";

    if (!header_only) {
      std::string source_text = make_component_source_template (
          header_path.filename ().string (), class_name, display_name, false);
      if (!write_text_file (source_path, source_text)) {
        m_output << "Failed to write source.\n";
        return;
      }
      m_output << "Created: " << source_path.string () << "\n";
    }
    return;
  }

  auto *scene = get_active_scene ();
  if (!scene) {
    m_output << "No active scene.\n";
    return;
  }

  if (tokens.size () < 3) {
    m_output << "Usage: comp <ls|add|rm|set> <ent_id> [args...]\n";
    return;
  }

  entt::entity e = (entt::entity)std::stoul (tokens[2]);

  if (action == "ls") {
    for (auto [id, storage] : scene->get_registry ().storage ()) {
      if (storage.contains (e)) {
        if (const auto *descriptor
            = m_rtc.component_registry.find_world_component (id)) {
          write_registered_entry (m_output, *descriptor);
          // Show property values via reflection
          entt::meta_type meta = entt::resolve (descriptor->type_id);
          if (meta && storage.value (e)) {
            format_component_properties (m_output, meta, storage.value (e),
                                         "    ");
          }
        } else {
          m_output << " - Unknown Component (Hash: " << id << ")\n";
        }
      }
    }
  } else if (action == "add") {
    if (tokens.size () < 4)
      return;
    ensure_runtime_module_loaded ();
    const auto *descriptor
        = m_rtc.component_registry.find_world_component (tokens[3]);
    if (descriptor && descriptor->emplace_default) {
      if (descriptor->emplace_default (scene->get_registry (), e)) {
        m_output << "Added " << tokens[3] << " to " << tokens[2] << "\n";
        auto_save_scene ();
      } else {
        m_output << "Failed to add " << tokens[3] << " to " << tokens[2]
                 << " (already has it or entity invalid)\n";
      }
    } else {
      m_output << "Unknown component type: " << tokens[3] << "\n";
    }
  } else if (action == "rm") {
    if (tokens.size () < 4)
      return;
    const auto *descriptor
        = m_rtc.component_registry.find_world_component (tokens[3]);
    if (descriptor && descriptor->remove) {
      if (descriptor->remove (scene->get_registry (), e)) {
        m_output << "Removed " << tokens[3] << " from " << tokens[2] << "\n";
        auto_save_scene ();
      } else {
        m_output << "Failed to remove " << tokens[3] << " from " << tokens[2]
                 << " (entity doesn't have this component or entity invalid)\n";
      }
    } else {
      m_output << "Unknown component type: " << tokens[3] << "\n";
    }
  } else if (action == "set") {
    if (tokens.size () < 6) {
      m_output << "Usage: comp set <id> <type> <property> <value>\n";
      return;
    }

    const auto *descriptor
        = m_rtc.component_registry.find_world_component (tokens[3]);
    if (!descriptor) {
      m_output << "Unknown component type: " << tokens[3] << "\n";
      return;
    }

    entt::meta_type meta = entt::resolve (descriptor->type_id);
    if (!meta) {
      m_output << "No reflection metadata for " << tokens[3] << "\n";
      return;
    }

    auto &registry = scene->get_registry ();

    if (!registry.valid (e)) {
      m_output << "Invalid entity: " << tokens[2] << "\n";
      return;
    }

    if (descriptor->contains && !descriptor->contains (registry, e)) {
      m_output << "Entity " << tokens[2] << " does not have component "
               << tokens[3] << "\n";
      return;
    }

    // Find component storage and get void pointer
    void *comp_ptr = nullptr;
    for (auto [sid, storage] : registry.storage ()) {
      if (storage.contains (e)) {
        entt::id_type stable = m_rtc.component_registry.to_stable_id (sid);
        if (stable == descriptor->type_id) {
          comp_ptr = storage.value (e);
          break;
        }
      }
    }

    if (!comp_ptr) {
      m_output << "Could not locate component data for " << tokens[3]
               << " on entity " << tokens[2] << "\n";
      return;
    }

    // Split property path on '.' for nested traversal
    std::string prop_path = tokens[4];
    std::vector<std::string> segments;
    size_t start = 0, dot;
    while ((dot = prop_path.find ('.', start)) != std::string::npos) {
      segments.push_back (prop_path.substr (start, dot - start));
      start = dot + 1;
    }
    segments.push_back (prop_path.substr (start));

    // ── Nested property traversal ──
    // meta_data::get/set internally use try_cast which fails on
    // reference-typed meta_any (from from_void or field accessors).
    // Solution: for the innermost parent, create a VALUE copy, set the
    // property on the VALUE copy (try_cast works), then write the
    // entire modified copy back to the registry component via memcpy.
    // The nested path is walked using the original comp_ptr and
    // from_void; each level's data() gives the field address which we
    // store so we know where to memcpy the modified parent back.
    struct cl
    {
      void *parent_ptr;
      entt::meta_type parent_type;
      entt::id_type acc_hash;
    };
    std::vector<cl> chain;

    void *cur = comp_ptr;
    entt::meta_type cur_meta = meta;
    for (size_t i = 0; i + 1 < segments.size (); ++i) {
      entt::id_type seg_id = entt::hashed_string::value (segments[i].c_str (),
                                                         segments[i].size ());
      auto data = find_meta_data (cur_meta, segments[i]);
      if (!data) {
        m_output << "Unknown property: " << segments[i] << " in path "
                 << tokens[4] << "\n";
        return;
      }
      chain.push_back ({ cur, cur_meta, seg_id });
      entt::meta_any p = cur_meta.from_void (cur);
      if (!p) {
        m_output << "Cannot access parent at " << segments[i] << "\n";
        return;
      }
      entt::meta_any s = data.get (p);
      if (!s) {
        m_output << "Cannot access " << segments[i] << " (null or unset)\n";
        return;
      }
      void *np = const_cast<void *> (s.base ().data ());
      if (!np) {
        m_output << "Cannot resolve field " << segments[i] << "\n";
        return;
      }
      cur = np;
      cur_meta = data.type ();
    }

    // Create a VALUE copy of the innermost parent
    size_t parent_sz = cur_meta.size_of ();
    void *parent_buf = std::malloc (parent_sz);
    std::memcpy (parent_buf, cur, parent_sz);
    entt::meta_any parent_val = cur_meta.from_void (parent_buf, true);

    // Set the leaf property on the VALUE copy
    auto prop_data = find_meta_data (cur_meta, segments.back ());
    if (!prop_data) {
      m_output << "Unknown property: " << tokens[4] << "\n";
      return;
    }

    std::string set_msg;
    if (set_component_property (parent_val, prop_data, tokens[5], set_msg,
                                &m_rtc.resource_manager)) {
      // Write the modified innermost parent back to wherever cur points
      std::memcpy (cur, parent_buf, parent_sz);

      // Walk the chain in reverse to propagate changes up to the
      // original component.  Each level makes a value copy of the
      // parent, calls meta_data::set() to write the modified child
      // into the copy, then memcpy's the copy back to the original
      // parent_ptr (which points into the ECS registry storage).
      void *modified = parent_buf;
      size_t modified_sz = parent_sz;
      for (size_t j = chain.size (); j-- > 0;) {
        auto &cl = chain[j];

        size_t psz = cl.parent_type.size_of ();
        void *pcopy = std::malloc (psz);
        std::memcpy (pcopy, cl.parent_ptr, psz);
        entt::meta_any pv = cl.parent_type.from_void (pcopy, false);

        auto cd = cl.parent_type.data (cl.acc_hash);
        if (!cd)
          break;
        entt::meta_any cv = cd.type ().from_void (modified, false);
        cd.set (pv, cv);

        std::memcpy (cl.parent_ptr, pcopy, psz);
        modified = cl.parent_ptr;
        modified_sz = psz;
        std::free (pcopy);
      }

      m_output << "Set " << tokens[3] << "." << tokens[4] << " = " << tokens[5]
               << " (" << set_msg << ")\n";
      auto_save_scene ();
    } else {
      m_output << "Failed to set " << tokens[3] << "." << tokens[4] << ": "
               << set_msg << "\n";
    }
  }
}

void
command_executor::cmd_singl (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2) {
    m_output << "Usage: singl <ls|add|create|set> [args...]\n";
    return;
  }
  const std::string &action = tokens[1];

  // ── singl ls ──
  if (action == "ls") {
    ensure_runtime_module_loaded ();
    auto singletons = m_rtc.singleton_registry.get_singleton_components ();
    auto *scene = get_active_scene ();
    m_output << "Singleton Components (" << singletons.size () << "):\n";
    for (const auto *s : singletons) {
      if (!s)
        continue;
      m_output << " - " << s->display_name;
      if (!s->type_name.empty () && s->type_name != s->display_name)
        m_output << " [" << s->type_name << "]";
      if (s->core)
        m_output << " [core]";
      if (scene && s->contains (scene->get_registry ()))
        m_output << " [present]";
      else if (scene)
        m_output << " [absent]";
      m_output << "\n";

      if (scene && s->contains (scene->get_registry ())) {
        void *ptr = s->get_ptr (scene->get_registry ());
        if (ptr) {
          entt::meta_type meta = entt::resolve (s->type_id);
          if (meta)
            format_component_properties (m_output, meta, ptr, "    ");
        }
      }
    }
    return;
  }

  // ── singl add <name> ──
  if (action == "add") {
    if (tokens.size () < 3) {
      m_output << "Usage: singl add <name>\n";
      return;
    }
    ensure_runtime_module_loaded ();
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }
    const auto *desc
        = m_rtc.singleton_registry.find_singleton_component (tokens[2]);
    if (!desc) {
      m_output << "Unknown singleton component: " << tokens[2] << "\n";
      return;
    }
    if (!desc->emplace_default) {
      m_output << desc->display_name
               << " is a bound singleton and cannot be added via CLI.\n";
      return;
    }
    if (desc->contains (scene->get_registry ())) {
      m_output << desc->display_name << " is already present.\n";
      return;
    }
    desc->emplace_default (scene->get_registry ());
    m_output << "Added singleton: " << desc->display_name << "\n";
    auto_save_scene ();
    return;
  }

  // ── singl create <name> [--source] ──
  if (action == "create") {
    if (tokens.size () < 3) {
      m_output << "Usage: singl create <name> [--source]\n";
      return;
    }
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    bool header_only = true;
    size_t name_idx = 2;
    if (tokens.size () > 3 && tokens[3] == "--source") {
      header_only = false;
    } else if (tokens.size () > 3 && tokens[2] == "--source") {
      header_only = false;
      name_idx = 3;
    }
    const std::vector<std::string> raw_tokens
        = tokenize_name (tokens[name_idx]);
    if (raw_tokens.empty ()) {
      m_output << "Invalid name: use letters or numbers.\n";
      return;
    }
    const std::string file_stem = to_snake_case (raw_tokens);
    const std::string class_name = to_pascal_case (raw_tokens);
    const std::string display_name = to_title_case (raw_tokens);
    if (class_name.empty ()
        || (std::isdigit (static_cast<unsigned char> (class_name.front ()))
            != 0)) {
      m_output << "Class name must start with a letter.\n";
      return;
    }
    std::filesystem::path base_dir
        = std::filesystem::path (m_current_project->root_path)
          / m_current_project->singletons_path;
    std::filesystem::path header_path = base_dir / (file_stem + ".hpp");
    std::filesystem::path source_path = base_dir / (file_stem + ".cpp");

    std::error_code ec;
    std::filesystem::create_directories (base_dir, ec);
    if (ec) {
      m_output << "Could not create target directory.\n";
      return;
    }
    if (std::filesystem::exists (header_path)
        || (!header_only && std::filesystem::exists (source_path))) {
      m_output << "File already exists: " << (file_stem + ".hpp") << "\n";
      return;
    }

    std::string header_text = make_singleton_header_template (
        class_name, display_name, header_only);
    if (!write_text_file (header_path, header_text)) {
      m_output << "Failed to write header.\n";
      return;
    }
    m_output << "Created: " << header_path.string () << "\n";

    if (!header_only) {
      std::string source_text = make_component_source_template (
          header_path.filename ().string (), class_name, display_name, true);
      if (!write_text_file (source_path, source_text)) {
        m_output << "Failed to write source.\n";
        return;
      }
      m_output << "Created: " << source_path.string () << "\n";
    }
    return;
  }

  // ── singl set <name> <property> <value> ──
  if (action == "set") {
    if (tokens.size () < 5) {
      m_output << "Usage: singl set <name> <property> <value>\n";
      return;
    }
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }
    const auto *desc
        = m_rtc.singleton_registry.find_singleton_component (tokens[2]);
    if (!desc) {
      m_output << "Unknown singleton component: " << tokens[2] << "\n";
      return;
    }
    if (!desc->contains (scene->get_registry ())) {
      m_output << desc->display_name
               << " is not present in the active scene.\n";
      return;
    }
    void *ptr = desc->get_ptr (scene->get_registry ());
    if (!ptr) {
      m_output << "Failed to access singleton data.\n";
      return;
    }
    entt::meta_type meta = entt::resolve (desc->type_id);
    if (!meta) {
      m_output << "No reflection metadata for " << desc->display_name << "\n";
      return;
    }

    std::string prop_path = tokens[3];
    std::vector<std::string> segments;
    size_t start = 0, dot;
    while ((dot = prop_path.find ('.', start)) != std::string::npos) {
      segments.push_back (prop_path.substr (start, dot - start));
      start = dot + 1;
    }
    segments.push_back (prop_path.substr (start));

    struct cl
    {
      void *parent_ptr;
      entt::meta_type parent_type;
      entt::id_type acc_hash;
    };
    std::vector<cl> chain;

    void *cur = ptr;
    entt::meta_type cur_meta = meta;
    for (size_t i = 0; i + 1 < segments.size (); ++i) {
      auto data = find_meta_data (cur_meta, segments[i]);
      if (!data) {
        m_output << "Unknown property: " << segments[i] << " in path "
                 << prop_path << "\n";
        return;
      }
      chain.push_back ({ cur, cur_meta,
                         entt::hashed_string::value (segments[i].c_str (),
                                                     segments[i].size ()) });
      entt::meta_any p = cur_meta.from_void (cur);
      if (!p) {
        m_output << "Cannot access parent at " << segments[i] << "\n";
        return;
      }
      entt::meta_any s = data.get (p);
      if (!s) {
        m_output << "Cannot access " << segments[i] << " (null or unset)\n";
        return;
      }
      void *np = const_cast<void *> (s.base ().data ());
      if (!np) {
        m_output << "Cannot resolve field " << segments[i] << "\n";
        return;
      }
      cur = np;
      cur_meta = data.type ();
    }

    size_t parent_sz = cur_meta.size_of ();
    void *parent_buf = std::malloc (parent_sz);
    std::memcpy (parent_buf, cur, parent_sz);
    entt::meta_any parent_val = cur_meta.from_void (parent_buf, true);

    auto prop_data = find_meta_data (cur_meta, segments.back ());
    if (!prop_data) {
      m_output << "Unknown property: " << prop_path << "\n";
      std::free (parent_buf);
      return;
    }

    std::string set_msg;
    if (set_component_property (parent_val, prop_data, tokens[4], set_msg,
                                &m_rtc.resource_manager)) {
      std::memcpy (cur, parent_buf, parent_sz);

      void *modified = parent_buf;
      size_t modified_sz = parent_sz;
      for (size_t j = chain.size (); j-- > 0;) {
        auto &cl = chain[j];

        size_t psz = cl.parent_type.size_of ();
        void *pcopy = std::malloc (psz);
        std::memcpy (pcopy, cl.parent_ptr, psz);
        entt::meta_any pv = cl.parent_type.from_void (pcopy, false);

        auto cd = cl.parent_type.data (cl.acc_hash);
        if (!cd)
          break;
        entt::meta_any cv = cd.type ().from_void (modified, false);
        cd.set (pv, cv);

        std::memcpy (cl.parent_ptr, pcopy, psz);
        modified = cl.parent_ptr;
        modified_sz = psz;
        std::free (pcopy);
      }

      m_output << "Set " << desc->display_name << "." << tokens[3] << " = "
               << tokens[4] << " (" << set_msg << ")\n";
      auto_save_scene ();
    } else {
      m_output << "Failed to set " << desc->display_name << "." << tokens[3]
               << ": " << set_msg << "\n";
    }
    std::free (parent_buf);
    return;
  }

  m_output << "Usage: singl <ls|add|create|set> [args...]\n";
}

void
command_executor::cmd_sig (const std::vector<std::string> &tokens)
{
  m_output << "Signal management not yet implemented in REPL.\n";
}

void
command_executor::cmd_sys (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2) {
    m_output << "Usage: sys <ls|avail|add>\n";
    return;
  }

  if (tokens[1] == "ls") {
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }

    // Collect core systems (always present in every scene).
    std::vector<std::string> core_names;
    if (m_rtc.core_systems) {
      for (sys::ecs_system *sys : m_rtc.core_systems->to_vec ()) {
        if (sys)
          core_names.push_back (sys->get_name ());
      }
    } else {
      // Headless mode: list core system display names from the factory.
      auto all = m_rtc.system_factory_registry.get_systems ();
      for (const auto *desc : all) {
        if (desc && !desc->runtime_registered)
          core_names.push_back (desc->display_name);
      }
    }

    // Collect per-scene (user-defined) systems.
    auto user_instances = scene->get_systems ();
    std::vector<std::string> user_names;
    user_names.reserve (user_instances.size ());
    for (const auto *sys : user_instances) {
      if (sys)
        user_names.push_back (sys->get_name ());
    }

    m_output << "Scene systems (" << (core_names.size () + user_names.size ())
             << "):\n";
    for (const auto &name : core_names) {
      m_output << "  " << name << " (core)\n";
    }
    for (const auto &name : user_names) {
      m_output << "  " << name << "\n";
    }
    if (core_names.empty () && user_names.empty ()) {
      m_output << "  (none)\n";
    }
  } else if (tokens[1] == "avail") {
    ensure_runtime_module_loaded ();
    auto systems = m_rtc.system_factory_registry.get_systems ();

    std::vector<const reg::system_factory_registry::system_descriptor *>
        user_systems;
    for (const auto *system : systems) {
      if (system && system->runtime_registered) {
        user_systems.push_back (system);
      }
    }

    if (user_systems.empty ()) {
      m_output << "No user-defined system types registered.\n"
               << "  Use `sys create <name>` to create a custom system, then\n"
               << "  `sys add <name>` to attach it to the active scene.\n";
      return;
    }

    m_output << "User-defined system types (" << user_systems.size () << "):\n";
    for (const auto *system : user_systems) {
      if (system == nullptr) {
        continue;
      }
      write_registered_entry (m_output, *system);
    }
  } else if (tokens[1] == "create") {
    if (tokens.size () < 3) {
      m_output << "Usage: sys create <name> [--source]\n";
      return;
    }
    if (!m_current_project) {
      m_output << "No project loaded.\n";
      return;
    }
    bool header_only = true;
    size_t name_idx = 2;
    if (tokens.size () > 3 && tokens[3] == "--source") {
      header_only = false;
    } else if (tokens.size () > 3 && tokens[2] == "--source") {
      header_only = false;
      name_idx = 3;
    }
    std::vector<std::string> raw_tokens = tokenize_name (tokens[name_idx]);
    if (raw_tokens.empty ()) {
      m_output << "Invalid name: use letters or numbers.\n";
      return;
    }
    // Append "system" if not already present (matching editor behavior)
    if (raw_tokens.back () != "system") {
      raw_tokens.push_back ("system");
    }
    const std::string file_stem = to_snake_case (raw_tokens);
    const std::string class_name = to_pascal_case (raw_tokens);
    const std::string display_name = to_title_case (raw_tokens);
    if (class_name.empty ()
        || (std::isdigit (static_cast<unsigned char> (class_name.front ()))
            != 0)) {
      m_output << "Class name must start with a letter.\n";
      return;
    }
    std::filesystem::path base_dir
        = std::filesystem::path (m_current_project->root_path)
          / m_current_project->systems_path;
    std::filesystem::path header_path = base_dir / (file_stem + ".hpp");
    std::filesystem::path source_path = base_dir / (file_stem + ".cpp");

    std::error_code ec;
    std::filesystem::create_directories (base_dir, ec);
    if (ec) {
      m_output << "Could not create target directory.\n";
      return;
    }
    if (std::filesystem::exists (header_path)
        || (!header_only && std::filesystem::exists (source_path))) {
      m_output << "File already exists: " << (file_stem + ".hpp") << "\n";
      return;
    }

    std::string header_text
        = make_system_header_template (class_name, display_name, header_only);
    if (!write_text_file (header_path, header_text)) {
      m_output << "Failed to write header.\n";
      return;
    }
    m_output << "Created: " << header_path.string () << "\n";

    if (!header_only) {
      std::string source_text = make_system_source_template (
          header_path.filename ().string (), class_name, display_name);
      if (!write_text_file (source_path, source_text)) {
        m_output << "Failed to write source.\n";
        return;
      }
      m_output << "Created: " << source_path.string () << "\n";
    }
  } else if (tokens[1] == "add") {
    if (tokens.size () < 3) {
      m_output << "Usage: sys add <name>\n";
      return;
    }
    ensure_runtime_module_loaded ();
    std::string sys_name = tokens[2];
    for (size_t i = 3; i < tokens.size (); ++i) {
      sys_name += ' ' + tokens[i];
    }
    auto *scene = get_active_scene ();
    if (!scene) {
      m_output << "No active scene.\n";
      return;
    }
    auto *desc = m_rtc.system_factory_registry.find_system (sys_name);
    if (desc == nullptr) {
      m_output << "Unknown system: " << sys_name << "\n";
    } else if (!desc->runtime_registered) {
      m_output << "'" << sys_name
               << "' is a core engine system and is always present.\n"
               << "  Use `sys add` only for custom systems created via `sys "
                  "create`.\n";
    } else {
      auto sys = m_rtc.system_factory_registry.create (sys_name, *scene);
      if (sys) {
        scene->add_system_instance (std::move (sys));
        m_output << "Added system '" << sys_name << "' to active scene.\n";
        auto_save_scene ();
      }
    }
  } else {
    m_output << "Usage: sys <ls|avail|add>\n";
  }
}

void
command_executor::cmd_check (const std::vector<std::string> &tokens)
{
  if (tokens.size () < 2)
    return;
  m_output << "Validation placeholder for " << tokens[1] << "\n";
}

static std::string
resource_state_str (wsl::rsc::model_state s)
{
  switch (s) {
  case wsl::rsc::model_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::model_state::loading_cpu:
    return "loading_cpu";
  case wsl::rsc::model_state::preparing_gpu:
    return "preparing_gpu";
  case wsl::rsc::model_state::uploading_gpu:
    return "uploading_gpu";
  case wsl::rsc::model_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::image_state s)
{
  switch (s) {
  case wsl::rsc::image_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::image_state::loading:
    return "loading";
  case wsl::rsc::image_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::cubemap_state s)
{
  switch (s) {
  case wsl::rsc::cubemap_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::cubemap_state::loading:
    return "loading";
  case wsl::rsc::cubemap_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::scene_state s)
{
  switch (s) {
  case wsl::rsc::scene_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::scene_state::loading:
    return "loading";
  case wsl::rsc::scene_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::audio_state s)
{
  switch (s) {
  case wsl::rsc::audio_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::audio_state::loading:
    return "loading";
  case wsl::rsc::audio_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::ui_layout_state s)
{
  switch (s) {
  case wsl::rsc::ui_layout_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::ui_layout_state::loaded:
    return "loaded";
  }
  return "unknown";
}

static std::string
resource_state_str (wsl::rsc::shader_state s)
{
  switch (s) {
  case wsl::rsc::shader_state::not_loaded:
    return "not_loaded";
  case wsl::rsc::shader_state::loading:
    return "loading";
  case wsl::rsc::shader_state::loaded:
    return "loaded";
  }
  return "unknown";
}

void
command_executor::cmd_rsc (const std::vector<std::string> &tokens)
{
  auto &mgr = m_rtc.resource_manager;

  auto list_type = [&] (std::string_view label, auto &&list_fn) {
    auto items = list_fn ();
    m_output << "  " << label << " (" << items.size () << "):\n";
    for (auto &item : items) {
      m_output << "    " << item.name << "  [" << item.path << "]\n";
    }
  };

  auto list_type_with_state = [&] (std::string_view label, auto &&list_fn) {
    auto items = list_fn ();
    m_output << "  " << label << " (" << items.size () << "):\n";
    for (auto &item : items) {
      m_output << "    " << item.name << "  [" << item.path << "]  ("
               << resource_state_str (item.state) << ")\n";
    }
  };

  auto show_info = [&] (const std::string &label, auto id, auto &&info_fn) {
    auto item = info_fn (id);
    if (item) {
      m_output << "Type: " << label << "\n";
      m_output << "  Name: " << item->name << "\n";
      m_output << "  Path: " << item->path << "\n";
      if constexpr (requires { item->state; }) {
        m_output << "  State: " << resource_state_str (item->state) << "\n";
      }
    } else {
      m_output << label << " not found.\n";
    }
  };

  // ── Resolve a name or path to a resource ID ──
  auto find_model = [&] (const std::string &input) -> std::optional<model_id> {
    if (auto id = mgr.find_model_by_path (input))
      return id;
    for (auto &info : mgr.list_models ())
      if (info.name == input)
        return model_id{ info.id };
    return std::nullopt;
  };

  auto find_image = [&] (const std::string &input) -> std::optional<image_id> {
    image_id id{ entt::hashed_string::value (input.c_str (), input.size ()) };
    if (mgr.contains (id))
      return id;
    for (auto &info : mgr.list_images ())
      if (info.name == input)
        return image_id{ info.id };
    return std::nullopt;
  };

  auto find_cubemap
      = [&] (const std::string &input) -> std::optional<cubemap_id> {
    cubemap_id id{ entt::hashed_string::value (input.c_str (), input.size ()) };
    if (mgr.contains (id))
      return id;
    for (auto &info : mgr.list_cubemaps ())
      if (info.name == input)
        return cubemap_id{ info.id };
    return std::nullopt;
  };

  auto find_scene = [&] (const std::string &input) -> std::optional<scene_id> {
    scene_id id{ entt::hashed_string::value (input.c_str (), input.size ()) };
    if (mgr.contains (id))
      return id;
    for (auto &info : mgr.list_scenes ())
      if (info.name == input)
        return scene_id{ info.id };
    return std::nullopt;
  };

  auto find_audio = [&] (const std::string &input) -> std::optional<audio_id> {
    audio_id id{ entt::hashed_string::value (input.c_str (), input.size ()) };
    if (mgr.contains (id))
      return id;
    for (auto &info : mgr.list_audio ())
      if (info.name == input)
        return audio_id{ info.id };
    return std::nullopt;
  };

  auto find_shader
      = [&] (const std::string &input) -> std::optional<shader_id> {
    shader_id id{ entt::hashed_string::value (input.c_str (), input.size ()) };
    if (mgr.contains (id))
      return id;
    for (auto &info : mgr.list_shaders ())
      if (info.name == input)
        return shader_id{ info.id };
    return std::nullopt;
  };

  auto find_layout
      = [&] (const std::string &input) -> std::optional<ui_layout_id> {
    for (auto &info : mgr.list_ui_layouts ())
      if (info.name == input)
        return ui_layout_id{ info.id };
    return std::nullopt;
  };

  auto find_font = [&] (const std::string &input) -> std::optional<font_id> {
    for (auto &info : mgr.list_fonts ())
      if (info.name == input)
        return font_id{ info.id };
    return std::nullopt;
  };

  if (tokens.size () < 2 || tokens[1] == "ls") {
    if (tokens.size () < 3) {
      m_output << "Registered resources:\n";
      list_type_with_state ("models", [&] { return mgr.list_models (); });
      list_type_with_state ("images", [&] { return mgr.list_images (); });
      list_type_with_state ("cubemaps", [&] { return mgr.list_cubemaps (); });
      list_type_with_state ("scenes", [&] { return mgr.list_scenes (); });
      list_type_with_state ("audio", [&] { return mgr.list_audio (); });
      list_type_with_state ("layouts", [&] { return mgr.list_ui_layouts (); });
      list_type ("fonts", [&] { return mgr.list_fonts (); });
      list_type_with_state ("shaders", [&] { return mgr.list_shaders (); });
    } else {
      const std::string &type = tokens[2];
      if (type == "models" || type == "model")
        list_type_with_state ("models", [&] { return mgr.list_models (); });
      else if (type == "images" || type == "image")
        list_type_with_state ("images", [&] { return mgr.list_images (); });
      else if (type == "cubemaps" || type == "cubemap")
        list_type_with_state ("cubemaps", [&] { return mgr.list_cubemaps (); });
      else if (type == "scenes" || type == "scene")
        list_type_with_state ("scenes", [&] { return mgr.list_scenes (); });
      else if (type == "audio" || type == "audios")
        list_type_with_state ("audio", [&] { return mgr.list_audio (); });
      else if (type == "layouts" || type == "layout")
        list_type_with_state ("layouts",
                              [&] { return mgr.list_ui_layouts (); });
      else if (type == "fonts" || type == "font")
        list_type ("fonts", [&] { return mgr.list_fonts (); });
      else if (type == "shaders" || type == "shader")
        list_type_with_state ("shaders", [&] { return mgr.list_shaders (); });
      else
        m_output << "Unknown resource type: " << type << "\n";
    }
    return;
  }

  const std::string &action = tokens[1];

  // ── rsc add <type> <path> [--load] ──
  if (action == "add") {
    if (tokens.size () < 4) {
      m_output << "Usage: rsc add <type> <path> [--load]\n";
      return;
    }
    const std::string &type = tokens[2];
    const std::string &path = tokens[3];
    bool do_load = tokens.size () > 4 && tokens[4] == "--load";

    auto add_and_maybe_load
        = [&] (auto id, auto register_fn, auto load_fn, const char *label) {
            id = register_fn (path);
            m_output << "Registered " << label << ": " << path << "\n";
            if (do_load) {
              load_fn (id);
              m_output << "Loaded " << label << ": " << path << "\n";
            }
          };

    if (type == "model" || type == "models") {
      add_and_maybe_load (
          model_id{},
          [&] (const std::string &p) { return mgr.register_model (p); },
          [&] (model_id id) { mgr.load (id); }, "model");
    } else if (type == "image" || type == "images") {
      add_and_maybe_load (
          image_id{},
          [&] (const std::string &p) { return mgr.register_image (p); },
          [&] (image_id id) { mgr.load (id); }, "image");
    } else if (type == "cubemap" || type == "cubemaps") {
      add_and_maybe_load (
          cubemap_id{},
          [&] (const std::string &p) { return mgr.register_cubemap (p); },
          [&] (cubemap_id id) { mgr.load (id); }, "cubemap");
    } else if (type == "scene" || type == "scenes") {
      add_and_maybe_load (
          scene_id{},
          [&] (const std::string &p) { return mgr.register_scene (p); },
          [&] (scene_id id) { mgr.load (id); }, "scene");
    } else if (type == "audio" || type == "audios") {
      add_and_maybe_load (
          audio_id{},
          [&] (const std::string &p) { return mgr.register_audio (p); },
          [&] (audio_id id) { mgr.load (id); }, "audio");
    } else if (type == "font" || type == "fonts") {
      mgr.register_font (path);
      m_output << "Registered font: " << path << "\n";
    } else if (type == "shader" || type == "shaders") {
      mgr.register_shader (path);
      m_output << "Registered shader: " << path << "\n";
      if (do_load) {
        shader_id id{ entt::hashed_string::value (path.c_str (),
                                                  path.size ()) };
        mgr.load (id);
        m_output << "Loaded shader: " << path << "\n";
      }
    } else if (type == "layout" || type == "layouts") {
      mgr.register_ui_layout (path);
      m_output << "Registered UI layout: " << path << "\n";
    } else {
      m_output << "Unknown resource type: " << type << "\n";
    }
    return;
  }

  // ── rsc rm <type> <name/path> ──
  if (action == "rm") {
    if (tokens.size () < 4) {
      m_output << "Usage: rsc rm <type> <name|path>\n";
      return;
    }
    const std::string &type = tokens[2];
    const std::string &name = tokens[3];

    auto do_rm = [&] (auto find_fn, const char *label) {
      auto id = find_fn (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Removed " << label << ": " << name << "\n";
      } else {
        m_output << label << " not found: " << name << "\n";
      }
    };

    if (type == "model" || type == "models")
      do_rm (find_model, "Model");
    else if (type == "image" || type == "images")
      do_rm (find_image, "Image");
    else if (type == "cubemap" || type == "cubemaps")
      do_rm (find_cubemap, "Cubemap");
    else if (type == "scene" || type == "scenes")
      do_rm (find_scene, "Scene");
    else if (type == "audio" || type == "audios")
      do_rm (find_audio, "Audio");
    else if (type == "font" || type == "fonts")
      m_output << "Fonts are metadata-only and cannot be removed.\n";
    else if (type == "shader" || type == "shaders")
      do_rm (find_shader, "Shader");
    else if (type == "layout" || type == "layouts")
      m_output << "UI layouts are metadata-only and cannot be removed.\n";
    else
      m_output << "Unknown resource type: " << type << "\n";
    return;
  }

  // ── rsc load <type> <name/path> ──
  if (action == "load") {
    if (tokens.size () < 4) {
      m_output << "Usage: rsc load <type> <name|path>\n";
      return;
    }
    const std::string &type = tokens[2];
    const std::string &name = tokens[3];

    if (type == "model" || type == "models") {
      auto id = find_model (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded model: " << name << "\n";
      } else {
        m_output << "Model not found: " << name << "\n";
      }
    } else if (type == "image" || type == "images") {
      auto id = find_image (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded image: " << name << "\n";
      } else {
        m_output << "Image not found: " << name << "\n";
      }
    } else if (type == "cubemap" || type == "cubemaps") {
      auto id = find_cubemap (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded cubemap: " << name << "\n";
      } else {
        m_output << "Cubemap not found: " << name << "\n";
      }
    } else if (type == "scene" || type == "scenes") {
      auto id = find_scene (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded scene: " << name << "\n";
      } else {
        m_output << "Scene not found: " << name << "\n";
      }
    } else if (type == "audio" || type == "audios") {
      auto id = find_audio (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded audio: " << name << "\n";
      } else {
        m_output << "Audio not found: " << name << "\n";
      }
    } else if (type == "font" || type == "fonts") {
      m_output << "Fonts are metadata-only and cannot be loaded.\n";
    } else if (type == "shader" || type == "shaders") {
      auto id = find_shader (name);
      if (id) {
        mgr.load (*id);
        m_output << "Loaded shader: " << name << "\n";
      } else {
        m_output << "Shader not found: " << name << "\n";
      }
    } else if (type == "layout" || type == "layouts") {
      m_output << "UI layouts are metadata-only and cannot be loaded.\n";
    } else {
      m_output << "Unknown resource type: " << type << "\n";
    }
    return;
  }

  // ── rsc unload <type> <name/path> ──
  if (action == "unload") {
    if (tokens.size () < 4) {
      m_output << "Usage: rsc unload <type> <name|path>\n";
      return;
    }
    const std::string &type = tokens[2];
    const std::string &name = tokens[3];

    if (type == "model" || type == "models") {
      auto id = find_model (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded model: " << name << "\n";
      } else {
        m_output << "Model not found: " << name << "\n";
      }
    } else if (type == "image" || type == "images") {
      auto id = find_image (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded image: " << name << "\n";
      } else {
        m_output << "Image not found: " << name << "\n";
      }
    } else if (type == "cubemap" || type == "cubemaps") {
      auto id = find_cubemap (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded cubemap: " << name << "\n";
      } else {
        m_output << "Cubemap not found: " << name << "\n";
      }
    } else if (type == "scene" || type == "scenes") {
      auto id = find_scene (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded scene: " << name << "\n";
      } else {
        m_output << "Scene not found: " << name << "\n";
      }
    } else if (type == "audio" || type == "audios") {
      auto id = find_audio (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded audio: " << name << "\n";
      } else {
        m_output << "Audio not found: " << name << "\n";
      }
    } else if (type == "font" || type == "fonts") {
      m_output << "Fonts are metadata-only and cannot be unloaded.\n";
    } else if (type == "shader" || type == "shaders") {
      auto id = find_shader (name);
      if (id) {
        mgr.unload (*id);
        m_output << "Unloaded shader: " << name << "\n";
      } else {
        m_output << "Shader not found: " << name << "\n";
      }
    } else if (type == "layout" || type == "layouts") {
      m_output << "UI layouts are metadata-only and cannot be unloaded.\n";
    } else {
      m_output << "Unknown resource type: " << type << "\n";
    }
    return;
  }

  // ── rsc info <type> <name/path> ──
  if (action == "info") {
    if (tokens.size () < 4) {
      m_output << "Usage: rsc info <type> <name|path>\n";
      return;
    }
    const std::string &type = tokens[2];
    const std::string &name = tokens[3];

    if (type == "model" || type == "models") {
      auto id = find_model (name);
      if (id)
        show_info ("model", *id, [&] (model_id i) { return mgr.info (i); });
      else
        m_output << "Model not found: " << name << "\n";
    } else if (type == "image" || type == "images") {
      auto id = find_image (name);
      if (id)
        show_info ("image", *id, [&] (image_id i) { return mgr.info (i); });
      else
        m_output << "Image not found: " << name << "\n";
    } else if (type == "cubemap" || type == "cubemaps") {
      auto id = find_cubemap (name);
      if (id)
        show_info ("cubemap", *id, [&] (cubemap_id i) { return mgr.info (i); });
      else
        m_output << "Cubemap not found: " << name << "\n";
    } else if (type == "scene" || type == "scenes") {
      auto id = find_scene (name);
      if (id)
        show_info ("scene", *id, [&] (scene_id i) { return mgr.info (i); });
      else
        m_output << "Scene not found: " << name << "\n";
    } else if (type == "audio" || type == "audios") {
      auto id = find_audio (name);
      if (id)
        show_info ("audio", *id, [&] (audio_id i) { return mgr.info (i); });
      else
        m_output << "Audio not found: " << name << "\n";
    } else if (type == "font" || type == "fonts") {
      auto id = find_font (name);
      if (id)
        show_info ("font", *id, [&] (font_id i) { return mgr.info (i); });
      else
        m_output << "Font not found: " << name << "\n";
    } else if (type == "shader" || type == "shaders") {
      auto id = find_shader (name);
      if (id)
        show_info ("shader", *id, [&] (shader_id i) { return mgr.info (i); });
      else
        m_output << "Shader not found: " << name << "\n";
    } else if (type == "layout" || type == "layouts") {
      auto id = find_layout (name);
      if (id)
        show_info ("layout", *id,
                   [&] (ui_layout_id i) { return mgr.info (i); });
      else
        m_output << "Layout not found: " << name << "\n";
    } else {
      m_output << "Unknown resource type: " << type << "\n";
    }
    return;
  }

  m_output << "Usage: rsc <ls|add|rm|load|unload|info> [args...]\n";
}

void
command_executor::cmd_help ()
{
  m_output
      << "Weasel Engine REPL - Available Commands\n"
      << "======================================\n\n"
      << "Save policy:\n"
      << "  One-shot mode (no -i/-a): changes auto-save to disk after each\n"
      << "  mutation command. Interactive/attach mode: save manually via\n"
      << "  scene save / proj save.\n\n"
      << "Project:\n"
      << "  proj new <path> <name>     Create a new project at path with name\n"
      << "  proj load <path>           Load an existing project\n"
      << "  proj info                  Show current project metadata\n"
      << "  proj set <field> <value>   Set project metadata (name, author, "
         "default_scene_path)\n"
      << "  proj save                  Save current project metadata to "
         "disk\n\n"
      << "Scene:\n"
      << "  scene new <name>           Create a new empty scene and set it "
         "active\n"
      << "  scene load <path|name>     Load a .wscn.json scene (path or scene "
         "name)\n"
      << "  scene save [path]          Save active scene (default: project "
         "scenes dir)\n"
      << "  scene ls                   List all scene assets in the project\n"
      << "  scene status               Show active scene statistics\n\n"
      << "Entity:\n"
      << "  ent new [name]             Create a new entity (optional name)\n"
      << "  ent ls                     List all entities in the active scene\n"
      << "  ent rm <id>                Destroy entity by numeric ID\n"
      << "  ent ren <id> <name>        Rename an entity\n"
      << "  ent inspect <id>           Show entity details and its "
         "components\n\n"
      << "Component:\n"
      << "  comp avail                 List all registered component types\n"
      << "  comp ls [id]               List components on an entity, or all "
         "types\n"
      << "  comp add <id> <type>       Add a component to entity by type name\n"
      << "  comp rm <id> <type>        Remove a component from entity\n"
      << "  comp set <id> <type> <prop> <val>   Set a component property\n"
      << "  comp create <name> [--source]  Generate a world component "
         "template\n"
      << "    Type name: short name (transform), display name (Transform),\n"
      << "              or fully qualified (wsl::comp::transform)\n"
      << "    Value is JSON: numbers, strings, booleans, arrays, objects\n"
      << "    Enum names: box/sphere, Static/Kinematic/Dynamic, etc.\n"
      << "    Resource paths: builtin://cube, builtin://sphere, etc.\n"
      << "    Nested paths: motion_type.value, collision_layer.value\n"
      << "    Examples:\n"
      << "      comp set 42 transform position '[1,2,3]'\n"
      << "      comp set 42 rigid_body shape sphere\n"
      << "      comp set 42 rigid_body radius 0.5\n"
      << "      comp set 42 rigid_body motion_type.value 2\n"
      << "      comp set 42 model_instance_3d model_id builtin://cube\n\n"
      << "Singleton:\n"
      << "  singl ls                   List singleton components in the scene\n"
      << "  singl add <name>           Add a value-owned singleton to the "
         "active scene\n"
      << "  singl create <name> [--source]  Generate a singleton template\n"
      << "  singl set <name> <prop> <val>   Set a singleton property\n\n"
      << "System:\n"
      << "  sys ls                     List all systems in the active scene\n"
      << "                             (core + user-defined)\n"
      << "  sys avail                  List user-defined system types "
         "available\n"
      << "                             via 'sys add'\n"
      << "  sys add <name>             Add a user-defined system to the scene\n"
      << "                             (core engine systems are always "
         "present)\n"
      << "  sys create <name> [--source]  Generate a system template\n\n"
      << "Resources:\n"
      << "  rsc ls [type]              List registered resources\n"
      << "  rsc add <type> <path> [--load]  Register a resource (name = "
         "filename without extension)\n"
      << "  rsc rm <type> <name>       Remove a registered resource\n"
      << "  rsc load <type> <name>     Load a registered resource\n"
      << "  rsc unload <type> <name>   Unload a registered resource\n"
      << "  rsc info <type> <name>     Show resource metadata\n"
      << "    Types: model, image, cubemap, scene, audio, font, shader, "
         "layout\n\n"
      << "Other:\n"
      << "  help                       Show this help message\n"
      << "  cls                        Clear the terminal screen\n"
      << "  exit | quit                Exit the REPL\n"
      << "  # ...                      Lines starting with # are ignored "
         "(comments)\n";
}

// -------- repl_handler implementation --------

repl_handler::repl_handler (const std::string &engine_res_path, bool attach)
    : m_engine_res_path (engine_res_path), m_attach (attach)
{
}

void
repl_handler::set_auto_save (bool enabled)
{
  ensure_local_executor ();
  if (m_local_executor) {
    m_local_executor->set_auto_save (enabled);
  }
}

void
repl_handler::ensure_local_executor ()
{
  if (m_attach || m_local_executor) {
    return;
  }

  m_rtc = std::make_unique<wsl::comp::singl::runtime_context> (
      "Weasel REPL", 0, 0, m_engine_res_path, true);
  m_rtc->set_editor_ctx (nullptr);
  register_repl_types (*m_rtc);
  m_local_executor = std::make_unique<command_executor> (*m_rtc);
}

bool
repl_handler::prepare (std::optional<std::string> initial_project,
                       std::optional<std::string> initial_scene)
{
  if (m_attach) {
    if (!m_editor_client.is_connected ()) {
      if (!initial_project) {
        std::cerr << "Error: --attach requires --project to be specified.\n";
        return false;
      }

      std::filesystem::path proj_path (*initial_project);
      wsl::log::cli ()->info ("Initial project path: {}", proj_path.string ());
      if (proj_path.filename ().string () == "wslpro.json") {
        proj_path = proj_path.parent_path ();
        wsl::log::cli ()->info ("Detected wslpro.json, using parent: {}",
                                proj_path.string ());
      }

      std::string abs_project_path
          = std::filesystem::weakly_canonical (proj_path).string ();
      wsl::log::cli ()->info ("Attaching to project at: {}", abs_project_path);
      if (!m_editor_client.connect (abs_project_path)) {
        wsl::log::cli ()->error (
            "Failed to connect to editor server for project: {}",
            *initial_project);
        wsl::log::cli ()->error (
            "Make sure the editor is running with the same project loaded");
        return false;
      }
      wsl::log::cli ()->info ("Connected to editor server for project: {}",
                              *initial_project);
    }

    if (initial_scene) {
      auto response = m_editor_client.execute_command (
          build_repl_command ({ "scene", "load", *initial_scene }));
      if (!response || command_failed (*response)) {
        if (response && !response->empty ()) {
          wsl::log::cli ()->error ("{}", *response);
          if (response->back () != '\n') {
            std::cerr << '\n';
          }
        }
        return false;
      }
    }

    return true;
  }

  ensure_local_executor ();
  if (initial_project) {
    std::string const output = m_local_executor->execute (
        build_repl_command ({ "proj", "load", *initial_project }));
    if (command_failed (output)) {
      std::cerr << output;
      return false;
    }
  }
  if (initial_scene) {
    std::string const output = m_local_executor->execute (
        build_repl_command ({ "scene", "load", *initial_scene }));
    if (command_failed (output)) {
      std::cerr << output;
      return false;
    }
  }

  return true;
}

void
repl_handler::run (std::optional<std::string> initial_project,
                   std::optional<std::string> initial_scene)
{
  wsl::log::cli ()->trace ("Run called, m_attach={}", m_attach);
  if (!prepare (initial_project, initial_scene)) {
    return;
  }

  std::string line;
  wsl::log::cli ()->info ("Weasel Engine REPL\nType 'help' for commands.");
  while (m_running && std::cout << "wsl> " && std::getline (std::cin, line)) {
    if (line.empty ())
      continue;
    execute_command (line);
  }
}

static bool
is_exit_command (const std::string &line)
{
  // Tokenize the first word to detect exit/quit
  std::size_t end = 0;
  while (end < line.size ()
         && !std::isspace (static_cast<unsigned char> (line[end])))
    ++end;
  std::string first = line.substr (0, end);
  return first == "exit" || first == "quit";
}

void
repl_handler::execute_command (const std::string &line)
{
  // Check for exit/quit before doing anything else
  if (is_exit_command (line)) {
    m_running = false;
    return;
  }

  // If attached to editor server, forward command remotely
  if (m_attach && m_editor_client.is_connected ()) {
    auto response = m_editor_client.execute_command (line);
    if (response) {
      wsl::log::cli ()->info ("{}", *response);
    } else {
      wsl::log::cli ()->error ("Lost connection to editor server");
      m_running = false;
    }
    return;
  }

  // Otherwise, execute locally
  ensure_local_executor ();
  if (m_local_executor) {
    std::string output = m_local_executor->execute (line);
    wsl::log::cli ()->info ("{}", output);
  }
}

} // namespace wsl::cli
