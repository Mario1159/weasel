#include "ecs_inspector.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "events.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"

namespace editor
{

ecs_inspector::ecs_inspector (wsl::comp::singl::runtime_context *runtime_ctx,
                              wsl::comp::singl::editor_context *editor_ctx,
                              ecs_selection &sel)
    : m_selection (sel), m_runtime_ctx (runtime_ctx),
      m_entities_and_singletons (runtime_ctx, editor_ctx, sel),
      m_inspector (runtime_ctx, editor_ctx, sel)
{
  auto sink = (runtime_ctx->dispatcher().sink<wsl::event::scene_changed>)();
  (sink.connect<&ecs_inspector::on_scene_changed>)(*this);
}

ecs_inspector::~ecs_inspector ()
{
  auto sink = (m_runtime_ctx->dispatcher().sink<wsl::event::scene_changed>)();
  (sink.disconnect<&ecs_inspector::on_scene_changed>)(*this);
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
