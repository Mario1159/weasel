#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace wsl::log
{

void init ();

std::shared_ptr<spdlog::logger> system ();
std::shared_ptr<spdlog::logger> cmake ();
std::shared_ptr<spdlog::logger> resources ();

} // namespace wsl::log
