#include "inspector.hpp"

#include "comp/component_meta.hpp"
#include "comp/singl/ui_manager.hpp"
#include "gfx/image.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/prefab_instance.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "ecs_inspector_utils.hpp"

#include <cstdint>
#include <entt/core/fwd.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/resolve.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <cstdio>
#include <entt/meta/container.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/pointer.hpp>
#include <imgui.h>
#include <string>
#include <vector>

namespace editor
{

static entt::registry &
get_singleton_inspector_registry (wsl::comp::singl::runtime_context &runtime_ctx,
                                  wsl::comp::singl::editor_context &editor_ctx,
                                  entt::registry &fallback_registry)
{
  if (wsl::rsc::scene *scene = runtime_ctx.scene_manager.get_active ()) {
    return scene->get_registry ();
  }

  auto &ctx = fallback_registry.ctx ();

  if (ctx.template contains<wsl::comp::singl::runtime_context *> ()) {
    ctx.erase<wsl::comp::singl::runtime_context *> ();
  }
  ctx.emplace<wsl::comp::singl::runtime_context *> (&runtime_ctx);

  if (ctx.template contains<wsl::comp::singl::editor_context *> ()) {
    ctx.erase<wsl::comp::singl::editor_context *> ();
  }
  ctx.emplace<wsl::comp::singl::editor_context *> (&editor_ctx);

  if (ctx.template contains<wsl::rsc::scene_manager *> ()) {
    ctx.erase<wsl::rsc::scene_manager *> ();
  }
  ctx.emplace<wsl::rsc::scene_manager *> (&runtime_ctx.scene_manager);

  if (ctx.template contains<wsl::rsc::resource_manager_view *> ()) {
    ctx.erase<wsl::rsc::resource_manager_view *> ();
  }
  ctx.emplace<wsl::rsc::resource_manager_view *> (&runtime_ctx.resource_manager_view);

  if (ctx.template contains<wsl::comp::singl::ui_manager *> ()) {
    ctx.erase<wsl::comp::singl::ui_manager *> ();
  }
  ctx.emplace<wsl::comp::singl::ui_manager *> (&runtime_ctx.ui_manager);
  runtime_ctx.singleton_registry.apply_core_singletons (fallback_registry);

  return fallback_registry;
}

#define runtime_ctx m_runtime_ctx

enum class editor_trait : uint16_t
{
  none = 0,
  read_only = 1 << 0,
  hidden = 1 << 1
};

static constexpr editor_trait
operator& (editor_trait a, editor_trait b) noexcept
{
  return static_cast<editor_trait> (static_cast<uint16_t> (a)
                                    & static_cast<uint16_t> (b));
}

inspector::inspector (
    wsl::comp::singl::runtime_context *runtime_ctx,
    wsl::comp::singl::editor_context *editor_ctx, ecs_selection &selected)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx), m_selection (selected)
{
}

static std::string
get_icon_path (const entt::meta_type &meta)
{
  return wsl::comp::meta_icon_path (meta);
}

static void
draw_system_name_list (const char *label, const std::vector<std::string> &items)
{
  ImGui::TextUnformatted (label);

  if (items.empty ()) {
    ImGui::TextDisabled ("None");
    return;
  }

  ImGui::Indent ();
  for (const std::string &item : items) {
    ImGui::BulletText ("%s", item.c_str ());
  }
  ImGui::Unindent ();
}

void
inspector::draw ()
{
  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.bold);
  bool const open = ImGui::Begin ("Inspector");
  ImGui::PopFont ();

  if (!open) {
    ImGui::End ();
    return;
  }

  if (m_selection.selected_system != nullptr) {
    draw_system_inspector (m_selection.selected_system);
  } else {
    switch (m_selection.kind) {
    case selection_kind::entity:
      if ((runtime_ctx->scene_manager.get_active () != nullptr)
          && runtime_ctx->scene_manager.get_active ()->get_registry ().valid (
              m_selection.selected_entity)) {
        draw_entity_inspector (m_selection.selected_entity);
      } else {
        draw_centered_icon (
            m_editor_ctx, m_editor_ctx->icon_inspector, 128.0F,
            "Select a system, entity or singleton component\nto inspect and "
            "edit their fields.");
      }
      break;

    case selection_kind::singleton:
      draw_singleton_inspector (m_selection.singleton_type);
      break;

    default:
      draw_centered_icon (
          m_editor_ctx, m_editor_ctx->icon_inspector, 128.0F,
          "Select a system, entity or singleton component\nto inspect and "
          "edit their fields.");      break;
    }
  }

  ImGui::End ();
}

