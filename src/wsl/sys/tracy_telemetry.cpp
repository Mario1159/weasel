#include "tracy_telemetry.hpp"

#include "../log/log.hpp"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <tracy/Tracy.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

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
publish_parameters (uint64_t frame_index, bool is_running, bool in_play_session)
{
  // v0.13.1 takes a `bool isBool` flag rather than an enum.
  // Setup is idempotent — Tracy re-broadcasts on every connection
  // and ignores repeat values. We call it every tick to push
  // live updates of the int parameters (isBool = false → int).
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
  // Visible in the trace description (Trace > Information panel).
  // Tracy concatenates successive AppInfo messages into a single
  // text block.
  TracyAppInfo ("Weasel Engine", 14);
  TracyAppInfo ("Tracy integration: FrameMark + AllocN pools", 46);
}

void
telemetry_loop ()
{
  // Label this thread for Tracy. The docs require a string literal
  // — the API stores the pointer and expects the bytes to live for
  // the lifetime of the process. Without this, the background tick
  // shows up as a numeric thread ID (or as "weasel", from the
  // process name) which is confusing in the Threads pane.
  tracy::SetThreadName ("Tracy Telemetry");

  publish_app_info ();

  // Configure the playback plots. Memory is now reported via
  // TracyAllocN / TracyFreeN into the Memory tab, not as RSS
  // plots; the simple rss_mb / virt_mb plots are gone.
  TracyPlotConfig ("playback.fps", tracy::PlotFormatType::Number,
                   /*step=*/false, /*fill=*/true, /*color=*/0x4CAF50);
  TracyPlotConfig ("playback.running", tracy::PlotFormatType::Number,
                   /*step=*/true, /*fill=*/false,
                   /*color=*/0xFF9800);

  using clock = std::chrono::steady_clock;
  auto next_tick = clock::now ();

  // Smoothed FPS computed from a simple EWMA of the snapshot's
  // reported FPS. FPS itself comes from the runtime's
  // instantaneous value (dt-based), so smoothing here just kills
  // single-frame spikes.
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

    // Push runtime parameters into the profiler. The connection
    // popup shows these as numeric / boolean fields that can be
    // edited at runtime (the edits do not propagate back to the
    // engine in this v0.13.1 setup; that's fine for read-only
    // inspection).
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
  // Primary frame marker.
  //
  // IMPORTANT: must use the null-name `FrameMark` macro, not
  // `FrameMarkNamed(label)`. In Tracy v0.13.1 the engine's internal
  // `m_frameCount` (which FrameImage uses to compute the per-frame
  // image association) is only incremented when the frame mark has
  // a null name. With a named frame mark the counter stays at 0
  // forever and every FrameImage is dropped by the server's
  // `m_onDemand && fidx <= 1` check, leaving the Playback window
  // empty. Same bug exists in upstream master.
  //
  // The "Frame N" label is therefore not shown in the Tracy Frame
  // view; the per-frame index is available in the engine's own log
  // and is implicit in the frame number column Tracy renders.
  // `frame_index` is accepted as a parameter only so the call site
  // can stay self-documenting.
  (void)frame_index;
  FrameMark;
}

void
tracy_telemetry_secondary_frame_begin (const char *name)
{
  // Open a secondary frame set. Tracy displays these as a separate
  // row in the Frame view. Must be paired with a matching
  // tracy_telemetry_secondary_frame_end() call.
  FrameMarkStart (name);
}

void
tracy_telemetry_secondary_frame_end (const char *name)
{
  FrameMarkEnd (name);
}

} // namespace wsl::sys
