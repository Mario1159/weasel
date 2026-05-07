#include "engine_resources.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include <string>
#include <vector>


namespace wsl
{

namespace comp::singl
{

engine_resources::engine_resources (comp::singl::runtime_context &runtime_ctx)
    : m_resource_manager (&runtime_ctx, runtime_ctx.resource_manager.get_engine_resource_path ())
{
}

void
engine_resources::set_editor_context (comp::singl::editor_context *editor_ctx)
{
  m_resource_manager.set_editor_context (editor_ctx);
}

void
engine_resources::update_async_uploads ()
{
  m_resource_manager.update_async_uploads ();
}

rsc::image_id
engine_resources::register_image (const std::string &path)
{
  return m_resource_manager.register_image (path);
}

engine_resources::image_handle
engine_resources::load (rsc::image_id id)
{
  return m_resource_manager.load (id);
}

engine_resources::image_handle
engine_resources::load_image (const std::string &path)
{
  rsc::image_id const id = register_image (path);
  return load (id);
}

engine_resources::image_handle
engine_resources::get (rsc::image_id id)
{
  return m_resource_manager.get (id);
}

rsc::image_state
engine_resources::state (rsc::image_id id) const
{
  return m_resource_manager.state (id);
}

bool
engine_resources::contains (rsc::image_id id) const
{
  return m_resource_manager.contains (id);
}

rsc::font_id
engine_resources::register_font (const std::string &path)
{
  return m_resource_manager.register_font (path);
}

std::vector<rsc::font_resource_info>
engine_resources::list_fonts () const
{
  return m_resource_manager.list_fonts ();
}

std::string
engine_resources::resolve_path (const std::string &path) const
{
  return m_resource_manager.resolve_path (path);
}

} // namespace comp::singl

} // namespace wsl
