#include "file_list.hpp"

#include "text_editor.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/rsc/resource_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imsearch.h>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace editor
{
namespace
{

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
    if (i != 0) {
      out += '_';
    }
    out += tokens[i];
  }
  return out;
}

std::string
to_pascal_case (const std::vector<std::string> &tokens)
{
  std::string out;
  for (const std::string &token : tokens) {
    if (token.empty ()) {
      continue;
    }

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
    if (i != 0) {
      out += ' ';
    }

    std::string piece = tokens[i];
    if (!piece.empty ()) {
      piece[0] = (char)std::toupper (static_cast<unsigned char> (piece[0]));
    }
    out += piece;
  }
  return out;
}

bool
write_text_file (const std::filesystem::path &path, std::string_view text)
{
  std::ofstream output (path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }

  output.write (text.data (), (std::streamsize)text.size ());
  return output.good ();
}

std::string
make_component_header_template (const std::string &class_name, bool header_only)
{
  std::ostringstream output;
  output << "#pragma once\n\n";
  output << "#include \"wsl/comp/component_meta.hpp\"\n";
  output << "#include \"wsl/reg/runtime_project_module_api.hpp\"\n";
  output << "#include <cereal/cereal.hpp>\n\n";
  output << "namespace wsl::comp\n{\n\n";
  output << "struct " << class_name << " : world_component {\n";
  output << "  float value = 1.0f;\n\n";
  output << "  static void register_meta();\n\n";
  output << "  template <class Archive> void serialize(Archive &archive) {\n";
  output << "    archive(cereal::make_nvp(\"value\", value));\n";
  output << "  }\n";
  output << "};\n\n";
  output << "} // namespace wsl::comp\n";

  if (header_only) {
    output << "\nWEASEL_RUNTIME_COMPONENT(wsl::comp::" << class_name << ")\n";
  }

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
  output << "      = wsl::comp::reflect_type<wsl::comp::" << class_name << ">(\n";
  output << "            entt::type_hash<wsl::comp::" << class_name << ">::value(),\n";
  output << "            \"" << display_name << "\",\n";
  output << "            \"Describe what this " << (is_singleton ? "singleton" : "component") << " stores.\");\n\n";
  output << "  wsl::comp::reflect_field<wsl::comp::" << class_name << ",\n";
  output << "                           &wsl::comp::" << class_name << "::value>(\n";
  output << "      factory, \"value\", {}, \"Example editable field.\");\n";
  output << "}\n\n";

  if (is_singleton) {
    output << "WEASEL_RUNTIME_SINGLETON(wsl::comp::" << class_name << ", \"" << display_name << "\")\n";
  } else {
    output << "WEASEL_RUNTIME_COMPONENT(wsl::comp::" << class_name << ")\n";
  }

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
  output << "namespace wsl::comp\n{\n\n";
  output << "struct " << class_name << " : singleton_component {\n";
  output << "  float value = 1.0f;\n\n";
  output << "  static void register_meta();\n\n";
  output << "  template <class Archive> void serialize(Archive &archive) {\n";
  output << "    archive(cereal::make_nvp(\"value\", value));\n";
  output << "  }\n";
  output << "};\n\n";
  output << "} // namespace wsl::comp\n";

  if (header_only) {
    output << "\nWEASEL_RUNTIME_SINGLETON(wsl::comp::" << class_name << ", \""
           << display_name << "\")\n";
  }

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
  output << "  void register_signals(wsl::reg::sig::signal_hub &hub) override {}\n";
  output << "  void register_event_handlers(wsl::reg::sig::signal_hub &hub) override {}\n";
  output << "  void register_iterations(wsl::reg::sig::signal_hub &hub) override {}\n";
  output << "};\n\n";
  output << "} // namespace wsl::sys\n";

  if (header_only) {
    output << "\nWEASEL_RUNTIME_SYSTEM(wsl::sys::" << class_name << ", \"" << display_name
           << "\")\n";
  }

  return output.str ();
}

