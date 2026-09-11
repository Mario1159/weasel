#pragma once

#include "wsl/comp/component_meta.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/engine_resources.hpp"
#include <algorithm>
#include <entt/entt.hpp>
#include <imgui.h>
#include <imgui_internal.h>

namespace wsl::sys
{
class ecs_system;
}

namespace editor
{

enum class selection_kind
{
  none,
  entity,
  singleton
};

struct ecs_selection
{
  selection_kind kind{ selection_kind::none };
  entt::entity selected_entity{ entt::null };
  entt::id_type singleton_type{ entt::null };
  wsl::sys::ecs_system *selected_system{ nullptr };

  void
  select_entity (entt::entity entity)
  {
    kind = selection_kind::entity;
    selected_entity = entity;
    singleton_type = entt::null;
    selected_system = nullptr;
  }

  void
  select_singleton (entt::id_type type)
  {
    kind = selection_kind::singleton;
    singleton_type = type;
    selected_entity = entt::null;
    selected_system = nullptr;
  }

  void
  select_system (wsl::sys::ecs_system *sys)
  {
    kind = selection_kind::none;
    selected_entity = entt::null;
    singleton_type = entt::null;
    selected_system = sys;
  }

  void
  clear_entity_singleton ()
  {
    kind = selection_kind::none;
    selected_entity = entt::null;
    singleton_type = entt::null;
  }

  void
  clear_system ()
  {
    selected_system = nullptr;
  }

  void
  clear_all ()
  {
    clear_entity_singleton ();
    clear_system ();
  }
};

static inline std::string
meta_type_debug_name (const entt::meta_type &type)
{
  if (!type) {
    return "<invalid>";
  }

  const auto name = type.info ().name ();
  if (!name.empty ()) {
    return std::string (name);
  }

  return "<unnamed>";
}

static inline std::string
get_description (const entt::meta_type &type)
{
  if (const std::optional<wsl::comp::meta_info> info
      = wsl::comp::get_meta_info (type);
      info && !info->description.empty ()) {
    return info->description;
  }
  return "";
}

static inline std::string
get_description (const entt::meta_data &data)
{
  if (const std::optional<wsl::comp::meta_info> info
      = wsl::comp::get_meta_info (data);
      info && !info->description.empty ()) {
    return info->description;
  }
  return "";
}

static inline void
centered_text (const char *label, const ImVec2 &size_arg)
{
  ImGuiWindow const *window = ImGui::GetCurrentWindow ();

  ImGuiContext const &context = *GImGui;
  const ImGuiStyle &style = context.Style;
  const ImVec2 label_size = ImGui::CalcTextSize (label, nullptr, true);

  ImVec2 const pos = window->DC.CursorPos;
  // Use 0.0f for vertical padding to remove "extra newline" feel
  ImVec2 const size = ImGui::CalcItemSize (
      size_arg, label_size.x + (style.FramePadding.x * 2.0F), label_size.y);

  const ImVec2 pos2 = ImVec2 ((pos.x + size.x), (pos.y + size.y));
  const ImRect bb (pos, pos2);

  ImGui::ItemSize (size, 0.0F);
  if (!ImGui::ItemAdd (bb, 0)) {
    return;
  }

  const ImVec2 pos_min = ImVec2 ((bb.Min.x + style.FramePadding.x), bb.Min.y);
  const ImVec2 pos_max = ImVec2 ((bb.Max.x - style.FramePadding.x), bb.Max.y);

  ImGui::RenderTextClipped (pos_min, pos_max, label, nullptr, &label_size,
                            ImVec2 (0.5F, 0.5F), &bb);
}

static inline void
draw_centered_icon (wsl::comp::singl::editor_context *editor_ctx,
                    wsl::rsc::image_id icon_id, float icon_size = 128.0F,
                    const char *text = nullptr)
{
  if (editor_ctx == nullptr) {
    return;
  }

  auto handle = editor_ctx->editor_resources ().get (icon_id);
  if (!handle || ((*handle).texture.get () == nullptr)) {
    editor_ctx->editor_resources ().load (icon_id);
    return;
  }

  ImVec2 const size (icon_size, icon_size);
  ImVec2 const avail = ImGui::GetContentRegionAvail ();
  ImVec2 pos = ImGui::GetCursorPos ();

  float total_height = size.y;
  if (text != nullptr) {
    std::string const text_str = text;
    size_t start = 0;
    size_t end = text_str.find ('\n');
    total_height += ImGui::GetStyle ().ItemSpacing.y;
    while (true) {
      std::string const line = text_str.substr (start, end - start);
      if (!line.empty ()) {
        total_height += ImGui::GetTextLineHeight ();
      }

      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
      end = text_str.find ('\n', start);
    }
  }

  pos.x += (avail.x - size.x) * 0.5F;
  pos.y += (avail.y - total_height) * 0.5F;

  pos.x = std::max<float> (pos.x, 0);
  pos.y = std::max<float> (pos.y, 0);

  ImGui::SetCursorPos (pos);
  ImGui::Image ((ImTextureID)(*handle).texture.get (), size);

  if (text != nullptr) {
    ImGui::PushStyleColor (ImGuiCol_Text,
                           ImGui::GetStyleColorVec4 (ImGuiCol_TextDisabled));

    std::string const text_str = text;
    size_t start = 0;
    size_t end = text_str.find ('\n');
    while (true) {
      std::string const line = text_str.substr (start, end - start);
      if (!line.empty ()) {
        centered_text (line.c_str (), ImVec2 (avail.x, 0));
      }

      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
      end = text_str.find ('\n', start);
    }

    ImGui::PopStyleColor ();
  }
}

} // namespace editor
