// Animator editor UI and meta registration.
//
// Kept out of animator.hpp because the inspector widget pulls in ImGui, which
// the component header must stay free of. The plain data fields are still
// registered as entt meta so the component remains introspectable by tooling
// (MCP describe_component, generic editors); the custom inspector is what the
// editor actually draws.

#include "animator.hpp"

#include "singl/runtime_context.hpp"
#include "wsl/log/log.hpp"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace wsl
{

namespace comp
{

namespace
{

/** Formats seconds as m:ss for the transport readout. */
std::string
format_time (float seconds)
{
  if (!(seconds > 0.0F)) {
    return "--:--";
  }
  const int total = static_cast<int> (seconds);
  char buffer[32];
  std::snprintf (buffer, sizeof (buffer), "%d:%02d", total / 60, total % 60);
  return buffer;
}

/** Shows the assigned path read-only, with a hint when it is derived. */
void
draw_path_field (const char *label, const std::string &value)
{
  ImGui::TextDisabled ("%s", label);
  ImGui::SameLine ();
  ImGui::TextUnformatted (value.empty () || value == "None" ? "(derived)"
                                                            : value.c_str ());
}

} // namespace

bool
animator::custom_inspect (const char * /*label*/,
                          comp::singl::runtime_context * /*runtime*/)
{
  bool changed = false;

  // ---- clip picker ----
  // Only clips the owning model actually declares are offered, so a
  // multi-model scene cannot assign a clip with mismatched joints.
  if (available_clips.empty ()) {
    ImGui::TextDisabled ("No animation clips for this model");
  } else {
    int current = -1;
    for (std::size_t i = 0; i < available_clips.size (); ++i) {
      if (available_clips[i].path == clip_path) {
        current = static_cast<int> (i);
        break;
      }
    }

    std::string preview
        = current >= 0
              ? available_clips[static_cast<std::size_t> (current)].name
              : std::string ("(none)");

    if (ImGui::BeginCombo ("Clip", preview.c_str ())) {
      if (ImGui::Selectable ("(none)", current < 0)) {
        clip_path = "None";
        time = 0.0F;
        finished = false;
        changed = true;
      }
      for (std::size_t i = 0; i < available_clips.size (); ++i) {
        const bool selected = (static_cast<int> (i) == current);
        if (ImGui::Selectable (available_clips[i].name.c_str (), selected)) {
          clip_path = available_clips[i].path;
          time = 0.0F;
          finished = false;
          changed = true;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus ();
        }
      }
      ImGui::EndCombo ();
    }
  }

  draw_path_field ("Skeleton", skeleton_path);

  // ---- playback settings ----
  ImGui::PushID (this);
  ImGui::Spacing ();

  ImGui::SetNextItemWidth (120.0F);
  if (ImGui::SliderFloat ("Speed", &speed, -4.0F, 4.0F, "%.2fx")) {
    changed = true;
  }
  ImGui::SameLine ();
  if (ImGui::Checkbox ("Loop", &loop)) {
    changed = true;
  }

  ImGui::SetNextItemWidth (120.0F);
  if (ImGui::DragFloat ("Crossfade", &crossfade_duration, 0.01F, 0.0F, 10.0F,
                        "%.2f s")) {
    crossfade_duration = std::max (0.0F, crossfade_duration);
    changed = true;
  }

  int skin = skin_index;
  ImGui::SetNextItemWidth (120.0F);
  if (ImGui::DragInt ("Skin", &skin, 1, 0)) {
    skin_index = std::max (0, skin);
    changed = true;
  }

  // ---- transport ----
  // A clip switch resets the clock, so scrubbing to the end here is a
  // deliberate authoring action rather than a live-playback side effect.
  ImGui::Spacing ();
  ImGui::SetNextItemWidth (200.0F);
  float const max_time = std::max (clip_duration, 0.001F);
  if (ImGui::SliderFloat ("Time", &time, 0.0F, max_time,
                          format_time (time).c_str ())) {
    finished = false;
    changed = true;
  }
  ImGui::SameLine ();
  if (ImGui::Button (playing ? "Pause" : "Play")) {
    playing = !playing;
    paused = false;
    finished = false;
    changed = true;
  }
  ImGui::SameLine ();
  ImGui::TextDisabled ("%s / %s", format_time (time).c_str (),
                       format_time (clip_duration).c_str ());

  ImGui::PopID ();
  return changed;
}

void
animator::register_meta ()
{
  using namespace entt::literals;

  register_meta_impl ();

  // The custom inspector replaces the default field widgets in the editor;
  // the members above stay registered for generic introspection.
  entt::meta_factory<comp::animator> ().func<&comp::animator::custom_inspect> (
      "custom_inspect"_hs);
}

} // namespace comp

} // namespace wsl
