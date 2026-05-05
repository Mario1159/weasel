#pragma once

#include "../../rsc/resource_manager.hpp"

#include <string>

namespace wsl
{

namespace comp::singl
{

class runtime_context;
class editor_context;

class engine_resources
{
public:
  using image_handle = rsc::resource_manager::image_handle;

  explicit engine_resources (comp::singl::runtime_context &runtime_ctx);

  void set_editor_context (comp::singl::editor_context *editor_ctx);

  void update_async_uploads ();

  rsc::image_id register_image (const std::string &path);
  image_handle load (rsc::image_id id);
  image_handle load_image (const std::string &path);
  image_handle get (rsc::image_id id);
  rsc::image_state state (rsc::image_id id) const;
  bool contains (rsc::image_id id) const;

  rsc::font_id register_font (const std::string &path);
  std::vector<rsc::font_resource_info> list_fonts () const;
  std::string resolve_path (const std::string &path) const;

private:
  rsc::resource_manager m_resource_manager;
};

} // namespace comp::singl

} // namespace wsl
