#include "signal_inspector.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "wsl/reg/sig/signal_hub.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "ecs_inspector.hpp"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
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

  // Nice cursor while hovering
  if (ImGui::IsItemHovered ()) {
    ImGui::SetMouseCursor (ImGuiMouseCursor_ResizeNS);
  }

  if (ImGui::IsItemActive ()) {
    top_height += ImGui::GetIO ().MouseDelta.y;
  }

  // Draw the splitter bar
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

signal_inspector::signal_inspector (
    wsl::comp::singl::runtime_context *runtime_ctx,
    wsl::comp::singl::editor_context *editor_ctx, ecs_selection *selection)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx),
      m_selection (selection)
{
}

static std::vector<const wsl::reg::sig::signal_debug_entry *>
collect_signals_for_system (const wsl::reg::registry_queries &queries,
                            entt::id_type system_type_id,
                            const char *search_filter)
{
  auto sigs = queries.find_signals_owned_by_system (system_type_id);
  std::vector<const wsl::reg::sig::signal_debug_entry *> out;

  for (const auto *e : sigs) {
    if (text_match (e->type_name.c_str (), search_filter)) {
      out.push_back (e);
    }
  }

  std::sort (out.begin (), out.end (), [] (const auto *a, const auto *b) {
    return a->type_name < b->type_name;
  });

  return out;
}

static std::vector<const wsl::reg::sig::signal_connection_debug_entry *>
collect_signal_connections (const wsl::reg::registry_queries &queries,
                            entt::id_type signal_type_id,
                            entt::entity source_entity)
{
  auto connections = queries.find_connections_for_signal (signal_type_id);
  std::vector<const wsl::reg::sig::signal_connection_debug_entry *> out;

  for (const auto *connection : connections) {
    if (source_entity != entt::null && connection->source_entity != entt::null
        && connection->source_entity != source_entity) {
      continue;
    }

    out.push_back (connection);
  }

  std::sort (out.begin (), out.end (), [] (const auto *a, const auto *b) {
    if (a->handler_name == b->handler_name) {
      if (a->system_type_name == b->system_type_name) {
        return static_cast<uint32_t> (a->target_entity)
               < static_cast<uint32_t> (b->target_entity);
      }
      return a->system_type_name < b->system_type_name;
    }
    return a->handler_name < b->handler_name;
  });

  return out;
}