std::string
make_system_source_template (const std::string &header_name,
                             const std::string &class_name,
                             const std::string &display_name)
{
  std::ostringstream output;
  output << "#include \"" << header_name << "\"\n\n";
  output << "WEASEL_RUNTIME_SYSTEM(wsl::sys::" << class_name << ", \"" << display_name << "\")\n";
  return output.str ();
}

bool
select_entry_by_path (const std::vector<file_list::entry> &entries,
                      const std::string &path, int &out_idx)
{
  for (int i = 0; i < (int)entries.size (); ++i) {
    if (entries[i].path == path) {
      out_idx = i;
      return true;
    }
  }
  return false;
}

} // namespace

void
file_list::gather_cpp_hpp (const std::filesystem::path &base,
                           std::vector<entry> &out)
{
  gather_files_with_extensions (base, { ".hpp", ".cpp", ".h", ".cc", ".cxx" },
                                out);
}

void
file_list::gather_files_with_extensions (const std::filesystem::path &base,
                                         const std::vector<std::string> &exts,
                                         std::vector<entry> &out)
{
  out.clear ();
  if (base.empty () || !std::filesystem::exists (base)) {
    return;
  }

  for (const auto &entry_it :
       std::filesystem::recursive_directory_iterator (base)) {
    if (!entry_it.is_regular_file ()) {
      continue;
    }

    const auto &path = entry_it.path ();
    std::string const ext = path.extension ().string ();
    bool match = false;
    for (const auto &e : exts) {
      if (ext == e) {
        match = true;
        break;
      }
    }

    if (match) {
      out.push_back (
          { .label = std::filesystem::relative (path, base).string (),
            .path = std::filesystem::weakly_canonical (path).string () });
    }
  }

  std::sort (out.begin (), out.end (), [] (const entry &a, const entry &b) {
    return a.label < b.label;
  });
}

void
file_list::refresh_if_needed (wsl::rsc::resource_manager *resources)
{
  if (resources == nullptr) {
    return;
  }

  auto proj = resources->current_project ();
  if (!proj) {
    last_project_root.clear ();
    last_components_abs.clear ();
    last_singletons_abs.clear ();
    last_systems_abs.clear ();
    last_ui_layouts_abs.clear ();
    last_shaders_abs.clear ();
    component_files.clear ();
    singleton_files.clear ();
    system_files.clear ();
    ui_layout_files.clear ();
    shader_files.clear ();
    selected_comp = -1;
    selected_singleton = -1;
    selected_sys = -1;
    selected_ui = -1;
    selected_shader = -1;
    needs_refresh = true;
    return;
  }

  const std::filesystem::path root = proj->root_path;
  const std::filesystem::path comp = root / proj->components_path;
  const std::filesystem::path singl = root / proj->singletons_path;
  const std::filesystem::path sys = root / proj->systems_path;
  const std::filesystem::path ui = root / proj->ui_layouts_path;
  const std::filesystem::path shaders = root / proj->shaders_path;

  const auto root_s = std::filesystem::weakly_canonical (root).string ();
  const auto comp_s = std::filesystem::weakly_canonical (comp).string ();
  const auto singl_s = std::filesystem::weakly_canonical (singl).string ();
  const auto sys_s = std::filesystem::weakly_canonical (sys).string ();
  const auto ui_s = std::filesystem::weakly_canonical (ui).string ();
  const auto shaders_s = std::filesystem::weakly_canonical (shaders).string ();

  if (!needs_refresh && root_s == last_project_root
      && comp_s == last_components_abs && singl_s == last_singletons_abs
      && sys_s == last_systems_abs && ui_s == last_ui_layouts_abs
      && shaders_s == last_shaders_abs) {
    return;
  }

  last_project_root = root_s;
  last_components_abs = comp_s;
  last_singletons_abs = singl_s;
  last_systems_abs = sys_s;
  last_ui_layouts_abs = ui_s;
  last_shaders_abs = shaders_s;

  gather_cpp_hpp (comp, component_files);
  gather_cpp_hpp (singl, singleton_files);
  gather_cpp_hpp (sys, system_files);
  gather_files_with_extensions (ui, { ".rml", ".rcss" }, ui_layout_files);
  gather_files_with_extensions (shaders, { ".hlsl", ".spv", ".dxil", ".metal", ".vert", ".frag" }, shader_files);

  if (!pending_open_path.empty ()) {
    if (select_entry_by_path (component_files, pending_open_path,
                              selected_comp)) {
      selected_sys = -1;
      selected_singleton = -1;
      selected_ui = -1;
    } else if (select_entry_by_path (singleton_files, pending_open_path,
                                     selected_singleton)) {
      selected_comp = -1;
      selected_sys = -1;
      selected_ui = -1;
    } else if (select_entry_by_path (system_files, pending_open_path,
                                     selected_sys)) {
      selected_comp = -1;
      selected_singleton = -1;
      selected_ui = -1;
    } else if (select_entry_by_path (ui_layout_files, pending_open_path,
                                     selected_ui)) {
      selected_comp = -1;
      selected_singleton = -1;
      selected_sys = -1;
      selected_shader = -1;
    } else if (select_entry_by_path (shader_files, pending_open_path,
                                     selected_shader)) {
      selected_comp = -1;
      selected_singleton = -1;
      selected_sys = -1;
      selected_ui = -1;
    }
    pending_open_path.clear ();
  } else {
    selected_comp = -1;
    selected_singleton = -1;
    selected_sys = -1;
    selected_ui = -1;
    selected_shader = -1;
  }

  needs_refresh = false;
}

