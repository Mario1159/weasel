#include "tracy_sink.hpp"

#include <tracy/Tracy.hpp>

namespace wsl::log
{

namespace
{

// Map spdlog level to a Tracy color (0xAARRGGBB).
uint32_t
level_color (spdlog::level::level_enum lvl)
{
  switch (lvl) {
  case spdlog::level::trace:
    return 0xFF90A4AE; // blue-gray
  case spdlog::level::debug:
    return 0xFF9E9E9E; // gray
  case spdlog::level::info:
    return 0xFF4CAF50; // green
  case spdlog::level::warn:
    return 0xFFFFA726; // orange
  case spdlog::level::err:
    return 0xFFEF5350; // red
  case spdlog::level::critical:
    return 0xFFD50000; // dark red
  default:
    return 0xFFFFFFFF;
  }
}

const char *
level_name (spdlog::level::level_enum lvl)
{
  switch (lvl) {
  case spdlog::level::trace:
    return "TRACE";
  case spdlog::level::debug:
    return "DEBUG";
  case spdlog::level::info:
    return "INFO";
  case spdlog::level::warn:
    return "WARN";
  case spdlog::level::err:
    return "ERROR";
  case spdlog::level::critical:
    return "CRIT";
  default:
    return "LOG";
  }
}

} // namespace

tracy_sink::tracy_sink () = default;
tracy_sink::~tracy_sink () = default;

void
tracy_sink::sink_it_ (const spdlog::details::log_msg &msg)
{
  // Build "[LEVEL][LOGGER] payload". Tracy's Messages column shows
  // the truncated prefix, so the level and logger go first.
  // spdlog's payload is a fmt::string_view (or std::string_view
  // depending on SPDLOG_FMT_EXTERNAL); copy through std::string to
  // avoid the conversion ambiguity.
  std::string logger_name (msg.logger_name.data (), msg.logger_name.size ());
  std::string payload (msg.payload.data (), msg.payload.size ());

  std::string formatted;
  formatted.reserve (payload.size () + logger_name.size () + 16);
  formatted.append ("[").append (level_name (msg.level)).append ("]");
  if (!logger_name.empty ()) {
    formatted.append ("[").append (logger_name).append ("] ");
  } else {
    formatted.append (" ");
  }
  formatted.append (payload);

  // TracyMessageC (the non-L variant) is the correct macro for
  // dynamic strings: it takes (text, size, color) and copies the
  // data into a Tracy-owned buffer. The L variant (`TracyMessageLC`,
  // signature: (text, color)) does NOT copy — it stores the raw
  // pointer and expects the string to live for the entire program
  // (i.e. a string literal). Passing a temporary `std::string::c_str()`
  // to TracyMessageLC produces the "corrupted" message text the user
  // saw: Tracy reads freed heap memory when the queue entry is
  // drained by the client thread.
  //
  // `TracyLogString` (which adds a proper severity enum instead of
  // just a colour) is only in the master branch; v0.13.1 doesn't
  // ship it. When Tracy is upgraded to >= 0.15, replace this with:
  //
  //   TracyLogString( <MessageSeverity>, color, 0, formatted.size(),
  //                   formatted.c_str() );
  TracyMessageC (formatted.c_str (), formatted.size (),
                 level_color (msg.level));
}

void
tracy_sink::flush_ ()
{
  // Tracy's queue is drained on the client thread; no per-sink flush
  // is needed.
}

} // namespace wsl::log
