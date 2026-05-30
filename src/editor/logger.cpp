#include "logger.hpp"
#include "imgui_internal.h"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"

#include "wsl/log/log.hpp"
#include <cstddef>
#include <imgui.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <algorithm>
#include <string>
#include <string_view>
#include "textselect.hpp"

namespace editor
{

editor::logger::logger (wsl::comp::singl::editor_context *editor_ctx)
    : m_editor_ctx (editor_ctx),
      m_text_select (
          [this] (std::size_t idx) -> std::string_view {
            return m_entries[idx].message;
          },
          [this] () -> std::size_t { return m_entries.size (); })
{
}

editor::logger::~logger ()
{
  for (auto &sink : m_sinks) {
    auto loggers_list
        = { wsl::log::core (), wsl::log::gfx (),    wsl::log::rsc (),
            wsl::log::sys (),  wsl::log::editor (), wsl::log::cli (),
            wsl::log::phys (), wsl::log::net (),    wsl::log::cmake () };
    for (const auto &l : loggers_list) {
      if (l) {
        l->sinks ().erase (
            std::remove (l->sinks ().begin (), l->sinks ().end (), sink),
            l->sinks ().end ());
      }
    }
  }
}

void
editor::logger::attach_to_spdlog ()
{
  if (!m_sinks.empty ()) {
    return;
  }

  auto create_sink = [&] (const std::shared_ptr<spdlog::logger> &l) {
    auto sink = std::make_shared<logger_sink> (*this);
    sink->set_formatter (
        std::make_unique<spdlog::pattern_formatter> ("%H:%M:%S [%^%l%$] %v"));
    l->sinks ().push_back (sink);
    m_sinks.push_back (sink);
  };

  create_sink (wsl::log::core ());
  create_sink (wsl::log::gfx ());
  create_sink (wsl::log::rsc ());
  create_sink (wsl::log::sys ());
  create_sink (wsl::log::editor ());
  create_sink (wsl::log::cli ());
  create_sink (wsl::log::phys ());
  create_sink (wsl::log::net ());
  create_sink (wsl::log::cmake ());
}

void
editor::logger::clear ()
{
  m_entries.clear ();
  m_text_select.clearSelection ();
}

void
editor::logger::set_auto_scroll (bool value)
{
  m_auto_scroll = value;
}

void
editor::logger::add_log (spdlog::level::level_enum level,
                         const std::string &category, const std::string &text)
{
  m_entries.push_back ({ level, category, text });
  m_scroll_to_bottom = true;
}

void
editor::logger::draw (const char *title, bool *open)
{
  if (!ImGui::Begin (title, open)) {
    ImGui::End ();
    return;
  }

  if (ImGui::Button ("Clear")) {
    clear ();
  }

  ImGui::SameLine ();
  ImGui::Checkbox ("Auto-scroll", &m_auto_scroll);

  ImGui::SameLine ();
  ImGui::SeparatorEx (ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine ();

  ImGui::SetNextItemWidth (120);
  if (ImGui::BeginCombo ("##sink_select", m_current_category.c_str ())) {
    const char *categories[] = { "All",    "core", "gfx",  "rsc", "sys",
                                 "editor", "cli",  "phys", "net", "cmake" };
    for (int i = 0; i < 10; ++i) {
      bool const is_selected = (m_current_category == categories[i]);
      if (ImGui::Selectable (categories[i], is_selected)) {
        m_current_category = categories[i];
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }
    ImGui::EndCombo ();
  }

  ImGui::SameLine ();
  ImGui::SeparatorEx (ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine ();

  ImGui::Checkbox ("T", &m_show_trace);
  ImGui::SameLine ();
  ImGui::Checkbox ("D", &m_show_debug);
  ImGui::SameLine ();
  ImGui::Checkbox ("I", &m_show_info);
  ImGui::SameLine ();
  ImGui::Checkbox ("W", &m_show_warn);
  ImGui::SameLine ();
  ImGui::Checkbox ("E", &m_show_error);
  ImGui::SameLine ();
  ImGui::Checkbox ("C", &m_show_critical);

  ImGui::Separator ();

  ImGui::PushStyleVar (ImGuiStyleVar_WindowPadding, ImVec2 (8, 8));
  ImGui::BeginChild ("console_scrolling_region", ImVec2 (0, 0), 1,
                     ImGuiWindowFlags_HorizontalScrollbar
                         | ImGuiWindowFlags_NoMove);

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->get_fonts ().mono);

  // Render visible lines
  for (size_t i = 0; i < m_entries.size (); ++i) {
    const log_entry &entry = m_entries[i];
    if (!is_level_visible (entry.level)) {
      continue;
    }
    if (!is_category_visible (entry.category)) {
      continue;
    }

    ImGui::PushStyleColor (ImGuiCol_Text, level_color (entry.level));
    ImGui::TextUnformatted (entry.message.c_str ());
    ImGui::PopStyleColor ();
  }

  // Update text selection logic
  m_text_select.update ();

  // Optional context menu
  if (ImGui::BeginPopupContextWindow ()) {
    ImGui::BeginDisabled (!m_text_select.hasSelection ());
    if (ImGui::MenuItem ("Copy", "Ctrl+C")) {
      m_text_select.copy ();
    }
    ImGui::EndDisabled ();

    if (ImGui::MenuItem ("Select all", "Ctrl+A")) {
      m_text_select.selectAll ();
    }

    if (ImGui::MenuItem ("Clear selection")) {
      m_text_select.clearSelection ();
    }

    ImGui::EndPopup ();
  }

  ImGui::PopFont ();

  if (m_scroll_to_bottom && m_auto_scroll) {
    ImGui::SetScrollHereY (1.0F);
  }

  m_scroll_to_bottom = false;

  ImGui::EndChild ();
  ImGui::PopStyleVar ();

  ImGui::End ();
}

ImVec4
editor::logger::level_color (spdlog::level::level_enum level)
{
  switch (level) {
  case spdlog::level::trace:
    return ImVec4 (0.6F, 0.6F, 0.6F, 1.0F);
  case spdlog::level::debug:
    return ImVec4 (0.4F, 0.7F, 1.0F, 1.0F);
  case spdlog::level::info:
    return ImVec4 (0.8F, 0.8F, 0.8F, 1.0F);
  case spdlog::level::warn:
    return ImVec4 (1.0F, 0.8F, 0.3F, 1.0F);
  case spdlog::level::err:
    return ImVec4 (1.0F, 0.4F, 0.4F, 1.0F);
  case spdlog::level::critical:
    return ImVec4 (1.0F, 0.0F, 0.0F, 1.0F);
  default:
    return ImVec4 (1, 1, 1, 1);
  }
}

editor::logger::logger_sink::logger_sink (logger &owner) : m_owner (owner) {}

void
editor::logger::logger_sink::sink_it_ (const spdlog::details::log_msg &msg)
{
  spdlog::memory_buf_t formatted;
  formatter_->format (msg, formatted);

  m_owner.add_log (
      msg.level, std::string (msg.logger_name.data (), msg.logger_name.size ()),
      std::string (formatted.data (), formatted.size ()));
}

bool
editor::logger::is_level_visible (spdlog::level::level_enum level) const
{
  using lvl = spdlog::level::level_enum;

  switch (level) {
  case lvl::trace:
    return m_show_trace;
  case lvl::debug:
    return m_show_debug;
  case lvl::info:
    return m_show_info;
  case lvl::warn:
    return m_show_warn;
  case lvl::err:
    return m_show_error;
  case lvl::critical:
    return m_show_critical;
  default:
    return true;
  }
}

bool
editor::logger::is_category_visible (const std::string &category) const
{
  return m_current_category == "All" || m_current_category == category;
}

} // namespace editor