void
inspector::draw_system_inspector (wsl::sys::ecs_system *system)
{
  if (system == nullptr) {
    ImGui::TextDisabled ("No system selected");
    return;
  }

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.semibold);
  ImGui::TextUnformatted (system->get_name ().c_str ());
  ImGui::PopFont ();

  ImGui::TextDisabled ("%s", system->get_type_name ());
  ImGui::Separator ();

  entt::registry *registry = nullptr;
  if (wsl::rsc::scene *scene = runtime_ctx->scene_manager.get_active ()) {
    registry = &scene->get_registry ();
  }

  bool init_on_startup = system->is_active () && true; // Use getter
  if (ImGui::Checkbox ("Init On Startup", &init_on_startup)) {
    system->set_init_on_startup (init_on_startup, registry,
                                 runtime_ctx->is_running);
  }

  bool editor_active = system->is_active ();
  if (ImGui::Checkbox ("Editor Active", &editor_active)) {
    system->set_editor_active (editor_active, registry,
                               runtime_ctx->is_running);
  }

  ImGui::TextDisabled ("Current State: %s",
                       system->is_active () ? "Active" : "Inactive");

  ImGui::Spacing ();
  draw_system_name_list ("Dependencies", system->get_dependencies ());
  ImGui::Spacing ();
  draw_system_name_list ("Conflicts", system->get_conflicts ());
  ImGui::Spacing ();
  ImGui::Separator ();
  ImGui::Spacing ();

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.medium);
  ImGui::TextUnformatted ("Iterations");
  ImGui::PopFont ();

  auto &db = runtime_ctx->signal_db;
  const entt::id_type sys_tid = system->get_type_id ();

  bool any = false;

  for (const auto &it : db.system_iterations) {
    if (it.system_type_id != sys_tid) {
      continue;
}

    any = true;

    if (ImGui::TreeNode (it.iteration_name.c_str ())) {
      if (it.component_types.empty ()) {
        ImGui::TextDisabled ("No component type list.");
      } else {
        ImGui::TextUnformatted ("Components:");
        ImGui::Indent ();
        for (const auto &ct : it.component_types) {
          ImGui::BulletText ("%s", ct.type_name.c_str ());
        }
        ImGui::Unindent ();
      }

      ImGui::TreePop ();
    }
  }

  if (!any) {
    ImGui::TextDisabled ("No iterations registered for this system.");
  }
}

void
inspector::draw_singleton_inspector (entt::id_type type)
{
  entt::registry &registry
      = get_singleton_inspector_registry (*runtime_ctx, *m_editor_ctx,
                                          m_no_scene_registry);
  const bool has_scene = runtime_ctx->scene_manager.get_active () != nullptr;

  const wsl::reg::singleton_registry::descriptor *desc
      = runtime_ctx->singleton_registry.find (type);
  entt::meta_type const meta = entt::resolve (type);

  if (desc == nullptr) {
    ImGui::TextDisabled ("Unregistered singleton");
    return;
  }

  bool remove_requested = false;
  draw_singleton_header (*desc, meta, !desc->core, &remove_requested);

  if (remove_requested && (desc->remove != nullptr)) {
    if (desc->remove (registry)) {
      m_selection.clear_entity_singleton ();
    }
    return;
  }

  void *instance_ptr = (desc->get_ptr != nullptr) ? desc->get_ptr (registry) : nullptr;
  if (instance_ptr == nullptr) {
    ImGui::TextDisabled (has_scene ? "Singleton not present in scene"
                                   : "Singleton unavailable without an active "
                                     "scene");
    return;
  }

  bool has_visible_members = false;
  for (auto &&[id, data] : meta.data ()) {
    (void)id;
    if (!is_hidden (data)) {
      has_visible_members = true;
      break;
    }
  }

  const bool has_custom_inspector
      = meta && static_cast<bool> (
                       meta.func (entt::hashed_string{ "custom_inspect" }));
  if (!has_visible_members && !has_custom_inspector) {
    ImGui::TextDisabled ("No reflected fields.");
    return;
  }

  entt::meta_any obj = meta.from_void (instance_ptr);
  draw_meta_class (obj);
}

