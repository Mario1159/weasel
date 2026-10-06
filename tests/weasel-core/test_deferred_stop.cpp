// M-regression: a play-session stop requested from a UI/render callback must be
// deferred to a frame boundary, not performed in place.

#include <doctest/doctest.h>

#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/log/log.hpp"

namespace
{
// Global log initializer, matching the other test targets: several engine paths
// log unconditionally, and the accessors segfault without init().
struct log_initializer
{
  log_initializer () { wsl::log::init (); }
};
static log_initializer init_log;
} // namespace

TEST_CASE ("a requested stop does not take effect until the frame boundary")
{
  // Regression test for the editor freezing when Stop was pressed.
  //
  // The Stop button lives in game_view::draw_camera_header, which runs as a UI
  // callback from inside core_systems::render_impl -- after the previous frame
  // was submitted but before this frame's command buffer is acquired. Calling
  // stop() there released the session renderer's GPU pipelines into SDL's
  // pending-destroy queue, and the very next begin_frame fence wait drained
  // that queue. On the RADON driver vkDestroyGraphicsPipeline then blocked
  // indefinitely, freezing the editor.
  //
  // request_stop() must therefore only flag the request; process_pending_stop()
  // runs it at the top of render_impl, before any GPU work is recorded.
  //
  // This test pins the contract itself: the stop must be observably deferred.
  // It fails if request_stop() ever grows a direct call to stop() again.

  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };
  runtime.set_in_play_session (true);
  REQUIRE (runtime.in_play_session ());

  runtime.request_stop ();

  // Deferred, not immediate: the session is still live on return.
  CHECK (runtime.in_play_session ());

  runtime.process_pending_stop ();
  CHECK_FALSE (runtime.in_play_session ());
}

TEST_CASE ("processing an unrequested stop does nothing")
{
  // The frame boundary calls process_pending_stop() unconditionally, so the
  // no-request path must be inert. If it ever tore the session down, every
  // frame would stop the play session regardless of user input.

  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };
  runtime.set_in_play_session (true);

  runtime.process_pending_stop ();
  CHECK (runtime.in_play_session ());

  // Idempotent when there is still nothing pending.
  runtime.process_pending_stop ();
  CHECK (runtime.in_play_session ());
}

TEST_CASE ("a consumed stop request is not replayed on later frames")
{
  // process_pending_stop() runs every frame. If the flag were left set, the
  // session would be torn down again on every subsequent frame.

  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };
  runtime.set_in_play_session (true);

  runtime.request_stop ();
  runtime.process_pending_stop ();
  CHECK_FALSE (runtime.in_play_session ());

  // Later frames must be inert. Re-arm the session and check it survives.
  runtime.set_in_play_session (true);
  runtime.process_pending_stop ();
  CHECK (runtime.in_play_session ());
}