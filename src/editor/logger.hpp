#pragma once

#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <textselect.hpp>

#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace wsl::comp::singl { class editor_context; }

namespace editor
{

class logger
{
public:
  explicit logger (wsl::comp::singl::editor_context *editor_ctx);
  ~logger ();

  // attach this logger to spdlog
  void attach_to_spdlog ();

  // draw the imgui window
  void draw (const char *title, bool *open = nullptr);

  // clear stored loggers
  void clear ();

  // enable or disable auto scroll
  void set_auto_scroll (bool value);

private:
  struct log_entry
  {
    spdlog::level::level_enum level;
    std::string category;
    std::string message;
  };

  class logger_sink : public spdlog::sinks::base_sink<std::mutex>
  {
  public:
    explicit logger_sink (logger &owner);

  protected:
    void sink_it_ (const spdlog::details::log_msg &msg) override;
    void
    flush_ () override
    {
    }

  private:
    logger &m_owner;
  };

  void add_log (spdlog::level::level_enum level, const std::string &category,
                const std::string &text);
  static ImVec4 level_color (spdlog::level::level_enum level) ;


  bool is_level_visible (spdlog::level::level_enum level) const;
  bool is_category_visible (const std::string &category) const;

  wsl::comp::singl::editor_context *m_editor_ctx;

  std::deque<log_entry> m_entries;
  bool m_auto_scroll = true;
  bool m_scroll_to_bottom = false;

  std::vector<std::shared_ptr<logger_sink>> m_sinks;

  std::string m_current_category = "system";

  bool m_show_trace = true;
  bool m_show_debug = true;
  bool m_show_info = true;
  bool m_show_warn = true;
  bool m_show_error = true;
  bool m_show_critical = true;

  TextSelect m_text_select;
};

} // namespace editor
