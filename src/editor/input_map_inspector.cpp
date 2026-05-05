#include "input_map_inspector.hpp"

#include "input.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <cstdio>
#include <entt/entity/fwd.hpp>
#include <imgui.h>
#include <iterator>

namespace editor
{

struct scancode_item
{
  SDL_Scancode code;
  const char *name;
};

static const scancode_item scancode_list[] = {
  { SDL_SCANCODE_A, "A" },          { SDL_SCANCODE_B, "B" },
  { SDL_SCANCODE_C, "C" },          { SDL_SCANCODE_D, "D" },
  { SDL_SCANCODE_E, "E" },          { SDL_SCANCODE_F, "F" },
  { SDL_SCANCODE_G, "G" },          { SDL_SCANCODE_H, "H" },
  { SDL_SCANCODE_I, "I" },          { SDL_SCANCODE_J, "J" },
  { SDL_SCANCODE_K, "K" },          { SDL_SCANCODE_L, "L" },
  { SDL_SCANCODE_M, "M" },          { SDL_SCANCODE_N, "N" },
  { SDL_SCANCODE_O, "O" },          { SDL_SCANCODE_P, "P" },
  { SDL_SCANCODE_Q, "Q" },          { SDL_SCANCODE_R, "R" },
  { SDL_SCANCODE_S, "S" },          { SDL_SCANCODE_T, "T" },
  { SDL_SCANCODE_U, "U" },          { SDL_SCANCODE_V, "V" },
  { SDL_SCANCODE_W, "W" },          { SDL_SCANCODE_X, "X" },
  { SDL_SCANCODE_Y, "Y" },          { SDL_SCANCODE_Z, "Z" },
  { SDL_SCANCODE_SPACE, "Space" },  { SDL_SCANCODE_ESCAPE, "Escape" },
  { SDL_SCANCODE_RETURN, "Enter" }, { SDL_SCANCODE_TAB, "Tab" },
};

static int
find_scancode_index (SDL_Scancode sc)
{
  for (int i = 0; i < (int)(sizeof (scancode_list) / sizeof (scancode_item));
       ++i) {
    if (scancode_list[i].code == sc) {
      return i;
}
}
  return 0;
}

void
input_map_inspector::draw (entt::registry & /*unused*/, wsl::comp::singl::runtime_context *runtime_ctx)
{
  if (!ImGui::Begin ("Input Map")) {
    ImGui::End ();
    return;
  }

  wsl::comp::singl::runtime_context &rtc = *runtime_ctx;
  auto &map = rtc.get_app_input_map ().bindings;

  ImGui::Text ("Action to Key Binding");
  ImGui::Separator ();

  int idx = 0;
  m_remove_index = -1;

  for (auto it = map.begin (); it != map.end (); ++it, ++idx) {
    ImGui::PushID (idx);

    // ---- Action name ----
    char action_buf[128];
    std::snprintf (action_buf, sizeof (action_buf), "%s", it->first.c_str ());

    if (ImGui::InputText ("Action", action_buf, sizeof (action_buf))) {
      wsl::input::key_binding const val = it->second;
      map.erase (it);
      map.emplace (action_buf, val);
      ImGui::PopID ();
      break; // iterator invalidated
    }

    wsl::input::key_binding &key = it->second;

    // ---- Scancode ----
    int sc_index = find_scancode_index (key.scancode);
    if (ImGui::Combo (
            "Key", &sc_index,
            [] (void *, int i, const char **out) {
              *out = scancode_list[i].name;
              return true;
            },
            nullptr, (int)(sizeof (scancode_list) / sizeof (scancode_item)))) {
      key.scancode = scancode_list[sc_index].code;
    }

    // ---- Modifiers ----
    bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0u;
    bool shift = (key.mod & SDL_KMOD_SHIFT) != 0u;
    bool alt = (key.mod & SDL_KMOD_ALT) != 0u;
    bool gui = (key.mod & SDL_KMOD_GUI) != 0u;

    if (ImGui::Checkbox ("Ctrl", &ctrl) || ImGui::Checkbox ("Shift", &shift)
        || ImGui::Checkbox ("Alt", &alt) || ImGui::Checkbox ("GUI", &gui)) {
      key.mod = SDL_KMOD_NONE;
      if (ctrl) {
        key.mod |= SDL_KMOD_CTRL;
}
      if (shift) {
        key.mod |= SDL_KMOD_SHIFT;
}
      if (alt) {
        key.mod |= SDL_KMOD_ALT;
}
      if (gui) {
        key.mod |= SDL_KMOD_GUI;
}
    }

    ImGui::SameLine ();
    if (ImGui::SmallButton ("Remove")) {
      m_remove_index = idx;
    }

    ImGui::Separator ();
    ImGui::PopID ();
  }

  // ---- Remove after loop ----
  if (m_remove_index >= 0) {
    auto it = map.begin ();
    std::advance (it, m_remove_index);
    map.erase (it);
  }

  // ================= Add new binding =================

  static char new_action[128] = "";
  static int new_sc_index = 0;
  static bool new_ctrl = false;
  static bool new_shift = false;
  static bool new_alt = false;
  static bool new_gui = false;

  ImGui::Separator ();
  ImGui::Text ("Add Binding");

  ImGui::InputText ("New Action", new_action, sizeof (new_action));

  ImGui::Combo (
      "New Key", &new_sc_index,
      [] (void *, int i, const char **out) {
        *out = scancode_list[i].name;
        return true;
      },
      nullptr, (int)(sizeof (scancode_list) / sizeof (scancode_item)));

  ImGui::Checkbox ("Ctrl##new", &new_ctrl);
  ImGui::SameLine ();
  ImGui::Checkbox ("Shift##new", &new_shift);
  ImGui::SameLine ();
  ImGui::Checkbox ("Alt##new", &new_alt);
  ImGui::SameLine ();
  ImGui::Checkbox ("GUI##new", &new_gui);

  if (ImGui::Button ("Add") && new_action[0] != '\0') {
    wsl::input::key_binding k{};
    k.scancode = scancode_list[new_sc_index].code;
    k.mod = SDL_KMOD_NONE;
    if (new_ctrl) {
      k.mod |= SDL_KMOD_CTRL;
}
    if (new_shift) {
      k.mod |= SDL_KMOD_SHIFT;
}
    if (new_alt) {
      k.mod |= SDL_KMOD_ALT;
}
    if (new_gui) {
      k.mod |= SDL_KMOD_GUI;
}

    map[new_action] = k;

    new_action[0] = '\0';
    new_ctrl = new_shift = new_alt = new_gui = false;
  }

  ImGui::End ();
}

} // namespace editor