void
inspector::draw_entity_inspector (entt::entity entity)
{
  auto &scene_mgr = runtime_ctx->scene_manager;
  auto &scene = *scene_mgr.get_active ();
  entt::registry &reg = scene.get_registry ();

  // Prefab instance logic
  m_is_prefab_instance = reg.all_of<wsl::comp::prefab_instance> (entity);
  m_prefab_registry = nullptr;
  m_prefab_entity = entt::null;

  if (m_is_prefab_instance) {
    auto &pi = reg.get<wsl::comp::prefab_instance> (entity);
    if (auto prefab_scene = runtime_ctx->resource_manager.get (pi.prefab_id)) {
      m_prefab_registry = &prefab_scene->get_registry ();
      m_prefab_entity = pi.prefab_entity;
    }
  }

  const std::string &name = scene.get_entity_name (entity);

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.semibold);
  ImGui::TextUnformatted (name.c_str ());
  ImGui::PopFont ();

  ImGui::SameLine ();

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.light);
  ImGui::TextDisabled (" (%u)", static_cast<uint32_t> (entity));
  ImGui::PopFont ();

  if (m_is_prefab_instance && (m_prefab_registry != nullptr)) {
    ImGui::SameLine ();
    ImGui::TextDisabled ("[Prefab Instance]");
  }

  entt::id_type component_to_remove = entt::null;

  for (auto &&[internal_id, storage] : reg.storage ()) {

    if (!storage.contains (entity)) {
      continue;
}

    entt::id_type const type_id = runtime_ctx->component_registry.to_stable_id (internal_id);
    const auto *descriptor = runtime_ctx->component_registry.find (type_id);
    std::string const display_name = (descriptor != nullptr) ? descriptor->display_name : "Unknown";

    // Skip internal/special components
    if (type_id == entt::type_hash<wsl::comp::prefab_instance>::value ()) {
      continue;
}

    entt::meta_type const meta = entt::resolve (type_id);
    if (!meta) {
      // Fallback: draw a basic header even if we can't reflect the fields
      ImGui::Separator ();
      ImGui::PushFont (m_editor_ctx->get_imgui_renderer ()->fonts.medium);
      ImGui::TextUnformatted (display_name.c_str ());
      ImGui::PopFont ();
      ImGui::TextDisabled ("(No reflection data available)");
      continue;
    }
    if (!meta.is_class ()) {
      continue;
    }

    ImGui::PushID (static_cast<int> (type_id));

    ImGuiTreeNodeFlags const flags = ImGuiTreeNodeFlags_DefaultOpen
                               | ImGuiTreeNodeFlags_AllowOverlap
                               | ImGuiTreeNodeFlags_Framed
                               | ImGuiTreeNodeFlags_SpanAvailWidth;

    bool const expanded = ImGui::TreeNodeEx ((void *)(intptr_t)type_id, flags, " ");
    ImGui::SameLine ();

    // Draw Icon if any
    std::string const icon = get_icon_path (meta);
    if (!icon.empty ()) {
      auto *editor_res = &m_editor_ctx->editor_resources;
      auto img_id = editor_res->register_image (icon);

      if (editor_res->state (img_id) == wsl::rsc::image_state::not_loaded) {
        editor_res->load (img_id);
      }

      auto handle = editor_res->get (img_id);
      if (handle
          && editor_res->state (img_id) == wsl::rsc::image_state::loaded) {
        wsl::gfx::image  const*img = handle.handle ().get ();
        if ((img != nullptr) && (img->texture != nullptr)) {
          ImGui::Image ((ImTextureID)img->texture, ImVec2 (16, 16));
          ImGui::SameLine (0.0F, 6.0F);
        }
      }
    }

    ImGui::TextUnformatted (display_name.c_str ());

    const std::string comp_desc = get_description (meta);
    if (!comp_desc.empty () && ImGui::IsItemHovered ()) {
      ImGui::SetTooltip ("%s", comp_desc.c_str ());
    }

    ImGui::SameLine ();
    ImGui::SetCursorPosX (ImGui::GetWindowContentRegionMax ().x
                          - ImGui::CalcTextSize ("x").x
                          - (ImGui::GetStyle ().FramePadding.x * 2) - 10);

    if (ImGui::SmallButton ("x")) {
      component_to_remove = type_id;
    }
    if (ImGui::IsItemHovered ()) {
      ImGui::SetTooltip ("Remove component");
    }

    if (expanded) {
      if (component_to_remove != type_id) {
        void *ptr = storage.value (entity);
        if (ptr != nullptr) {
          ImGui::PushID ((int)entt::to_integral (entity));

          glm::vec3 entity_scale{ 1.0F, 1.0F, 1.0F };
          if (auto *t = reg.try_get<wsl::comp::transform> (entity); t) {
            entity_scale = glm::vec3 (t->scale.x, t->scale.y, t->scale.z);
          }

          if (type_id == entt::type_hash<wsl::comp::hierarchy>::value ()) {
            draw_hierarchy_component (entity);
          } else {
            entt::meta_any instance = meta.from_void (ptr);

            // If it's a prefab instance, try to find the same component in the
            // prefab
            entt::meta_any prefab_instance;
            if ((m_prefab_registry != nullptr) && m_prefab_registry->valid (m_prefab_entity)
                && (m_prefab_registry->storage (type_id) != nullptr)
                && m_prefab_registry->storage (type_id)->contains (
                    m_prefab_entity)) {
              void *p_ptr = m_prefab_registry->storage (type_id)->value (
                  m_prefab_entity);
              if (p_ptr != nullptr) {
                prefab_instance = meta.from_void (p_ptr);
              }
            }

            draw_meta_class (instance, entity_scale,
                             prefab_instance ? &prefab_instance : nullptr);
          }

          ImGui::PopID ();
        }
      }
      ImGui::TreePop ();
    }

    ImGui::PopID ();
  }

  if (component_to_remove != entt::null) {
    if (auto *storage = reg.storage (component_to_remove)) {
      storage->remove (entity);
    }
  }

  ImGui::Separator ();
  draw_add_component_ui (entity);
}

