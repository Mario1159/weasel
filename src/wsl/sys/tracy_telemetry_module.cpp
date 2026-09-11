module;

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <tracy/Tracy.hpp>

module wsl.core;

namespace wsl::sys
{

namespace
{

// Snapshot function the application hands in; called from the
// background thread. May be null in which case the playback plots
// stay at 0.
runtime_snapshot_fn s_snapshot = nullptr;
std::atomic<bool> s_running{ false };
std::thread s_thread;

// Indices for the TracyParameterSetup call. Tracy doesn't care what
// these values are, but they must be unique per parameter. Keep them
// in a namespace so they can be used by both init and the parameter
// callback (if one is registered).
constexpr uint32_t kParamIndexIsRunning = 0;
constexpr uint32_t kParamIndexInPlaySession = 1;
constexpr uint32_t kParamIndexFrame = 2;

void
publish_parameters ([[maybe_unused]] uint64_t frame_index,
                    [[maybe_unused]] bool is_running,
                    [[maybe_unused]] bool in_play_session)
{
  TracyParameterSetup (kParamIndexFrame, "frame", false,
                       static_cast<int32_t> (frame_index));
  TracyParameterSetup (kParamIndexIsRunning, "is_running", false,
                       is_running ? 1 : 0);
  TracyParameterSetup (kParamIndexInPlaySession, "in_play_session", false,
                       in_play_session ? 1 : 0);
}

void
publish_app_info ()
{
  TracyAppInfo ("Weasel Engine", 14);
  TracyAppInfo ("Tracy integration: FrameMark + AllocN pools", 46);
}

void
telemetry_loop ()
{
  tracy::SetThreadName ("Tracy Telemetry");

  publish_app_info ();

  TracyPlotConfig ("playback.fps", tracy::PlotFormatType::Number,
                   /*step=*/false, /*fill=*/true, /*color=*/0x4CAF50);
  TracyPlotConfig ("playback.running", tracy::PlotFormatType::Number,
                   /*step=*/true, /*fill=*/false,
                   /*color=*/0xFF9800);

  using clock = std::chrono::steady_clock;
  auto next_tick = clock::now ();

  double smoothed_fps = 0.0;
  double const alpha = 0.3;

  while (s_running.load (std::memory_order_relaxed)) {
    next_tick += std::chrono::milliseconds (250);
    std::this_thread::sleep_until (next_tick);

    uint64_t frame_index = 0;
    bool is_running = false;
    bool in_play_session = false;
    double instant_fps = 0.0;
    if (s_snapshot != nullptr) {
      s_snapshot (frame_index, instant_fps, is_running, in_play_session);
    }
    if (instant_fps > 0.0) {
      smoothed_fps = (alpha * instant_fps) + ((1.0 - alpha) * smoothed_fps);
    }
    TracyPlot ("playback.fps", smoothed_fps);
    TracyPlot ("playback.running", is_running ? 1.0 : 0.0);

    publish_parameters (frame_index, is_running, in_play_session);
  }
}

} // namespace

void
tracy_telemetry_init (runtime_snapshot_fn snapshot_fn)
{
  if (s_running.load (std::memory_order_relaxed)) {
    return;
  }
  s_snapshot = snapshot_fn;
  s_running.store (true, std::memory_order_relaxed);
  s_thread = std::thread (telemetry_loop);
  wsl::log::core ()->debug ("Tracy telemetry: background tick started");
}

void
tracy_telemetry_shutdown ()
{
  if (!s_running.load (std::memory_order_relaxed)) {
    return;
  }
  s_running.store (false, std::memory_order_relaxed);
  if (s_thread.joinable ()) {
    s_thread.join ();
  }
  s_snapshot = nullptr;
  wsl::log::core ()->debug ("Tracy telemetry: background tick stopped");
}

void
tracy_telemetry_frame_mark (uint64_t frame_index)
{
  (void)frame_index;
  FrameMark;
}

void
tracy_telemetry_secondary_frame_begin ([[maybe_unused]] const char *name)
{
  FrameMarkStart (name);
}

void
tracy_telemetry_secondary_frame_end ([[maybe_unused]] const char *name)
{
  FrameMarkEnd (name);
}

} // namespace wsl::sys
