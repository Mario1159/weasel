#include "ecs_inspector.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "events.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/event/event_hub.hpp"

namespace editor
{

ecs_inspector::ecs_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                              wsl::comp::singl::editor_context *editor_ctx,
                              ecs_selection &sel)
    : m_selection (sel), m_runtime_ctx (runtime_ctx),
      m_entities_and_singletons (runtime_ctx, editor_ctx, sel),
      m_inspector (runtime_ctx, editor_ctx, sel)
{
  // Observe `scene_changed` through the observer event hub (single eventing
  // mechanism). `ecs_inspector` is not an `ecs_system`, so the handler is
  // invoked via a captured `void *` owner.
  m_runtime_ctx->event_hub ()
      .declare_event_sink<wsl::event::scene_changed, ecs_inspector> (
          "on_scene_changed",
          +[] (void *owner, entt::registry &, const void *ev) {
            static_cast<ecs_inspector *> (owner)->on_scene_changed (
                *static_cast<const wsl::event::scene_changed *> (ev));
          },
          this);
  m_runtime_ctx->event_hub ().connect (
      wsl::comp::stable_type_id<wsl::event::scene_changed> (),
      wsl::comp::stable_type_id<ecs_inspector> (), "on_scene_changed");
}

ecs_inspector::~ecs_inspector ()
{
  m_runtime_ctx->event_hub ().disconnect (
      wsl::comp::stable_type_id<wsl::event::scene_changed> (),
      wsl::comp::stable_type_id<ecs_inspector> (), "on_scene_changed");
}

void
ecs_inspector::draw ()
{
  m_entities_and_singletons.draw ();
  m_inspector.draw ();
}

void
ecs_inspector::on_scene_changed (const wsl::event::scene_changed & /*unused*/)
{
  m_selection.clear_all ();
}

} // namespace editor
