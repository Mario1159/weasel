#include "world_transform.hpp"

#include "component_meta.hpp"
#include "singl/runtime_context.hpp"

#include <entt/entt.hpp>
#include <imgui.h>

namespace wsl
{

namespace comp
{

bool
world_transform::custom_inspect (const char * /*label*/,
                                 comp::singl::runtime_context * /*runtime*/)
{
  if (ImGui::TreeNodeEx ("Matrix", 0)) {
    for (int i = 0; i < 4; ++i) {
      ImGui::PushID (i);
      for (int j = 0; j < 4; ++j) {
        ImGui::PushID (j);
        float v = m_value[i][j];
        ImGui::SetNextItemWidth (ImGui::CalcItemWidth () / 4.1F);
        ImGui::InputFloat ("##v", &v, 0.0F, 0.0F, "%.3f",
                           ImGuiInputTextFlags_ReadOnly);
        if (j < 3)
          ImGui::SameLine ();
        ImGui::PopID ();
      }
      ImGui::PopID ();
    }
    ImGui::TreePop ();
  }
  return false; // read-only
}

void
world_transform::register_meta ()
{
  using namespace entt::literals;

  {
    auto &&factory
        = entt::meta_factory<comp::world_transform> ()
              .type (entt::type_hash<comp::world_transform>::value ())
              .custom<comp::meta_info> (
                  meta_info{ "World Transform",
                             "Computed world-space transform (read-only)",
                             "engine://icons/comp_world_transform.svg" });
    (factory.func<&comp::world_transform::custom_inspect>)("custom_inspect"_hs);
  }

  entt::meta_factory<comp::world_transform> ()
      .data<&comp::world_transform::m_value> ("matrix"_hs)
      .custom<comp::meta_info> (meta_info{
          "Matrix", "Final world matrix after hierarchy evaluation", "" });
}

} // namespace comp

} // namespace wsl
