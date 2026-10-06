#include "event_inspector.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "wsl/event/event_hub.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/reg/registry_queries.hpp"
#include "ecs_inspector.hpp"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <imgui.h>
#include <string>
#include <vector>

namespace editor
{

static float system_signals_h = 180.0F;
static char sys_sig_search[128] = "";
static char sys_handler_search[128] = "";
static char entity_sys_search[128] = "";
static char entity_sig_search[128] = "";

static void
draw_hsplitter (const char *id, float &top_height, float /*min_top*/,
                float /*min_bottom*/, float thickness = 6.0F)
{
  ImGui::InvisibleButton (id, ImVec2 (-1, thickness));

  if (ImGui::IsItemHovered ()) {
    ImGui::SetMouseCursor (ImGuiMouseCursor_ResizeNS);
  }

  if (ImGui::IsItemActive ()) {
    top_height += ImGui::GetIO ().MouseDelta.y;
  }

  ImU32 col = ImGui::GetColorU32 (ImGuiCol_Separator);
  if (ImGui::IsItemHovered () || ImGui::IsItemActive ()) {
    col = ImGui::GetColorU32 (ImGuiCol_SeparatorHovered);
  }

  ImVec2 const min = ImGui::GetItemRectMin ();
  ImVec2 const max = ImGui::GetItemRectMax ();
  ImGui::GetWindowDrawList ()->AddRectFilled (min, max, col);

  ImGui::Dummy (ImVec2 (0.0F, 6.0F));
}

static bool
text_match (const char *text, const char *filter)
{
  if ((filter == nullptr) || filter[0] == '\0') {
    return true;
  }

  std::string t = (text != nullptr) ? text : "";
  std::string f = filter;

  std::transform (t.begin (), t.end (), t.begin (),
                  [] (unsigned char c) { return (char)std::tolower (c); });
  std::transform (f.begin (), f.end (), f.begin (),
                  [] (unsigned char c) { return (char)std::tolower (c); });

  return t.find (f) != std::string::npos;
}

event_inspector::event_inspector (
    wsl::comp::singl::runtime_context *runtime_ctx,
    wsl::comp::singl::editor_context *editor_ctx, ecs_selection *selection)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_selection (selection)
{
}

static std::vector<const wsl::event::event_hub::registered_event_source *>
collect_event_sources_for_system (const wsl::reg::registry_queries &queries,
                                  entt::id_type system_type_id,
                                  const char *search_filter)
{
  auto sigs = queries.find_event_sources_owned_by_system (system_type_id);
  std::vector<const wsl::event::event_hub::registered_event_source *> out;

  for (const auto *e : sigs) {
    if (text_match (e->event_type_name.c_str (), search_filter)) {
      out.push_back (e);
    }
  }

  std::sort (out.begin (), out.end (), [] (const auto *a, const auto *b) {
    return a->event_type_name < b->event_type_name;
  });

  return out;
}

static std::vector<const wsl::event::event_connection_debug_entry *>
collect_event_connections (const wsl::reg::registry_queries &queries,
                           entt::id_type event_type_id)
{
  auto connections = queries.find_connections_for_event (event_type_id);
  std::vector<const wsl::event::event_connection_debug_entry *> out;

  for (const auto *connection : connections) {
    out.push_back (connection);
  }

  std::sort (out.begin (), out.end (), [] (const auto *a, const auto *b) {
    if (a->handler_name == b->handler_name) {
      return a->system_type_name < b->system_type_name;
    }
    return a->handler_name < b->handler_name;
  });

  return out;
}

static void
draw_event_source_table (
    const char *table_id,
    const std::vector<const wsl::event::event_hub::registered_event_source *>
        &sigs,
    wsl::event::event_debug_db *db, entt::id_type &selected_signal_type,
    entt::id_type &connect_requested_signal_type)
{
  if (ImGui::BeginTable (table_id, 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_SizingStretchProp
                             | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn ("Event");
    ImGui::TableSetupColumn ("Listeners");
    ImGui::TableSetupColumn ("Emits");
    ImGui::TableHeadersRow ();

    for (const auto *e : sigs) {
      ImGui::TableNextRow ();
      ImGui::TableNextColumn ();

      const bool is_selected = (selected_signal_type == e->event_type_id);
      std::string const label
          = e->event_type_name + "##sig_" + std::to_string (e->event_type_id);

      if (ImGui::Selectable (label.c_str (), is_selected,
                             ImGuiSelectableFlags_SpanAllColumns
                                 | ImGuiSelectableFlags_AllowOverlap)) {
        selected_signal_type = e->event_type_id;
      }

      if (ImGui::IsItemClicked (ImGuiMouseButton_Right)) {
        selected_signal_type = e->event_type_id;
      }

      const std::string popup_id
          = "signal_row_context_" + std::to_string (e->event_type_id);
      if (ImGui::BeginPopupContextItem (popup_id.c_str ())) {
        selected_signal_type = e->event_type_id;

        if (ImGui::Button ("Connect Event")) {
          connect_requested_signal_type = e->event_type_id;
          ImGui::CloseCurrentPopup ();
        }

        ImGui::EndPopup ();
      }

      ImGui::TableNextColumn ();
      if (db != nullptr) {
        auto it = db->entries.find (e->event_type_id);
        if (it != db->entries.end ()) {
          ImGui::Text ("%zu", it->second.listener_count);
        }
      }

      ImGui::TableNextColumn ();
      if (db != nullptr) {
        auto it = db->entries.find (e->event_type_id);
        if (it != db->entries.end ()) {
          ImGui::Text ("%zu", it->second.emit_count);
        }
      }
    }

    ImGui::EndTable ();
  }
}

static void
draw_entity_event_tree (
    const wsl::reg::registry_queries &queries,
    const std::vector<const wsl::event::event_hub::registered_event_source *>
        &sigs,
    entt::id_type &selected_signal_type,
    entt::id_type &connect_requested_signal_type)
{
  for (const auto *signal : sigs) {
    const auto connections
        = collect_event_connections (queries, signal->event_type_id);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected_signal_type == signal->event_type_id) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    const std::string tree_id = signal->event_type_name
                                + "##entity_signal_tree_"
                                + std::to_string (signal->event_type_id);
    const bool open = ImGui::TreeNodeEx (tree_id.c_str (), flags, "%s",
                                         signal->event_type_name.c_str ());

    if (ImGui::IsItemClicked ()) {
      selected_signal_type = signal->event_type_id;
    }

    const std::string popup_id
        = "entity_signal_context_" + std::to_string (signal->event_type_id);
    if (ImGui::BeginPopupContextItem (popup_id.c_str ())) {
      selected_signal_type = signal->event_type_id;

      if (ImGui::Button ("Connect Event")) {
        connect_requested_signal_type = signal->event_type_id;
        ImGui::CloseCurrentPopup ();
      }

      ImGui::EndPopup ();
    }

    if (!open) {
      continue;
    }

    for (const auto *connection : connections) {
      std::string label = connection->handler_name + "  ["
                          + connection->system_type_name + "]";
      ImGui::BulletText ("%s", label.c_str ());
    }

    if (connections.empty ()) {
      ImGui::TextDisabled ("No connected handlers.");
    }

    ImGui::TreePop ();
  }
}

static void
draw_system_handlers_table (const wsl::reg::registry_queries &queries,
                            entt::id_type system_type_id,
                            const char *search_filter)
{
  if (ImGui::BeginTable ("system_handlers_table", 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_SizingStretchProp
                             | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn ("Handler");
    ImGui::TableSetupColumn ("System");
    ImGui::TableSetupColumn ("Kind");
    ImGui::TableHeadersRow ();

    for (const auto *h :
         queries.find_event_handlers_owned_by_system (system_type_id)) {
      if (!text_match (h->handler_name.c_str (), search_filter)) {
        continue;
      }

      ImGui::TableNextRow ();
      ImGui::TableNextColumn ();
      ImGui::TextUnformatted (h->handler_name.c_str ());
      ImGui::TableNextColumn ();
      ImGui::TextUnformatted (h->system_type_name.c_str ());
      ImGui::TableNextColumn ();
      ImGui::TextDisabled ("Global");
    }

    for (const auto *h :
         queries.find_event_sinks_owned_by_system (system_type_id)) {
      if (!text_match (h->handler_name.c_str (), search_filter)) {
        continue;
      }

      ImGui::TableNextRow ();
      ImGui::TableNextColumn ();
      ImGui::TextUnformatted (h->handler_name.c_str ());
      ImGui::TableNextColumn ();
      ImGui::TextUnformatted (h->system_type_name.c_str ());
      ImGui::TableNextColumn ();
      ImGui::TextDisabled ("Connectable");
    }

    ImGui::EndTable ();
  }
}

void
event_inspector::open_signal_connection_modal (entt::id_type signal_type)
{
  m_connect_signal_type = signal_type;
  m_selected_signal_type = signal_type;
  m_connect_handler_system_type = 0;
  m_connect_handler_name.clear ();
  m_connect_modal_error.clear ();
  m_connect_handler_search[0] = '\0';
  m_request_open_connect_modal = true;
}

void
event_inspector::draw_signal_connection_modal (entt::registry & /*registry*/)
{
  if (m_request_open_connect_modal) {
    ImGui::OpenPopup ("Connect Event");
    m_request_open_connect_modal = false;
  }

  ImGui::SetNextWindowSize (ImVec2 (560.0F, 0.0F), ImGuiCond_FirstUseEver);
  if (!ImGui::BeginPopupModal ("Connect Event", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  auto &hub = m_runtime_ctx->event_hub ();
  auto *db = hub.db;

  const wsl::event::event_source_debug_entry *signal_entry = nullptr;
  if (db != nullptr) {
    auto it = db->entries.find (m_connect_signal_type);
    if (it != db->entries.end ()) {
      signal_entry = &it->second;
    }
  }

  if (signal_entry == nullptr) {
    ImGui::TextDisabled ("Select a valid event before creating a connection.");
  } else {
    ImGui::Text ("Event: %s", signal_entry->type_name.c_str ());
    if (!signal_entry->owner_system_type_name.empty ()) {
      ImGui::TextDisabled ("Owner: %s",
                           signal_entry->owner_system_type_name.c_str ());
    }

    ImGui::Separator ();
    ImGui::TextUnformatted ("Event Handlers");
    ImGui::InputTextWithHint (
        "##ConnectHandlerSearch", "Search event handlers...",
        m_connect_handler_search, IM_ARRAYSIZE (m_connect_handler_search));

    std::vector<const wsl::event::event_hub::registered_event_sink *> handlers;
    for (const auto &h : hub.registered_event_sinks) {
      if (h.event_type_id == m_connect_signal_type) {
        const std::string search_text
            = h.handler_name + " " + h.system_type_name;
        if (text_match (search_text.c_str (), m_connect_handler_search)) {
          handlers.push_back (&h);
        }
      }
    }

    ImGui::BeginChild ("ConnectHandlersList", ImVec2 (520.0F, 180.0F), 1);
    if (handlers.empty ()) {
      ImGui::TextDisabled ("No connectable handlers match this event.");
    } else {
      for (const auto *handler : handlers) {
        const bool is_selected
            = m_connect_handler_system_type == handler->system_type_id
              && m_connect_handler_name == handler->handler_name;

        std::string label
            = handler->handler_name + "  [" + handler->system_type_name + "]";
        label += "##connect_handler_" + std::to_string (handler->system_type_id)
                 + "_" + std::to_string (handler->event_type_id);

        if (ImGui::Selectable (label.c_str (), is_selected)) {
          m_connect_handler_system_type = handler->system_type_id;
          m_connect_handler_name = handler->handler_name;
          m_connect_modal_error.clear ();
        }
      }
    }
    ImGui::EndChild ();

    const wsl::event::event_hub::registered_event_sink *selected_handler
        = nullptr;
    for (const auto &h : hub.registered_event_sinks) {
      if (h.event_type_id == m_connect_signal_type
          && h.system_type_id == m_connect_handler_system_type
          && h.handler_name == m_connect_handler_name) {
        selected_handler = &h;
        break;
      }
    }

    if (selected_handler != nullptr) {
      ImGui::Separator ();
      ImGui::Text ("Selected Handler: %s",
                   selected_handler->handler_name.c_str ());
      ImGui::TextDisabled ("System: %s",
                           selected_handler->system_type_name.c_str ());
    }
  }

  if (!m_connect_modal_error.empty ()) {
    ImGui::Spacing ();
    ImGui::TextColored (ImVec4 (1.0F, 0.45F, 0.45F, 1.0F), "%s",
                        m_connect_modal_error.c_str ());
  }

  const bool can_connect
      = (signal_entry != nullptr) && (!m_connect_handler_name.empty ());

  if (!can_connect) {
    ImGui::BeginDisabled ();
  }

  if (ImGui::Button ("Connect")) {
    if (m_runtime_ctx->event_hub ().connect (m_connect_signal_type,
                                             m_connect_handler_system_type,
                                             m_connect_handler_name)) {
      m_connect_modal_error.clear ();
      ImGui::CloseCurrentPopup ();
    } else {
      m_connect_modal_error = "Could not create the event connection with the "
                              "current selection.";
    }
  }

  if (!can_connect) {
    ImGui::EndDisabled ();
  }

  ImGui::SameLine ();

  if (ImGui::Button ("Cancel")) {
    m_connect_modal_error.clear ();
    ImGui::CloseCurrentPopup ();
  }

  ImGui::EndPopup ();
}

void
event_inspector::draw ()
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts ().bold);
  const bool open = ImGui::Begin ("Events");
  ImGui::PopFont ();

  if (!open) {
    ImGui::End ();
    return;
  }

  auto *scene = m_runtime_ctx->scene_manager ().get_active ();
  if (scene == nullptr) {
    draw_centered_icon (
        m_editor_ctx, m_editor_ctx->icon_signal (), 128.0F,
        "Events are the heartbeat of your game,\nMonitor and debug events as "
        "they flow through the system.");
    ImGui::End ();
    return;
  }

  auto &registry = scene->get_registry ();
  auto &queries = m_runtime_ctx->reg_queries ();
  auto *db = m_runtime_ctx->event_hub ().db;
  entt::id_type connect_requested_signal_type = 0;

  const bool has_system
      = (m_selection != nullptr) && m_selection->selected_system != nullptr;

  const bool has_entity = (m_selection != nullptr)
                          && m_selection->kind == selection_kind::entity
                          && m_selection->selected_entity != entt::null
                          && registry.valid (m_selection->selected_entity);

  if (has_system) {
    wsl::sys::ecs_system const *sys = m_selection->selected_system;
    const entt::id_type sys_tid = sys->get_type_id ();

    const float splitter_thickness = 6.0F;
    const float total_h = ImGui::GetContentRegionAvail ().y;
    const float min_top = 120.0F;
    const float min_bottom = 120.0F;

    system_signals_h = ImClamp (system_signals_h, min_top,
                                total_h - min_bottom - splitter_thickness);

    ImGui::Text ("System: %s", sys->get_name ().c_str ());
    ImGui::TextDisabled ("%s", sys->get_type_name ());
    ImGui::Separator ();

    ImGui::TextUnformatted ("System Events");
    ImGui::InputTextWithHint ("##SysSigSearch", "Search system events...",
                              sys_sig_search, IM_ARRAYSIZE (sys_sig_search));

    ImGui::BeginChild ("SystemSignalsRegion", ImVec2 (0, system_signals_h), 1);

    {
      auto sigs
          = collect_event_sources_for_system (queries, sys_tid, sys_sig_search);
      if (sigs.empty ()) {
        ImGui::TextDisabled ("No events declared for this system.");
      } else {
        draw_event_source_table ("system_signals_table", sigs, db,
                                 m_selected_signal_type,
                                 connect_requested_signal_type);
      }
    }

    ImGui::EndChild ();

    draw_hsplitter ("##system_signal_handler_splitter", system_signals_h,
                    min_top, min_bottom, splitter_thickness);

    ImGui::TextUnformatted ("System Event Handlers");
    ImGui::InputTextWithHint ("##SysHandlerSearch", "Search handlers...",
                              sys_handler_search,
                              IM_ARRAYSIZE (sys_handler_search));

    ImGui::BeginChild ("SystemHandlersRegion", ImVec2 (0, 0), 1);

    bool has_any_handler
        = !queries.find_event_handlers_owned_by_system (sys_tid).empty ()
          || !queries.find_event_sinks_owned_by_system (sys_tid).empty ();

    if (!has_any_handler) {
      ImGui::TextDisabled ("No handlers declared for this system.");
    } else {
      draw_system_handlers_table (queries, sys_tid, sys_handler_search);
    }

    ImGui::EndChild ();

    if (connect_requested_signal_type != 0) {
      open_signal_connection_modal (connect_requested_signal_type);
    }
    draw_signal_connection_modal (registry);

    ImGui::End ();
    return;
  }

  if (has_entity) {
    const entt::entity ent = m_selection->selected_entity;

    ImGui::Text ("Entity: %u", static_cast<uint32_t> (ent));
    ImGui::InputTextWithHint ("##EntitySystemSearch",
                              "Search matching systems...", entity_sys_search,
                              IM_ARRAYSIZE (entity_sys_search));
    ImGui::InputTextWithHint (
        "##EntitySignalSearch", "Search events inside expanded systems...",
        entity_sig_search, IM_ARRAYSIZE (entity_sig_search));
    ImGui::Separator ();

    auto matched_systems = queries.get_matching_systems (registry, ent);

    if (matched_systems.empty ()) {
      ImGui::TextDisabled ("No registered systems match this entity.");
      ImGui::End ();
      return;
    }

    for (const auto *sys_desc : matched_systems) {
      if (!text_match (sys_desc->display_name.c_str (), entity_sys_search)) {
        continue;
      }

      const std::string label = sys_desc->display_name + "##entity_system_"
                                + std::to_string (sys_desc->type_id);

      if (ImGui::TreeNodeEx (label.c_str (),
                             ImGuiTreeNodeFlags_SpanAvailWidth)) {
        auto sigs = collect_event_sources_for_system (
            queries, sys_desc->type_id, entity_sig_search);

        if (sigs.empty ()) {
          ImGui::TextDisabled ("No events declared for this system.");
        } else {
          draw_entity_event_tree (queries, sigs, m_selected_signal_type,
                                  connect_requested_signal_type);
        }

        ImGui::TreePop ();
      }
    }

    if (connect_requested_signal_type != 0) {
      open_signal_connection_modal (connect_requested_signal_type);
    }
    draw_signal_connection_modal (registry);

    ImGui::End ();
    return;
  }

  ImGui::TextDisabled ("Select a system or an entity.");
  draw_signal_connection_modal (registry);
  ImGui::End ();
}

} // namespace editor
