#pragma once

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

namespace wsl::log
{

/**
 * spdlog sink that forwards every log line to Tracy as a
 *        `TracyMessageLC` (coloured message).
 *
 * The level maps to a colour: trace/debug = gray, info = green,
 * warn = orange, error/critical = red. The message is prefixed with
 * the source logger name in brackets so the Messages column in
 * Tracy shows `[gfx] vkCreateBuffer failed: ...` etc.
 *
 * The sink is thread-safe; the underlying tracy::Profiler::Message*
 * calls are already lock-free per-thread, so the std::mutex only
 * serialises the formatting of the payload (which touches
 * std::string's small-buffer optimisation state and is not
 * reentrant).
 */
class tracy_sink : public spdlog::sinks::base_sink<std::mutex>
{
public:
  tracy_sink ();
  ~tracy_sink () override;

protected:
  void sink_it_ (const spdlog::details::log_msg &msg) override;
  void flush_ () override;
};

} // namespace wsl::log
