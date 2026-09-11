module;
#include <cstdint>
#include <string>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

module wsl.core;

namespace wsl::log
{

namespace
{

uint32_t
level_color (spdlog::level::level_enum lvl)
{
  switch (lvl) {
  case spdlog::level::trace:
    return 0xFF90A4AE;
  case spdlog::level::debug:
    return 0xFF9E9E9E;
  case spdlog::level::info:
    return 0xFF4CAF50;
  case spdlog::level::warn:
    return 0xFFFFA726;
  case spdlog::level::err:
    return 0xFFEF5350;
  case spdlog::level::critical:
    return 0xFFD50000;
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

  TracyMessageC (formatted.c_str (), formatted.size (),
                 level_color (msg.level));
}

void
tracy_sink::flush_ ()
{
}

} // namespace wsl::log