void
inspector::draw_hierarchy_component (entt::entity entity)
{

  entt::registry &registry
      = runtime_ctx->scene_manager.get_active ()->get_registry ();

  ImGui::PushID ((int)entt::to_integral (entity));

  wsl::comp::hierarchy  const&h = registry.get<wsl::comp::hierarchy> (entity);

  wsl::rsc::scene  const*scene = runtime_ctx->scene_manager.get_active ();

  const char *preview = "None";
  if (h.parent != entt::null) {
    preview = scene->get_entity_name (h.parent).c_str ();
  }

  if (ImGui::BeginCombo ("Parent", preview)) {

    if (ImGui::Selectable ("None", h.parent == entt::null)) {
      set_parent (entity, entt::null);
    }

    for (auto e : registry.view<entt::entity> ()) {
      if (e == entity) {
        continue;
}

      const std::string &name = scene->get_entity_name (e);
      bool const selected = (e == h.parent);

      if (ImGui::Selectable (name.c_str (), selected)) {
        set_parent (entity, e);
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
}
    }

    ImGui::EndCombo ();
  }

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.light);
  ImGui::TextDisabled ("First child: %u", (uint32_t)h.first);
  ImGui::TextDisabled ("Next sibling: %u", (uint32_t)h.next);
  ImGui::PopFont ();

  ImGui::PopID ();
}

void
inspector::set_parent (entt::entity child,
                                       entt::entity new_parent)
{

  entt::registry &registry
      = runtime_ctx->scene_manager.get_active ()->get_registry ();

  wsl::comp::hierarchy &ch = registry.get<wsl::comp::hierarchy> (child);

  // detach from old parent
  if (ch.parent != entt::null) {
    wsl::comp::hierarchy &old = registry.get<wsl::comp::hierarchy> (ch.parent);

    entt::entity *link = &old.first;
    while (*link != entt::null) {
      if (*link == child) {
        *link = registry.get<wsl::comp::hierarchy> (*link).next;
        break;
      }
      link = &registry.get<wsl::comp::hierarchy> (*link).next;
    }
  }

  ch.parent = new_parent;
  ch.next = entt::null;

  if (new_parent != entt::null) {
    wsl::comp::hierarchy &ph = registry.get<wsl::comp::hierarchy> (new_parent);
    ch.next = ph.first;
    ph.first = child;
  }
}

