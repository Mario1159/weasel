#pragma once

#include <string>
#include <TextEditor.h>
#include <imgui.h>
#include <memory>

namespace editor
{

class text_editor
{
public:
  text_editor ();
  ~text_editor ();

  void
  set_mono_font (ImFont *font)
  {
    m_mono_font = font;
  }

  void set_background_color (ImU32 color);

  bool open_file (const char *path);
  bool save_file (const char *path);

  void draw (const char *title, bool *p_open);

private:
  void ensure_cpp_language ();

  std::string m_current_path;
  bool m_has_loaded_once = false;

  std::unique_ptr<TextEditor> m_editor_ptr;

  ImFont *m_mono_font = nullptr; // set from renderer_imgui
};

} // namespace editor