void
file_list::queue_create_popup (create_kind kind, bool header_only)
{
  pending_create_kind = kind;
  create_header_only = header_only;
  create_name[0] = '\0';
  create_error.clear ();
  request_create_popup_open = true;
  request_name_focus = true;
}

void
file_list::draw_create_popup (wsl::rsc::resource_manager *resources,
                              editor::text_editor *editor, bool *show_editor)
{
  if (request_create_popup_open) {
    ImGui::OpenPopup ("Create Runtime Script");
    request_create_popup_open = false;
  }

  auto proj = (resources != nullptr) ? resources->current_project () : nullptr;
  const bool is_system = pending_create_kind == create_kind::system;
  const bool is_singleton = pending_create_kind == create_kind::singleton;
  const char *kind_label = is_system ? "system" : (is_singleton ? "singleton" : "component");

  if (!ImGui::BeginPopupModal ("Create Runtime Script", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::Text ("Create a new runtime %s (%s).", kind_label,
               create_header_only ? "Header Only" : "Header and Source");
  if (proj) {
    const std::filesystem::path base_dir
        = std::filesystem::path (proj->root_path)
          / (is_system ? proj->systems_path : (is_singleton ? proj->singletons_path : proj->components_path));
    const std::string base_dir_str = base_dir.lexically_normal ().string ();
    ImGui::TextDisabled ("%s", base_dir_str.c_str ());
  }

  if (request_name_focus) {
    ImGui::SetKeyboardFocusHere ();
    request_name_focus = false;
  }

  const bool submit_with_enter = ImGui::InputTextWithHint (
      "##RuntimeName", "e.g. health or movement", create_name,
      IM_ARRAYSIZE (create_name), ImGuiInputTextFlags_EnterReturnsTrue);

  if (!create_error.empty ()) {
    ImGui::Spacing ();
    ImGui::TextColored (ImVec4 (0.92F, 0.36F, 0.36F, 1.0F), "%s",
                        create_error.c_str ());
  }

  bool should_create = submit_with_enter;

  if (ImGui::Button ("Create")) {
    should_create = true;
  }
  ImGui::SameLine ();
  if (ImGui::Button ("Cancel")) {
    create_error.clear ();
    ImGui::CloseCurrentPopup ();
  }

  if (should_create) {
    if (!proj) {
      create_error = "Open a project before creating runtime files.";
    } else {
      const std::vector<std::string> raw_tokens = tokenize_name (create_name);
      if (raw_tokens.empty ()) {
        create_error = "Enter a valid name using letters or numbers.";
      } else {
        std::vector<std::string> tokens = raw_tokens;
        if (is_system && tokens.back () != "system") {
          tokens.push_back ("system");
        }

        const std::string file_stem = to_snake_case (tokens);
        const std::string class_name = to_pascal_case (tokens);
        const std::string display_name = to_title_case (tokens);

        if (class_name.empty ()
            || (std::isdigit (
                static_cast<unsigned char> (class_name.front ())) != 0)) {
          create_error = "The generated class name must start with a letter.";
        } else {
          const std::filesystem::path base_dir
              = std::filesystem::path (proj->root_path)
                / (is_system ? proj->systems_path : (is_singleton ? proj->singletons_path : proj->components_path));
          const std::filesystem::path header_path
              = base_dir / (file_stem + ".hpp");
          const std::filesystem::path source_path
              = base_dir / (file_stem + ".cpp");

          std::error_code ec;
          std::filesystem::create_directories (base_dir, ec);
          if (ec) {
            create_error = "Could not create the target folder.";
          } else if (std::filesystem::exists (header_path)
                     || (!create_header_only && std::filesystem::exists (source_path))) {
            create_error = "A file with that name already exists in "
                           "the project.";
          } else {
            const std::string header_name = header_path.filename ().string ();
            std::string header_text;
            if (is_system) {
              header_text = make_system_header_template (class_name, display_name, create_header_only);
            } else if (is_singleton) {
              header_text = make_singleton_header_template (class_name, display_name, create_header_only);
            } else {
              header_text = make_component_header_template (class_name, create_header_only);
            }

            const std::string source_text
                = is_system
                      ? make_system_source_template (header_name, class_name,
                                                     display_name)
                      : (is_singleton ? make_component_source_template (
                             header_name, class_name, display_name, true)
                                      : make_component_source_template (
                                          header_name, class_name,
                                          display_name, false));

            bool success = write_text_file (header_path, header_text);
            if (success && !create_header_only) {
              success = write_text_file (source_path, source_text);
            }

            if (!success) {
              create_error = "Could not write the new source files.";
            } else {
              const std::string canonical_header
                  = std::filesystem::weakly_canonical (header_path).string ();
              pending_open_path = canonical_header;
              needs_refresh = true;

              if (show_editor != nullptr) {
                *show_editor = true;
              }
              if (editor != nullptr) {
                editor->open_file (canonical_header.c_str ());
              }

              create_error.clear ();
              ImGui::CloseCurrentPopup ();
            }
          }
        }
      }
    }
  }

  ImGui::EndPopup ();
}

void
file_list::draw (const char *title, bool *p_open,
                 wsl::rsc::resource_manager *resources, editor::text_editor *editor,
                 bool *show_editor, wsl::comp::singl::runtime_context *runtime_ctx)
{
  refresh_if_needed (resources);

  if (!ImGui::Begin (title, p_open)) {
    ImGui::End ();
    return;
  }

  auto proj = (resources != nullptr) ? resources->current_project () : nullptr;
  draw_create_popup (resources, editor, show_editor);

  if (ImSearch::BeginSearch ()) {
    if (ImGui::Button ("+##unified_create", ImVec2 (ImGui::GetFrameHeight (), ImGui::GetFrameHeight ()))) {
      ImGui::OpenPopup ("UnifiedCreatePopup");
    }
    ImGui::SameLine ();

    if (runtime_ctx != nullptr) {
      const bool can_compile = (proj != nullptr);
      if (!can_compile) {
        ImGui::BeginDisabled ();
      }

      auto *editor_ctx = runtime_ctx->editor_ctx;
      if (editor_ctx != nullptr) {
        auto handle = editor_ctx->editor_resources.get (editor_ctx->icon_refresh);
        const float btn_size = ImGui::GetFrameHeight ();
        const float icon_padding = 4.0F;
        const float icon_size = btn_size - (icon_padding * 2.0F);

        if (handle && (handle->texture != nullptr)) {
          ImGui::PushStyleVar (ImGuiStyleVar_FramePadding, ImVec2 (icon_padding, icon_padding));
          if (ImGui::ImageButton ("##reload_scripts", (ImTextureID)handle->texture,
                                  ImVec2 (icon_size, icon_size))) {
            if (proj) {
              runtime_ctx->runtime_project_module.compile_and_load (*proj);
              runtime_ctx->runtime_project_module.finalize_load ();
            }
          }
          ImGui::PopStyleVar ();
        } else {
          if (ImGui::Button ("R##reload_scripts", ImVec2 (btn_size, btn_size))) {
            if (proj) {
              runtime_ctx->runtime_project_module.compile_and_load (*proj);
              runtime_ctx->runtime_project_module.finalize_load ();
            }
          }
        }
      }

      if (ImGui::IsItemHovered (ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip ("Reload Scripts");
      }

      if (!can_compile) {
        ImGui::EndDisabled ();
      }
      ImGui::SameLine ();
    }

    ImGui::SetNextItemWidth (-1.0F);
    ImSearch::SearchBar ("Search");

    if (ImGui::BeginPopup ("UnifiedCreatePopup")) {
      if (ImGui::BeginMenu ("World Component")) {
        if (ImGui::MenuItem ("Header Only")) {
          queue_create_popup (create_kind::component, true);
}
        if (ImGui::MenuItem ("Header and Source")) {
          queue_create_popup (create_kind::component, false);
}
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("Singleton Component")) {
        if (ImGui::MenuItem ("Header Only")) {
          queue_create_popup (create_kind::singleton, true);
}
        if (ImGui::MenuItem ("Header and Source")) {
          queue_create_popup (create_kind::singleton, false);
}
        ImGui::EndMenu ();
      }
      if (ImGui::BeginMenu ("System")) {
        if (ImGui::MenuItem ("Header Only")) {
          queue_create_popup (create_kind::system, true);
}
        if (ImGui::MenuItem ("Header and Source")) {
          queue_create_popup (create_kind::system, false);
}
        ImGui::EndMenu ();
      }
      ImGui::EndPopup ();
    }

    ImGui::BeginChild ("##files_merged_list", ImVec2 (0, 0), 1);

    auto draw_file_entry = [&] (const std::vector<entry> &files, int &selected_idx) {
      for (int i = 0; i < (int)files.size (); ++i) {
        ImSearch::SearchableItem (files[i].label.c_str (), [&, i] (const char *) {
          const bool is_this_selected = (selected_idx == i);
          if (ImGui::Selectable (files[i].label.c_str (), is_this_selected)) {
            selected_comp = -1;
            selected_sys = -1;
            selected_singleton = -1;
            selected_ui = -1;
            selected_shader = -1;
            selected_idx = i;
            if (show_editor) {
              *show_editor = true;
}
            if (editor) {
              editor->open_file (files[i].path.c_str ());
}
          }
        });
      }
    };

    // Category 1: World Components
    if (ImSearch::PushSearchable ("World Components", [] (const char *name) {
          return ImGui::TreeNodeEx (name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull);
        })) {
      draw_file_entry (component_files, selected_comp);
      ImSearch::PopSearchable ([] () { ImGui::TreePop (); });
    }

    // Category 2: Singleton Components
    if (ImSearch::PushSearchable ("Singleton Components", [] (const char *name) {
          return ImGui::TreeNodeEx (name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull);
        })) {
      draw_file_entry (singleton_files, selected_singleton);
      ImSearch::PopSearchable ([] () { ImGui::TreePop (); });
    }

    // Category 3: Systems
    if (ImSearch::PushSearchable ("Systems", [] (const char *name) {
          return ImGui::TreeNodeEx (name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull);
        })) {
      draw_file_entry (system_files, selected_sys);
      ImSearch::PopSearchable ([] () { ImGui::TreePop (); });
    }

    // Category 4: UI Layouts
    if (ImSearch::PushSearchable ("UI Layouts", [] (const char *name) {
          return ImGui::TreeNodeEx (name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull);
        })) {
      draw_file_entry (ui_layout_files, selected_ui);
      ImSearch::PopSearchable ([] () { ImGui::TreePop (); });
    }

    // Category 5: Shaders
    if (ImSearch::PushSearchable ("Shaders", [] (const char *name) {
          return ImGui::TreeNodeEx (name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesFull);
        })) {
      draw_file_entry (shader_files, selected_shader);
      ImSearch::PopSearchable ([] () { ImGui::TreePop (); });
    }

    ImSearch::Submit ();
    ImGui::EndChild ();
    ImSearch::EndSearch ();
  }

  ImGui::End ();
}

} // namespace editor