bool
inspector::draw_meta_class (entt::meta_any &object,
                                            const glm::vec3 &scale,
                                            entt::meta_any *prefab_object)
{
  auto type = object.type ();

  // If the class provides a full custom inspector, let it run.
  if (auto fn = type.func (entt::hashed_string{ "custom_inspect" }); fn) {
    if (entt::meta_any ret = fn.invoke (object, "##component", runtime_ctx);
        ret) {
      (void)ret.allow_cast<bool> ();
      const bool changed = ret.cast<bool> ();

      // If custom inspector changed something, also notify.
      if (changed) {
        if (auto on_changed
            = type.func (entt::hashed_string{ "on_inspector_changed" });
            on_changed) {
          on_changed.invoke (object, runtime_ctx, scale);
        }
      }
      return changed;
    }

    if (entt::meta_any ret = fn.invoke (object, "##component"); ret) {
      (void)ret.allow_cast<bool> ();
      const bool changed = ret.cast<bool> ();

      if (changed) {
        if (auto on_changed
            = type.func (entt::hashed_string{ "on_inspector_changed" });
            on_changed) {
          on_changed.invoke (object, runtime_ctx, scale);
        }
      }
      return changed;
    }

    return false;
  }

  // Default per-field inspector
  bool changed = false;

  for (auto &&[id, data] : type.data ()) {
    if (is_hidden (data)) {
      continue;
}

    entt::meta_any value = data.get (object);
    if (!value) {
      continue;
}

    entt::meta_any *prefab_value = nullptr;
    entt::meta_any prefab_value_storage;
    if (prefab_object != nullptr) {
      prefab_value_storage = data.get (*prefab_object);
      if (prefab_value_storage) {
        prefab_value = &prefab_value_storage;
      }
    }

    ImGui::PushID (static_cast<int> (id));

    const std::string field_name = wsl::comp::meta_display_name (data, "<unnamed>");
    ImGui::TextUnformatted (field_name.c_str ());

    if (ImGui::IsItemHovered ()) {
      ImGui::BeginTooltip ();
      ImGui::Text ("Type: %s", meta_type_debug_name (data.type ()).c_str ());
      const std::string desc = get_description (data);
      if (!desc.empty ()) {
        ImGui::Separator ();
        ImGui::TextWrapped ("%s", desc.c_str ());
      }
      ImGui::EndTooltip ();
    }

    ImGui::SameLine ();

    if (draw_meta_object ("##value", value, prefab_value)) {
      data.set (object, value);
      changed = true;
    }

    ImGui::PopID ();
  }

  // NEW: after any edit, notify the object (e.g. rigid_body)
  if (changed) {
    if (auto on_changed
        = type.func (entt::hashed_string{ "on_inspector_changed" });
        on_changed) {
      on_changed.invoke (object, runtime_ctx, scale);
    }
  }

  return changed;
}

static bool
meta_any_equal (const entt::meta_any &a, const entt::meta_any &b)
{
  if (!a || !b) {
    return false;
}
  if (a.type () != b.type ()) {
    return false;
}

  auto type = a.type ();

  // Try using meta equality operator if registered
  if (auto eq = type.func (entt::hashed_string{ "operator==" }); eq) {
    if (auto res = eq.invoke (a, b); res) {
      return res.cast<bool> ();
    }
  }

  // Fallback for common primitive types
  if (type == entt::resolve<float> ()) {
    return a.cast<float> () == b.cast<float> ();
  }
  if (type == entt::resolve<int> ()) {
    return a.cast<int> () == b.cast<int> ();
  }
  if (type == entt::resolve<bool> ()) {
    return a.cast<bool> () == b.cast<bool> ();
  }
  if (type == entt::resolve<std::string> ()) {
    return a.cast<std::string> () == b.cast<std::string> ();
  }
  if (type == entt::resolve<entt::entity> ()) {
    return a.cast<entt::entity> () == b.cast<entt::entity> ();
  }
  if (type == entt::resolve<uint32_t> ()) {
    return a.cast<uint32_t> () == b.cast<uint32_t> ();
  }

  return false;
}