static void
draw_signal_table (const char *table_id,
                   const std::vector<const wsl::reg::sig::signal_debug_entry *> &sigs,
                   entt::id_type &selected_signal_type,
                   entt::id_type &connect_requested_signal_type)
{
  if (ImGui::BeginTable (table_id, 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_SizingStretchProp
                             | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn ("Signal");
    ImGui::TableSetupColumn ("Listeners");
    ImGui::TableSetupColumn ("Emits");
    ImGui::TableHeadersRow ();

    for (const auto *e : sigs) {
      ImGui::TableNextRow ();
      ImGui::TableNextColumn ();

      const bool is_selected = (selected_signal_type == e->type_id);
      std::string const label
          = e->type_name + "##sig_" + std::to_string (e->type_id);

      if (ImGui::Selectable (label.c_str (), is_selected,
                             ImGuiSelectableFlags_SpanAllColumns
                                 | ImGuiSelectableFlags_AllowOverlap)) {
        selected_signal_type = e->type_id;
      }

      if (ImGui::IsItemClicked (ImGuiMouseButton_Right)) {
        selected_signal_type = e->type_id;
      }

      const std::string popup_id
          = "signal_row_context_" + std::to_string (e->type_id);
      if (ImGui::BeginPopupContextItem (popup_id.c_str ())) {
        selected_signal_type = e->type_id;

        if (ImGui::Button ("Connect Signal")) {
          connect_requested_signal_type = e->type_id;
          ImGui::CloseCurrentPopup ();
        }

        ImGui::EndPopup ();
      }

      ImGui::TableNextColumn ();
      ImGui::Text ("%zu", e->listener_count);

      ImGui::TableNextColumn ();
      ImGui::Text ("%zu", e->emit_count);
    }

    ImGui::EndTable ();
  }
}

static void
draw_entity_signal_tree (
    wsl::rsc::scene &scene, entt::registry &registry,
    const wsl::reg::registry_queries &queries,
    const std::vector<const wsl::reg::sig::signal_debug_entry *> &sigs,
    entt::entity source_entity, entt::id_type &selected_signal_type,
    entt::id_type &connect_requested_signal_type)
{
  for (const auto *signal : sigs) {
    const auto connections
        = collect_signal_connections (queries, signal->type_id, source_entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected_signal_type == signal->type_id) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    const std::string tree_id = signal->type_name + "##entity_signal_tree_"
                                + std::to_string (signal->type_id);
    const bool open = ImGui::TreeNodeEx (tree_id.c_str (), flags, "%s",
                                         signal->type_name.c_str ());

    if (ImGui::IsItemClicked ()) {
      selected_signal_type = signal->type_id;
    }

    const std::string popup_id
        = "entity_signal_context_" + std::to_string (signal->type_id);
    if (ImGui::BeginPopupContextItem (popup_id.c_str ())) {
      selected_signal_type = signal->type_id;

      if (ImGui::Button ("Connect Signal")) {
        connect_requested_signal_type = signal->type_id;
        ImGui::CloseCurrentPopup ();
      }

      ImGui::EndPopup ();
    }

    if (!open) {
      continue;
    }

    // Explicit connections
    for (const auto *connection : connections) {
      const bool has_target = connection->target_entity != entt::null;
      std::string label = connection->handler_name + "  ["
                          + connection->system_type_name + "]";
      if (has_target) {
        label += " -> " + scene.get_entity_name (connection->target_entity)
                 + " ("
                 + std::to_string (
                     static_cast<uint32_t> (connection->target_entity))
                 + ")";
      }

      ImGui::BulletText ("%s", label.c_str ());

      if (connection->source_entity == entt::null) {
        ImGui::SameLine ();
        ImGui::TextDisabled ("(global)");
      }
    }

    // Automatic (potential) handlers
    for (const auto *h :
         queries.find_event_handlers_using_world_component (0)) { // This is tricky, find_event_handlers_using_world_component(0) doesn't exist
      // Actually, we can get all connectable handlers from signal_hub if we want,
      // but let's just stick to what registry_queries offers.
    }

    // For now, let's just show explicit connections in the tree.
    // The previous implementation was poking signal_db.connectable_handlers directly.

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
  if (ImGui::BeginTable ("system_handlers_table", 2,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_SizingStretchProp
                             | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn ("Handler");
    ImGui::TableSetupColumn ("Type");
    ImGui::TableHeadersRow ();

    for (const auto *h : queries.find_event_handlers_owned_by_system (system_type_id)) {
      if (!text_match (h->handler_name.c_str (), search_filter)) {
        continue;
      }

      ImGui::TableNextRow ();
      ImGui::TableNextColumn ();
      ImGui::TextUnformatted (h->handler_name.c_str ());
      ImGui::TableNextColumn ();
      ImGui::TextDisabled ("Global");
    }

    // Note: registry_queries currently only returns system_handler_debug_entry
    // for owned handlers. Connectable handlers are a different type.
    // In a mature refactor, we'd unified these in registry_queries.

    ImGui::EndTable ();
  }
}

void
signal_inspector::open_signal_connection_modal (entt::id_type signal_type,
                                                entt::entity source_entity)
{
  m_connect_signal_type = signal_type;
  m_selected_signal_type = signal_type;
  m_connect_handler_system_type = 0;
  m_connect_source_entity = source_entity;
  m_connect_target_entity = entt::null;
  m_connect_handler_name.clear ();
  m_connect_modal_error.clear ();
  m_connect_handler_search[0] = '\0';
  m_connect_entity_search[0] = '\0';
  m_request_open_connect_modal = true;
}

void
signal_inspector::draw_signal_connection_modal (entt::registry &registry)
{
  if (m_request_open_connect_modal) {
    ImGui::OpenPopup ("Connect Signal");
    m_request_open_connect_modal = false;
  }

  ImGui::SetNextWindowSize (ImVec2 (560.0F, 0.0F), ImGuiCond_FirstUseEver);
  if (!ImGui::BeginPopupModal ("Connect Signal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  auto *scene = m_runtime_ctx->scene_manager.get_active ();
  auto &queries = m_runtime_ctx->reg_queries;

  // We need to find the signal entry. registry_queries could have a find_signal method.
  // For now, we'll poke signal_hub.db if needed, but the goal is to avoid it.
  const wsl::reg::sig::signal_debug_entry *signal_entry = nullptr;
  if (m_runtime_ctx->signal_hub.db != nullptr) {
    auto it = m_runtime_ctx->signal_hub.db->entries.find (m_connect_signal_type);
    if (it != m_runtime_ctx->signal_hub.db->entries.end ()) {
      signal_entry = &it->second;
    }
  }

  if ((scene == nullptr) || (signal_entry == nullptr)) {
    ImGui::TextDisabled ("Select a valid signal before creating a connection.");
  } else {
    ImGui::Text ("Signal: %s", signal_entry->type_name.c_str ());
    if (!signal_entry->owner_system_type_name.empty ()) {
      ImGui::TextDisabled ("Owner: %s",
                           signal_entry->owner_system_type_name.c_str ());
    }
    if (m_connect_source_entity != entt::null) {
      const std::string &source_name
          = scene->get_entity_name (m_connect_source_entity);
      ImGui::TextDisabled ("Source Entity: %s (%u)", source_name.c_str (),
                           static_cast<uint32_t> (m_connect_source_entity));
    }

    ImGui::Separator ();
    ImGui::TextUnformatted ("Event Handlers");
    ImGui::InputTextWithHint (
        "##ConnectHandlerSearch", "Search event handlers...",
        m_connect_handler_search, IM_ARRAYSIZE (m_connect_handler_search));

    // collect_connectable_handlers should be in queries
    std::vector<const wsl::reg::sig::signal_connectable_handler_debug_entry *> handlers;
    if (m_runtime_ctx->signal_hub.db != nullptr) {
        for (auto &h : m_runtime_ctx->signal_hub.db->connectable_handlers) {
            if (h.signal_type_id == m_connect_signal_type) {
                const std::string search_text = h.handler_name + " " + h.system_type_name;
                if (text_match (search_text.c_str (), m_connect_handler_search)) {
                    handlers.push_back (&h);
                }
            }
        }
    }

    ImGui::BeginChild ("ConnectHandlersList", ImVec2 (520.0F, 180.0F), 1);
    if (handlers.empty ()) {
      ImGui::TextDisabled ("No connectable handlers match this signal.");
    } else {
      for (const auto *handler : handlers) {
        const bool is_selected
            = m_connect_handler_system_type == handler->system_type_id
              && m_connect_handler_name == handler->handler_name;

        std::string label
            = handler->handler_name + "  [" + handler->system_type_name + "]";
        label += "##connect_handler_" + std::to_string (handler->system_type_id)
                 + "_" + std::to_string (handler->signal_type_id);

        if (ImGui::Selectable (label.c_str (), is_selected)) {
          m_connect_handler_system_type = handler->system_type_id;
          m_connect_handler_name = handler->handler_name;
          m_connect_target_entity = entt::null;
          m_connect_modal_error.clear ();
        }
      }
    }
    ImGui::EndChild ();

    // Finding the selected handler entry
    const wsl::reg::sig::signal_connectable_handler_debug_entry *selected_handler = nullptr;
    if (m_runtime_ctx->signal_hub.db != nullptr) {
        for (auto &h : m_runtime_ctx->signal_hub.db->connectable_handlers) {
            if (h.signal_type_id == m_connect_signal_type && h.system_type_id == m_connect_handler_system_type && h.handler_name == m_connect_handler_name) {
                selected_handler = &h;
                break;
            }
        }
    }

    if (selected_handler != nullptr) {
      ImGui::Separator ();
      ImGui::Text ("Selected Handler: %s",
                   selected_handler->handler_name.c_str ());
      ImGui::TextDisabled ("System: %s",
                           selected_handler->system_type_name.c_str ());

      if (!selected_handler->component_types.empty ()) {
        ImGui::TextUnformatted ("Required Components:");
        for (const auto &component : selected_handler->component_types) {
          ImGui::BulletText ("%s", component.type_name.c_str ());
        }
      }

      if (selected_handler->entity_matches != nullptr) {
        ImGui::Spacing ();
        ImGui::TextUnformatted ("Target Entity");
        ImGui::InputTextWithHint (
            "##ConnectEntitySearch", "Search matching entities...",
            m_connect_entity_search, IM_ARRAYSIZE (m_connect_entity_search));

        // collect_matching_entities_for_handler
        std::vector<entt::entity> entities;
        for (auto ent : registry.view<entt::entity> ()) {
            if (selected_handler->matches_entity (registry, ent)) {
                const std::string &entity_name = scene->get_entity_name (ent);
                if (text_match (entity_name.c_str (), m_connect_entity_search)) {
                    entities.push_back (ent);
                }
            }
        }
        std::sort (entities.begin (), entities.end (), [&] (entt::entity a, entt::entity b) {
            return scene->get_entity_name (a) < scene->get_entity_name (b);
        });

        if (m_connect_target_entity != entt::null
            && !selected_handler->matches_entity (registry,
                                                  m_connect_target_entity)) {
          m_connect_target_entity = entt::null;
        }

        ImGui::BeginChild ("ConnectEntitiesList", ImVec2 (520.0F, 160.0F),
                           1);
        if (entities.empty ()) {
          ImGui::TextDisabled ("No entities match this handler.");
        } else {
          for (entt::entity const entity : entities) {
            const bool is_selected = m_connect_target_entity == entity;
            std::string label
                = scene->get_entity_name (entity) + " ("
                  + std::to_string (static_cast<uint32_t> (entity)) + ")";
            label += "##connect_entity_"
                     + std::to_string (static_cast<uint32_t> (entity));

            if (ImGui::Selectable (label.c_str (), is_selected)) {
              m_connect_target_entity = entity;
              m_connect_modal_error.clear ();
            }
          }
        }
        ImGui::EndChild ();
      }
    }
  }

  if (!m_connect_modal_error.empty ()) {
    ImGui::Spacing ();
    ImGui::TextColored (ImVec4 (1.0F, 0.45F, 0.45F, 1.0F), "%s",
                        m_connect_modal_error.c_str ());
  }

  // We need selected_handler here too.
  const wsl::reg::sig::signal_connectable_handler_debug_entry *selected_handler = nullptr;
  if (m_runtime_ctx->signal_hub.db != nullptr) {
      for (auto &h : m_runtime_ctx->signal_hub.db->connectable_handlers) {
          if (h.signal_type_id == m_connect_signal_type && h.system_type_id == m_connect_handler_system_type && h.handler_name == m_connect_handler_name) {
              selected_handler = &h;
              break;
          }
      }
  }

  const bool can_connect = (signal_entry != nullptr) && (selected_handler != nullptr)
                           && ((selected_handler->entity_matches == nullptr)
                               || (m_connect_target_entity != entt::null
                                   && selected_handler->matches_entity (
                                       registry, m_connect_target_entity)));

  if (!can_connect) {
    ImGui::BeginDisabled ();
  }

  if (ImGui::Button ("Connect")) {
    if (m_runtime_ctx->signal_hub.connect (
            m_connect_signal_type, m_connect_handler_system_type,
            m_connect_handler_name, m_connect_source_entity,
            m_connect_target_entity)) {
      m_connect_modal_error.clear ();
      ImGui::CloseCurrentPopup ();
    } else {
      m_connect_modal_error = "Could not create the signal connection with "
                            "the current selection.";
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
signal_inspector::draw ()
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts().bold);
  const bool open = ImGui::Begin ("Signals");
  ImGui::PopFont ();

  if (!open) {
    ImGui::End ();
    return;
  }

  auto *scene = m_runtime_ctx->scene_manager.get_active ();
  if (scene == nullptr) {
    draw_centered_icon (
        m_editor_ctx, m_editor_ctx->icon_signal, 128.0F,
        "Signals are the heartbeat of your game,\nMonitor and debug events as "
        "they flow through the system.");
    ImGui::End ();
    return;
  }

  auto &registry = scene->get_registry ();
  auto &queries = m_runtime_ctx->reg_queries;
  entt::id_type connect_requested_signal_type = 0;

  const bool has_system
      = (m_selection != nullptr) && m_selection->selected_system != nullptr;

  const bool has_entity
      = (m_selection != nullptr) && m_selection->kind == selection_kind::entity
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

    ImGui::TextUnformatted ("System Signals");
    ImGui::InputTextWithHint ("##SysSigSearch", "Search system signals...",
                              sys_sig_search, IM_ARRAYSIZE (sys_sig_search));

    ImGui::BeginChild ("SystemSignalsRegion", ImVec2 (0, system_signals_h), 1);

    {
      auto sigs = collect_signals_for_system (queries, sys_tid, sys_sig_search);
      if (sigs.empty ()) {
        ImGui::TextDisabled ("No signals declared for this system.");
      } else {
        draw_signal_table ("system_signals_table", sigs, m_selected_signal_type,
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

    bool has_any_handler = !queries.find_event_handlers_owned_by_system (sys_tid).empty();
    // We should also check connectable handlers in a unified way

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
        "##EntitySignalSearch", "Search signals inside expanded systems...",
        entity_sig_search, IM_ARRAYSIZE (entity_sig_search));
    ImGui::Separator ();

    auto matched_systems = queries.get_matching_systems (registry, ent);

    if (matched_systems.empty ()) {
      ImGui::TextDisabled (
          "No registered systems match this entity.");
      ImGui::End ();
      return;
    }

    for (const auto *sys_desc : matched_systems) {
      if (!text_match (sys_desc->display_name.c_str (), entity_sys_search)) {
        continue;
      }

      const std::string label
          = sys_desc->display_name + "##entity_system_"
            + std::to_string (sys_desc->type_id);

      if (ImGui::TreeNodeEx (label.c_str (),
                             ImGuiTreeNodeFlags_SpanAvailWidth)) {
        auto sigs = collect_signals_for_system (queries, sys_desc->type_id,
                                                entity_sig_search);

        if (sigs.empty ()) {
          ImGui::TextDisabled ("No signals declared for this system.");
        } else {
          draw_entity_signal_tree (*scene, registry, queries, sigs, ent,
                                   m_selected_signal_type,
                                   connect_requested_signal_type);
        }

        ImGui::TreePop ();
      }
    }

    if (connect_requested_signal_type != 0) {
      const entt::entity source_entity
          = m_runtime_ctx->signal_hub.has_signal_source (
                connect_requested_signal_type)
                ? ent
                : entt::null;
      open_signal_connection_modal (connect_requested_signal_type,
                                    source_entity);
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
