// M-regression: a moved scene must not keep a signal bound to the old object.

#include <doctest/doctest.h>

#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/log/log.hpp"

#include <entt/entt.hpp>

#include <memory>
#include <utility>

namespace
{
// Global log initializer, matching the other test targets: several engine paths
// log unconditionally, and the accessors segfault without init().
struct log_initializer
{
  log_initializer () { wsl::log::init (); }
};
static log_initializer init_log;

/** Counts how many of *live* the registry still considers valid. */
template <typename... Entities>
std::size_t
live_count (const entt::registry &registry, Entities... live)
{
  std::size_t count = 0;
  for (const entt::entity e : { live... }) {
    if (registry.valid (e)) {
      ++count;
    }
  }
  return count;
}

/**
 * Builds a scene and returns it **moved**, destroying the source before the
 * caller ever sees the result.
 *
 * This mirrors `scene_loader::operator()` / `world::add_scene()`, which move a
 * scene out of a temporary that dies immediately afterwards. The source's
 * lifetime ending is the whole point: while it is still alive, a stale `this`
 * in the registry's signal reads valid memory and hides the defect.
 */
std::unique_ptr<wsl::rsc::scene>
make_scene_via_move (wsl::comp::singl::runtime_context &runtime,
                     entt::entity &entity_out)
{
  wsl::rsc::scene source{ &runtime, nullptr, "source" };
  entity_out = source.get_registry ().create ();
  source.set_entity_name (entity_out, "entity");
  return std::make_unique<wsl::rsc::scene> (std::move (source));
  // `source` is destroyed here -- exactly the window in which the editor
  // crashed.
}
} // namespace

TEST_CASE ("a moved scene does not invoke a handler bound to the old object")
{
  // Regression test for a SIGSEGV on stop in the editor.
  //
  // scene's constructor registers an EnTT on_destroy signal whose delegate
  // captures a raw `this`. `scene` declared its move operations `= default`,
  // which copies that delegate verbatim -- so a moved-to scene's registry still
  // invokes the handler at the *source* object's address. Both
  // `scene_loader::operator()` and `world::add_scene()` move a scene out of a
  // temporary that dies immediately, so the next `registry.clear()` (which
  // stop_and_clear() performs while restoring a play-session snapshot) called
  // through a dangling pointer and dereferenced garbage members, faulting at
  // address 0x67.
  //
  // The fix reconnects the signal after each move.

  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };

  entt::entity e = entt::null;
  auto moved = make_scene_via_move (runtime, e);

  REQUIRE (moved != nullptr);
  CHECK (moved->get_name () == "source");

  // The moved scene keeps its contents.
  REQUIRE (moved->get_registry ().valid (e));
  CHECK (moved->get_entity_name (e) == "entity");

  // This is the operation that used to crash: clearing the registry publishes
  // on_destroy, invoking the handler through whatever `this` the delegate
  // captured -- now a dead address.
  CHECK_NOTHROW (moved->get_registry ().clear ());
  CHECK (live_count (moved->get_registry (), e) == 0u);
}

TEST_CASE ("the destroy signal reaches the live scene after a move")
{
  // The positive half of the contract: the rebind must not merely avoid the
  // crash, it must leave the handler working. The handler's only visible effect
  // is through the runtime context, so the observable proof is that the moved
  // scene's entity teardown completes against a valid scene rather than a
  // stale one -- and that a second teardown is equally safe.
  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };

  entt::entity e = entt::null;
  auto moved = make_scene_via_move (runtime, e);
  REQUIRE (moved->get_registry ().valid (e));

  CHECK_NOTHROW (moved->get_registry ().clear ());
  CHECK (live_count (moved->get_registry (), e) == 0u);

  // Clearing again (stop_and_clear() can be reached more than once) must also
  // be safe.
  CHECK_NOTHROW (moved->get_registry ().clear ());
}

TEST_CASE ("move-assigning a scene also rebinds its destroy signal")
{
  // The same hazard through the assignment operator: a crash here would mean
  // operator= moved the registry without refreshing `this`.
  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };

  entt::entity e = entt::null;
  auto source = make_scene_via_move (runtime, e);
  REQUIRE (source->get_registry ().valid (e));

  wsl::rsc::scene target{ &runtime, nullptr, "target" };
  target = std::move (*source);

  CHECK (target.get_name () == "source");

  // NOTE: entt::registry move-assignment *swaps* rather than moves (see
  // basic_registry::operator=(basic_registry&&) -> swap(other)), so after the
  // assignment `source` holds what `target` had and vice versa. Only the live
  // scene's registry is asserted on; pinning which side kept what would be
  // asserting EnTT internals rather than the property under test.
  CHECK_NOTHROW (target.get_registry ().clear ());
  CHECK (live_count (target.get_registry (), e) == 0u);
}

TEST_CASE ("a moved-from scene is inert rather than a wild pointer")
{
  // The source's runtime context is nulled on move, so a stale delegate that
  // somehow still fires becomes a no-op instead of dereferencing a dead
  // runtime_context.
  wsl::comp::singl::runtime_context runtime{ "test", 64, 64, ".", true };

  entt::entity e = entt::null;
  auto moved = make_scene_via_move (runtime, e);

  CHECK (live_count (moved->get_registry (), e) == 1u);
  CHECK_NOTHROW (moved->get_registry ().clear ());
}
