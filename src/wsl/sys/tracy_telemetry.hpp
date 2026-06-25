#pragma once

#include <cstdint>

namespace wsl::sys
{

/*!
 * \brief Start the periodic Tracy telemetry tick.
 *
 * Spawns a background thread that, every ~250 ms, pushes:
 *   - `mem.rss_mb`  : resident set size in MB (ProcessMemory form)
 *   - `mem.virt_mb` : virtual size in MB
 *   - `playback.fps`: smoothed frames-per-second (instantaneous FPS as
 *                     reported by the runtime, not a wallclock average)
 *   - `playback.running`: 0/1 — the engine's play state
 *
 * `runtime_ctx_fn` is a function pointer invoked from the telemetry
 * thread to fetch the current frame index, FPS, and play state. The
 * caller is responsible for keeping the underlying context alive.
 *
 * The thread is shut down by @ref tracy_telemetry::shutdown(), which
 * the application calls from its destructor. Safe to call multiple
 * times; the second call is a no-op.
 */
using runtime_snapshot_fn = void (*) (uint64_t &frame_index, double &fps,
                                      bool &is_running, bool &in_play_session);
void tracy_telemetry_init (runtime_snapshot_fn snapshot_fn);
void tracy_telemetry_shutdown ();

/*!
 * \brief Force-push the current frame counter as a Tracy frame marker.
 *
 * Should be called once per rendered frame, after the command buffer
 * is submitted. Tracy computes the frame time automatically from the
 * gap between consecutive frame marks; the per-frame index is shown
 * in the Frame view label so a capture is self-describing.
 */
void tracy_telemetry_frame_mark (uint64_t frame_index);

/*!
 * \brief Open a secondary frame set named @p name.
 *
 * Tracy's Frame view supports multiple frame sets. The main one is
 * driven by @ref tracy_telemetry_frame_mark (one entry per render
 * loop). Use the secondary frame APIs to mark disjoint work units
 * — e.g. one "Update" frame and one "Render" frame per loop
 * iteration — that the profiler can show side-by-side.
 *
 * The string @p name must be a string literal whose address is
 * unique per frame set (Tracy interns it by pointer).
 */
void tracy_telemetry_secondary_frame_begin (const char *name);

/*!
 * \brief Close a secondary frame set opened by
 *        @ref tracy_telemetry_secondary_frame_begin.
 *
 * Must be called exactly once per begin, in the same thread, in
 * the correct LIFO order if nested.
 */
void tracy_telemetry_secondary_frame_end (const char *name);

} // namespace wsl::sys
