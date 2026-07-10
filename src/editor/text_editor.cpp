#include "text_editor.hpp"

#include <TextEditor.h>
#include <fstream>
#include <imgui.h>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace editor
{

static std::string
read_entire_file (const char *path)
{
  std::ifstream const f (path, std::ios::binary);
  if (!f.good ()) {
    return {};
  }
  std::ostringstream ss;
  ss << f.rdbuf ();
  return ss.str ();
}

static bool
write_entire_file (const char *path, std::string_view data)
{
  std::ofstream f (path, std::ios::binary | std::ios::trunc);
  if (!f.good ()) {
    return false;
  }
  f.write (data.data (), (std::streamsize)data.size ());
  return true;
}

text_editor::text_editor ()
{
  m_editor_ptr = std::make_unique<TextEditor> ();

  m_editor_ptr->SetLanguage (TextEditor::Language::Cpp ());
  m_editor_ptr->SetPalette (TextEditor::GetDarkPalette ());

  // NOTE: TrieAutoComplete::buildTrie() (third-party ImGuiColorTextEdit)
  // crashes in std::unordered_map when indexing the identifiers of certain
  // documents (e.g. large generated shader sources), taking down the editor.
  // Until that is fixed upstream we deliberately do not connect it; the
  // editor remains fully usable for viewing/editing without the
  // identifier-dropdown autocomplete.
}

text_editor::~text_editor ()
{
  // unique_ptr will clean up the TextEditor instance automatically
}

void
text_editor::set_background_color (ImU32 color)
{
  TextEditor::Palette p = TextEditor::GetDarkPalette ();
  p[static_cast<size_t> (TextEditor::Color::background)] = color;
  m_editor_ptr->SetPalette (p);
}

void
text_editor::ensure_cpp_language ()
{
  m_editor_ptr->SetLanguage (TextEditor::Language::Cpp ());
}

bool
text_editor::open_file (const char *path)
{
  if ((path == nullptr) || (path[0] == 0)) {
    return false;
  }
  std::string const txt = read_entire_file (path);
  if (txt.empty ()) {
    std::ifstream const probe (path);
    if (!probe.good ()) {
      return false;
    }
  }
  m_editor_ptr->SetText (txt);
  m_current_path = path;
  m_has_loaded_once = true;
  ensure_cpp_language ();
  return true;
}

bool
text_editor::save_file (const char *path)
{
  if ((path == nullptr) || (path[0] == 0)) {
    return false;
  }
  const std::string text = m_editor_ptr->GetText ();
  if (!write_entire_file (path, text)) {
    return false;
  }
  m_current_path = path;
  return true;
}

void
text_editor::draw (const char *title, bool *p_open)
{
  ensure_cpp_language ();

  ImGuiWindowFlags const flags
      = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar;

  if (!ImGui::Begin (title, p_open, flags)) {
    ImGui::End ();
    return;
  }

  if (ImGui::BeginMenuBar ()) {
    if (ImGui::BeginMenu ("File")) {
      if (ImGui::MenuItem ("Save", "Ctrl+S", false, !m_current_path.empty ())) {
        save_file (m_current_path.c_str ());
      }
      ImGui::Separator ();
      if (ImGui::MenuItem ("Close")) {
        if (p_open != nullptr) {
          *p_open = false;
        }
      }
      ImGui::EndMenu ();
    }
    if (ImGui::BeginMenu ("Edit")) {
      const bool ro = m_editor_ptr->IsReadOnlyEnabled ();
      if (ImGui::MenuItem ("Read-only", nullptr,
                           m_editor_ptr->IsReadOnlyEnabled ())) {
        m_editor_ptr->SetReadOnlyEnabled (!m_editor_ptr->IsReadOnlyEnabled ());
      }

      ImGui::Separator ();
      if (ImGui::MenuItem ("Undo", "Alt+Backspace", nullptr,
                           !ro && m_editor_ptr->CanUndo ())) {
        m_editor_ptr->Undo ();
      }
      if (ImGui::MenuItem ("Redo", "Ctrl+Y", nullptr,
                           !ro && m_editor_ptr->CanRedo ())) {
        m_editor_ptr->Redo ();
      }

      ImGui::Separator ();
      if (ImGui::MenuItem ("Copy", "Ctrl+C", nullptr,
                           m_editor_ptr->AnyCursorHasSelection ())) {
        m_editor_ptr->Copy ();
      }
      if (ImGui::MenuItem ("Cut", "Ctrl+X", nullptr,
                           !ro && m_editor_ptr->AnyCursorHasSelection ())) {
        m_editor_ptr->Cut ();
      }
      if (ImGui::MenuItem ("Paste", "Ctrl+V", nullptr,
                           !ro && ImGui::GetClipboardText () != nullptr)) {
        m_editor_ptr->Paste ();
      }

      ImGui::Separator ();
      if (ImGui::MenuItem ("Select All", "Ctrl+A")) {
        m_editor_ptr->SelectRegion (
            TextEditor::DocPos (),
            TextEditor::DocPos (m_editor_ptr->GetLineCount (), 0));
      }
      ImGui::EndMenu ();
    }
    ImGui::EndMenuBar ();
  }

  auto cpos = m_editor_ptr->GetMainCursorPosition ();
  const char *path
      = m_current_path.empty () ? "<unsaved>" : m_current_path.c_str ();
  ImGui::Text ("%6zu:%-4zu  %6zu lines  | %s | %s", cpos.line + 1,
               cpos.index + 1, m_editor_ptr->GetLineCount (),
               m_editor_ptr->IsOverwriteEnabled () ? "Ovr" : "Ins", path);

  // ---- monospace font for editor area ----
  if (m_mono_font != nullptr) {
    ImGui::PushFont (m_mono_font);
  }
  m_editor_ptr->Render ("TextEditor");
  if (m_mono_font != nullptr) {
    ImGui::PopFont ();
  }

  ImGui::End ();
}

} // namespace editor