bool
inspector::draw_meta_object (const char *label,
                                             entt::meta_any &object,
                                             entt::meta_any *prefab_object)
{

  auto type = object.type ();

  if (auto fn = type.func (entt::hashed_string{ "custom_inspect" }); fn) {
    // Try (label, runtime_ctx) first:
    if (entt::meta_any ret = fn.invoke (object, label, runtime_ctx); ret) {
      (void)ret.allow_cast<bool> ();
      return ret.cast<bool> ();
    }

    // Fallback: allow old signature (label) like wsl::math::vec3f
    if (entt::meta_any ret = fn.invoke (object, label); ret) {
      (void)ret.allow_cast<bool> ();
      return ret.cast<bool> ();
    }

    return false;
  }

  // ---- existing logic continues below ----
  if (type == entt::resolve<entt::entity> ()) {
    return draw_meta_value (label, object, prefab_object);
  }

  if (type == entt::resolve<std::string> ()) {
    return draw_meta_value (label, object, prefab_object);
  }

  if (type == entt::resolve<wsl::rsc::model_id> ()) {
    return draw_meta_value (label, object, prefab_object);
  }

  if (type == entt::resolve<wsl::rsc::cubemap_id> ()) {
    return draw_meta_value (label, object, prefab_object);
  }

  if (type == entt::resolve<wsl::rsc::audio_id> ()) {
    return draw_meta_value (label, object, prefab_object);
  }

  if (type.is_enum ()) { {
    return draw_meta_enum (label, object);
  } } if (type.is_sequence_container ())
    return draw_meta_sequence (label, object);
  else if (type.is_class ()) {
    if (ImGui::TreeNode (label)) {
      bool changed = draw_meta_class (object, { 1.0f, 1.0f, 1.0f }, prefab_object);
      ImGui::TreePop ();
      return changed;
    }
  } else {
    return draw_meta_value (label, object, prefab_object);
  }
  return false;
}

static const char *
enum_label (const entt::meta_data& data)
{
  // meta_custom stores a value of type const char*,
  // so the safe pointer conversion yields const char**.
  if (const char * const*p = data.custom (); (p != nullptr) && ((*p) != nullptr)) {
    return *p;
  }
  return "<unnamed>";
}

bool
inspector::draw_meta_enum (const char *label,
                                           entt::meta_any &object)
{
  auto type = object.type ();

  (void)object.allow_cast<int> ();
  const int current = object.cast<int> ();

  const char *preview = "...";

  for (auto &&[id, data] : type.data ()) {
    entt::meta_any v = data.get ({});
    if (!v) {
      continue;
}

    (void)v.allow_cast<int> ();
    if (v.cast<int> () == current) {
      preview = enum_label (data);
      break;
    }
  }

  bool changed = false;

  if (ImGui::BeginCombo (label, preview)) {
    for (auto &&[id, data] : type.data ()) {
      entt::meta_any v = data.get ({});
      if (!v) {
        continue;
}

      (void)v.allow_cast<int> ();
      const int value = v.cast<int> ();
      const bool selected = (value == current);

      if (ImGui::Selectable (enum_label (data), selected)) {
        object = value;
        changed = true;
      }

      if (selected) {
        ImGui::SetItemDefaultFocus ();
}
    }
    ImGui::EndCombo ();
  }

  return changed;
}

bool
inspector::draw_meta_sequence (const char *label,
                                               entt::meta_any &object)
{
  auto view = object.as_sequence_container ();
  if (!view) {
    return false;
}

  bool changed = false;

  if (ImGui::TreeNode (label)) {
    std::size_t i = 0;
    for (auto it = view.begin (); it != view.end (); ++it, ++i) {
      entt::meta_any element = *it;
      ImGui::PushID (static_cast<int> (i));
      if (draw_meta_object ("element", element)) {
        *it = element;
        changed = true;
      }
      ImGui::PopID ();
    }
    ImGui::TreePop ();
  }

  return changed;
}

