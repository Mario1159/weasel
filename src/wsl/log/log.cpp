#include "log.hpp"
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace wsl::log
{

static std::shared_ptr<spdlog::logger> s_system_logger;
static std::shared_ptr<spdlog::logger> s_cmake_logger;
static std::shared_ptr<spdlog::logger> s_resources_logger;

void
init ()
{
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();
  
  s_system_logger = std::make_shared<spdlog::logger> ("system", stdout_sink);
  s_cmake_logger = std::make_shared<spdlog::logger> ("cmake", stdout_sink);
  s_resources_logger = std::make_shared<spdlog::logger> ("resources", stdout_sink);

  spdlog::register_logger (s_system_logger);
  spdlog::register_logger (s_cmake_logger);
  spdlog::register_logger (s_resources_logger);

  spdlog::set_default_logger (s_system_logger);
  spdlog::set_level (spdlog::level::debug);
}

std::shared_ptr<spdlog::logger>
system ()
{
  return s_system_logger;
}

std::shared_ptr<spdlog::logger>
cmake ()
{
  return s_cmake_logger;
}

std::shared_ptr<spdlog::logger>
resources ()
{
  return s_resources_logger;
}

} // namespace wsl::log
