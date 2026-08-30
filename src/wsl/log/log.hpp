#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <spdlog/spdlog.h>
#endif
#if !defined(WSL_MODULE_BUILD)
#include <memory>
#endif

namespace wsl::log
{

void init ();

std::shared_ptr<spdlog::logger> core ();
std::shared_ptr<spdlog::logger> gfx ();
std::shared_ptr<spdlog::logger> rsc ();
std::shared_ptr<spdlog::logger> sys ();
std::shared_ptr<spdlog::logger> editor ();
std::shared_ptr<spdlog::logger> cli ();
std::shared_ptr<spdlog::logger> phys ();
std::shared_ptr<spdlog::logger> net ();
std::shared_ptr<spdlog::logger> cmake ();

} // namespace wsl::log