bool
inspector::draw_meta_value (const char *label,
                                            entt::meta_any &object,
                                            entt::meta_any *prefab_object)
{
  auto type = object.type ();

  entt::registry &registry
      = runtime_ctx->scene_manager.get_active ()->get_registry ();

  bool changed = false;

  auto draw_restore_button = [&] () {
    if (prefab_object && !meta_any_equal (object, *prefab_object)) {
      ImGui::SameLine ();
      if (ImGui::SmallButton ("Restore")) {
        object = *prefab_object;
        changed = true;
      }
      if (ImGui::IsItemHovered ()) {
        ImGui::SetTooltip ("Restore to prefab default");
      }
    }
  };

  if (type == entt::resolve<entt::entity> ()) {
    auto &scene = *runtime_ctx->scene_manager.get_active ();

    entt::entity const current = object.cast<entt::entity> ();

    const char *preview = "None";
    if (current != entt::null && registry.valid (current)) {
      preview = scene.get_entity_name (current).c_str ();
    }

    if (ImGui::BeginCombo (label, preview)) {

      if (ImGui::Selectable ("None", current == entt::null)) {
        object = entt::null;
        changed = true;
      }

      for (auto e : registry.view<entt::entity> ()) {

        const std::string &name = scene.get_entity_name (e);
        bool const selected = (e == current);

        if (ImGui::Selectable (name.c_str (), selected)) {
          object = e;
          changed = true;
        }

        if (selected) {
          ImGui::SetItemDefaultFocus ();
}
      }

      ImGui::EndCombo ();
    }

    draw_restore_button ();
    return changed;
  } if (type == entt::resolve<wsl::rsc::cubemap_id> ()) {
    wsl::rsc::resource_manager *res_mgr = &runtime_ctx->resource_manager;

    auto &current = object.cast<wsl::rsc::cubemap_id &> ();

    const char *preview = "None";
    static char buf[64];

    if (current.value != entt::null) {
      std::snprintf (buf, sizeof (buf), "Cubemap %08X",
                     (uint32_t)current.value);
      preview = buf;
    }

    if (ImGui::BeginCombo (label, preview)) {

      if (ImGui::Selectable ("None", current.value == entt::null)) {
        current.value = entt::null;
        changed = true;
      }

      for (const auto &rec : res_mgr->list_cubemaps ()) {
        if (rec.state != wsl::rsc::cubemap_state::loaded)
          continue;

        bool selected = (rec.id == current.value);

        char name[64];
        std::snprintf (name, sizeof (name), "Cubemap %08X", (uint32_t)rec.id);

        if (ImGui::Selectable (name, selected)) {
          current.value = rec.id;
          changed = true;
        }

        if (selected)
          ImGui::SetItemDefaultFocus ();
      }

      ImGui::EndCombo ();
    }

    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<wsl::rsc::model_id> ()) {
    auto &current = object.cast<wsl::rsc::model_id &> ();
    if (current.custom_inspect (label, runtime_ctx)) {
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<wsl::rsc::audio_id> ()) {
    auto &current = object.cast<wsl::rsc::audio_id &> ();
    if (current.custom_inspect (label, runtime_ctx)) {
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<std::string> ()) {
    std::string &str = object.cast<std::string &> ();

    static char buffer[256];
    std::snprintf (buffer, sizeof (buffer), "%s", str.c_str ());

    if (ImGui::InputText (label, buffer, sizeof (buffer))) {
      object = std::string (buffer);
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<bool> ()) {
    bool v = object.cast<bool> ();
    if (ImGui::Checkbox (label, &v)) {
      object = v;
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<int> ()) {
    int v = object.cast<int> ();
    if (ImGui::DragInt (label, &v)) {
      object = v;
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<uint32_t> ()) {
    uint32_t v = object.cast<uint32_t> ();

    // ImGui has no native DragUInt, so use int but clamp
    int tmp = static_cast<int> (v);

    if (ImGui::DragInt (label, &tmp, 1.0f, 0, INT32_MAX)) {
      object = static_cast<uint32_t> (tmp);
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<float> ()) {
    float v = object.cast<float> ();
    if (ImGui::DragFloat (label, &v, 0.1f)) {
      object = v;
      changed = true;
    }
    draw_restore_button ();
    return changed;
  } else if (type == entt::resolve<glm::mat4> ()) {
    const glm::mat4 &m = object.cast<const glm::mat4 &> ();

    ImGui::TextUnformatted (label);
    ImGui::PushID (label);

    ImGui::BeginGroup ();
    for (int i = 0; i < 4; ++i) {
      ImGui::PushID (i);
      for (int j = 0; j < 4; ++j) {
        ImGui::PushID (j);
        float v = m[i][j];
        ImGui::SetNextItemWidth (ImGui::CalcItemWidth () / 4.1f);
        ImGui::InputFloat ("##v", &v, 0.0f, 0.0f, "%.3f",
                           ImGuiInputTextFlags_ReadOnly);
        if (j < 3)
          ImGui::SameLine ();
        ImGui::PopID ();
      }
      ImGui::PopID ();
    }
    ImGui::EndGroup ();

    ImGui::PopID ();
    return false; // Read-only, so never changed
  }

  return false;
}

bool
inspector::is_hidden (const entt::meta_data& data) 
{
  return (data.traits<editor_trait> () & editor_trait::hidden)
         == editor_trait::hidden;
}

void
inspector::draw_add_component_ui (entt::entity entity)
{
  static entt::id_type selected_type = entt::null;

  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.medium);
  ImGui::TextUnformatted ("Add Component");
  ImGui::PopFont ();

  auto &reg = runtime_ctx->scene_manager.get_active ()->get_registry ();

  auto label_for = [] (const entt::meta_type& meta) -> std::string {
    return wsl::comp::meta_display_name (meta, "<unregistered>");
  };

  const bool has_valid_selection
      = selected_type != entt::null
        && selected_type != entt::type_hash<void>::value ();

  const char *preview = "Select component";
  std::string preview_storage;
  if (has_valid_selection) {
    preview_storage = label_for (entt::resolve (selected_type));
    preview = preview_storage.c_str ();
  }

  if (ImGui::BeginCombo ("##add_component", preview)) {
    for (const auto *desc : runtime_ctx->component_registry.ordered ()) {
      if ((desc == nullptr) || !desc->can_add_default) {
        continue;
      }

      entt::meta_type const meta = entt::resolve (desc->type_id);
      if (!meta || !meta.is_class ()) {
        continue;
      }

      if ((desc->contains != nullptr) && desc->contains (reg, entity)) {
        continue;
      }

      const bool selected = (selected_type == desc->type_id);
      const std::string name = label_for (meta);
      if (ImGui::Selectable (name.c_str (), selected)) {
        selected_type = desc->type_id;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus ();
      }
    }

    ImGui::EndCombo ();
  }

  if (selected_type != entt::null) {
    if (ImGui::Button ("Add")) {
      if (const auto *desc
          = runtime_ctx->component_registry.find (selected_type);
          (desc != nullptr) && (desc->emplace_default != nullptr)) {
        desc->emplace_default (reg, entity);
      }

      selected_type = entt::null;
    }
  }
}

void
inspector::draw_singleton_header (
    const wsl::reg::singleton_registry::descriptor &desc, const entt::meta_type& meta,
    bool can_remove, bool *remove_requested)
{
  ImGui::PushFont (m_editor_ctx->get_imgui_renderer()->fonts.medium);
  ImGui::TextUnformatted (desc.display_name.c_str ());
  ImGui::PopFont ();

  (void)meta;
  ImGui::TextDisabled ("%s", desc.type_name.c_str ());

  if (!can_remove) {
    ImGui::BeginDisabled ();
  }

  ImGui::SameLine ();
  if (ImGui::SmallButton ("Remove") && (remove_requested != nullptr)) {
    *remove_requested = true;
  }

  if (ImGui::IsItemHovered () && !can_remove) {
    ImGui::SetTooltip ("Core singleton components cannot be removed.");
  }

  if (!can_remove) {
    ImGui::EndDisabled ();
  }

  ImGui::Separator ();
}

#undef runtime_ctx

} // namespace editor
