#include "log.hpp"
#include "tracy_sink.hpp"
#include <memory>
#include <cstdint>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace wsl::log
{

static std::shared_ptr<spdlog::logger> s_core_logger;
static std::shared_ptr<spdlog::logger> s_gfx_logger;
static std::shared_ptr<spdlog::logger> s_rsc_logger;
static std::shared_ptr<spdlog::logger> s_sys_logger;
static std::shared_ptr<spdlog::logger> s_editor_logger;
static std::shared_ptr<spdlog::logger> s_cli_logger;
static std::shared_ptr<spdlog::logger> s_phys_logger;
static std::shared_ptr<spdlog::logger> s_net_logger;
static std::shared_ptr<spdlog::logger> s_xmake_logger;

#ifdef _WIN32
namespace
{
// Map "\033[3Xm" to Win32 console attributes (FOREGROUND_BLUE 0x1 |
// FOREGROUND_GREEN 0x2 | FOREGROUND_RED 0x4). Numeric constants avoid
// <windows.h> here.
std::uint16_t
wincolor_for_ansi (const char *code)
{
  unsigned attr = 7; // white
  if (code[0] == '\033' && code[1] == '[' && code[3] == 'm') {
    switch (code[2]) {
    case '1':
      attr = 4;
      break; // red
    case '2':
      attr = 2;
      break; // green
    case '3':
      attr = 6;
      break; // yellow
    case '4':
      attr = 1;
      break; // blue
    case '5':
      attr = 5;
      break; // magenta
    case '6':
      attr = 3;
      break; // cyan
    case '7':
      attr = 7;
      break; // white
    default:
      break;
    }
  }
  return static_cast<std::uint16_t> (attr);
}
} // namespace
#endif

static std::shared_ptr<spdlog::logger>
make_logger (const char *name, const char *info_color)
{
  // stdout_color_sink_mt is ansicolor on POSIX but wincolor on Windows.
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();
  stdout_sink->set_pattern ("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
#ifdef _WIN32
  // wincolor set_color takes console attributes, not ANSI codes.
  stdout_sink->set_color (spdlog::level::info, wincolor_for_ansi (info_color));
#else
  stdout_sink->set_color (spdlog::level::info, info_color);
#endif
  stdout_sink->set_color (spdlog::level::info, info_color);

  // Each logger also writes into Tracy (Tracy messages column).
  // Tracy is enabled at compile time via the TRACY_ENABLE define
  // pulled in by the existing wsl CMake link to TracyClient; when
  // Tracy is disabled the macros compile to no-ops so this sink
  // is harmless overhead.
  auto tracy_sink_ptr = std::make_shared<wsl::log::tracy_sink> ();

  // spdlog takes a vector of sinks; the first one is the "primary"
  // for error-message formatting.
  std::vector<spdlog::sink_ptr> sinks{ stdout_sink, tracy_sink_ptr };
  auto logger
      = std::make_shared<spdlog::logger> (name, sinks.begin (), sinks.end ());
  spdlog::register_logger (logger);
  return logger;
}

void
init ()
{
  if (s_core_logger)
    return;
  s_core_logger = make_logger ("core", "\033[37m");
  s_gfx_logger = make_logger ("gfx", "\033[32m");
  s_rsc_logger = make_logger ("rsc", "\033[36m");
  s_sys_logger = make_logger ("sys", "\033[35m");
  s_editor_logger = make_logger ("editor", "\033[33m");
  s_cli_logger = make_logger ("cli", "\033[34m");
  s_phys_logger = make_logger ("phys", "\033[31m");
  s_net_logger = make_logger ("net", "\033[34m");
  s_xmake_logger = make_logger ("xmake", "\033[37m");

  spdlog::set_default_logger (s_core_logger);
  spdlog::set_level (spdlog::level::debug);
}

std::shared_ptr<spdlog::logger>
core ()
{
  return s_core_logger;
}

std::shared_ptr<spdlog::logger>
gfx ()
{
  return s_gfx_logger;
}

std::shared_ptr<spdlog::logger>
rsc ()
{
  return s_rsc_logger;
}

std::shared_ptr<spdlog::logger>
sys ()
{
  return s_sys_logger;
}

std::shared_ptr<spdlog::logger>
editor ()
{
  return s_editor_logger;
}

std::shared_ptr<spdlog::logger>
cli ()
{
  return s_cli_logger;
}

std::shared_ptr<spdlog::logger>
phys ()
{
  return s_phys_logger;
}

std::shared_ptr<spdlog::logger>
net ()
{
  return s_net_logger;
}

std::shared_ptr<spdlog::logger>
xmake ()
{
  return s_xmake_logger;
}

} // namespace wsl::log
