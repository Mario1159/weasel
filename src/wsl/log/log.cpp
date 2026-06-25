#include "log.hpp"
#include "tracy_sink.hpp"
#include <memory>
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
static std::shared_ptr<spdlog::logger> s_cmake_logger;

static std::shared_ptr<spdlog::logger>
make_logger (const char *name, const char *info_color)
{
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();
  stdout_sink->set_pattern ("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
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
  s_cmake_logger = make_logger ("cmake", "\033[37m");

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
cmake ()
{
  return s_cmake_logger;
}

} // namespace wsl::log
