#include "game_view.hpp"

#include "editor/ecs_inspector_utils.hpp"
#include "gfx/mesh.hpp"
#include "gfx/render_window.hpp"
#include "rsc/resource_ids.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/model_instance_3d.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/world_transform.hpp"

#include <ImGuizmo.h>
#include "imviewguizmo.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>
#include <imgui.h>
#include <limits>
#include <unordered_set>
#include <utility>

namespace editor
{

struct pick_ray
{
  glm::vec3 origin;
  glm::vec3 dir;
};

static void
draw_hsplitter (const char *id, float &top_height, float thickness = 6.0F)
{
  ImGui::InvisibleButton (id, ImVec2 (-1, thickness));

  // Nice cursor while hovering
  if (ImGui::IsItemHovered ()) {
    ImGui::SetMouseCursor (ImGuiMouseCursor_ResizeNS);
  }

  if (ImGui::IsItemActive ()) {
    top_height += ImGui::GetIO ().MouseDelta.y;
  }

  // Draw the splitter bar
  ImU32 col = ImGui::GetColorU32 (ImGuiCol_Separator);
  if (ImGui::IsItemHovered () || ImGui::IsItemActive ()) {
    col = ImGui::GetColorU32 (ImGuiCol_SeparatorHovered);
  }

  ImVec2 const min = ImGui::GetItemRectMin ();
  ImVec2 const max = ImGui::GetItemRectMax ();
  ImGui::GetWindowDrawList ()->AddRectFilled (min, max, col);
}

static pick_ray
make_mouse_ray (const ImVec2 &mouse_pos, const ImVec2 &img_min,
                const ImVec2 &img_size,
                const wsl::comp::singl::editor_context::resolved_camera &rc)
{
  // Mouse inside image -> NDC
  float const x = (mouse_pos.x - img_min.x) / img_size.x;
  float const y = (mouse_pos.y - img_min.y) / img_size.y;

  float const ndc_x = (x * 2.0F) - 1.0F;
  float const ndc_y = 1.0F - (y * 2.0F);

  glm::mat4 const inv_vp = glm::inverse (rc.vp);

  // Use far plane point; works fine for ray direction.
  glm::vec4 const far_clip (ndc_x, ndc_y, 1.0F, 1.0F);
  glm::vec4 far_world4 = inv_vp * far_clip;
  far_world4 /= far_world4.w;

  pick_ray ray;
  ray.origin = rc.world_pos;
  ray.dir = glm::normalize (glm::vec3 (far_world4) - ray.origin);
  return ray;
}

static bool
ray_aabb_intersect (const glm::vec3 &ray_origin, const glm::vec3 &ray_dir,
                    const glm::vec3 &bmin, const glm::vec3 &bmax, float &t_hit)
{
  float tmin = 0.0F;
  float tmax = std::numeric_limits<float>::max ();

  for (int i = 0; i < 3; ++i) {
    float const ro = ray_origin[i];
    float const rd = ray_dir[i];
    float const mn = bmin[i];
    float const mx = bmax[i];

    if (std::abs (rd) < 1e-8F) {
      if (ro < mn || ro > mx) {
        return false;
      }
      continue;
    }

    float const inv = 1.0F / rd;
    float t1 = (mn - ro) * inv;
    float t2 = (mx - ro) * inv;

    if (t1 > t2) {
      std::swap (t1, t2);
    }

    tmin = std::max (tmin, t1);
    tmax = std::min (tmax, t2);

    if (tmin > tmax) {
      return false;
    }
  }

  t_hit = tmin;
  return true;
}

static void
transform_aabb (const glm::vec3 &local_min, const glm::vec3 &local_max,
                const glm::mat4 &m, glm::vec3 &out_min, glm::vec3 &out_max)
{
  glm::vec3 const corners[8] = {
    { local_min.x, local_min.y, local_min.z },
    { local_max.x, local_min.y, local_min.z },
    { local_min.x, local_max.y, local_min.z },
    { local_max.x, local_max.y, local_min.z },
    { local_min.x, local_min.y, local_max.z },
    { local_max.x, local_min.y, local_max.z },
    { local_min.x, local_max.y, local_max.z },
    { local_max.x, local_max.y, local_max.z },
  };

  out_min = glm::vec3 (std::numeric_limits<float>::max ());
  out_max = glm::vec3 (-std::numeric_limits<float>::max ());

  for (const glm::vec3 &c : corners) {
    glm::vec3 const wc = glm::vec3 (m * glm::vec4 (c, 1.0F));
    out_min = glm::min (out_min, wc);
    out_max = glm::max (out_max, wc);
  }
}

static entt::entity
pick_entity_from_game_view (
    entt::registry &registry, wsl::comp::singl::runtime_context *runtime_ctx,
    const wsl::comp::singl::editor_context::resolved_camera &rc,
    const ImVec2 &img_min, const ImVec2 &img_size, const ImVec2 &mouse_pos)
{

  if (runtime_ctx == nullptr) {
    return entt::null;
  }

  auto view
      = registry
            .view<wsl::comp::world_transform, wsl::comp::model_instance_3d> ();

  pick_ray const ray = make_mouse_ray (mouse_pos, img_min, img_size, rc);

  entt::entity best = entt::null;
  float best_t = std::numeric_limits<float>::max ();

  for (entt::entity const entity : view) {
    wsl::comp::world_transform const &world
        = view.get<wsl::comp::world_transform> (entity);
    wsl::comp::model_instance_3d const &instance
        = view.get<wsl::comp::model_instance_3d> (entity);

    auto model = runtime_ctx->resource_manager.get (instance.id);
    if (!model) {
      continue;
    }

    // You need this helper on model_3d (recommended).
    glm::vec3 local_min;
    glm::vec3 local_max;
    if (!model->get_scene_bounds (instance.scene_index, local_min, local_max)) {
      continue;
    }

    glm::vec3 world_min;
    glm::vec3 world_max;
    transform_aabb (local_min, local_max, world.value, world_min, world_max);

    float t_hit = 0.0F;
    if (ray_aabb_intersect (ray.origin, ray.dir, world_min, world_max, t_hit)) {
      if (t_hit < best_t) {
        best_t = t_hit;
        best = entity;
      }
    }
  }

  return best;
}

static glm::vec3
extract_translation (const glm::mat4 &m)
{
  return glm::vec3 (m[3]);
}

static glm::quat
look_at_rotation (const glm::vec3 &eye, const glm::vec3 &target,
                  const glm::vec3 &up = glm::vec3 (0.0F, 1.0F, 0.0F))
{
  glm::vec3 const f = glm::normalize (target - eye);

  if (glm::length2 (f) < 1e-8F) {
    return glm::quat (1.0F, 0.0F, 0.0F, 0.0F);
  }

  return glm::quatLookAtRH (f, up);
}

static void
extract_trs (const glm::mat4 &m, glm::vec3 &pos, glm::quat &rot,
             glm::vec3 &scale)
{
  pos = glm::vec3 (m[3]);

  glm::vec3 x = glm::vec3 (m[0]);
  glm::vec3 y = glm::vec3 (m[1]);
  glm::vec3 z = glm::vec3 (m[2]);

  scale.x = glm::length (x);
  scale.y = glm::length (y);
  scale.z = glm::length (z);

  if (scale.x != 0.0F) {
    x /= scale.x;
  }
  if (scale.y != 0.0F) {
    y /= scale.y;
  }
  if (scale.z != 0.0F) {
    z /= scale.z;
  }

  glm::mat3 const rot_m (x, y, z);
  rot = glm::quat_cast (rot_m);
}

void
game_view::set_render_texture (const wsl::gfx::texture &texture)
{
  m_render_texture = texture.texture_data;
  m_tex_width = (int)texture.width;
  m_tex_height = (int)texture.height;
}

void
game_view::draw (entt::registry &registry, wsl::gfx::render_window &rw)
{
  if (!visible) {
    return;
  }

  if (!ImGui::Begin ("Game View", &visible,
                     ImGuiWindowFlags_NoScrollbar
                         | ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End ();
    return;
  }

  if (m_toolbar_height < 0) {
    m_toolbar_height = ImGui::GetFrameHeightWithSpacing () + 4.0F;
  }

  ImGui::BeginChild ("##toolbar_area", ImVec2 (0, m_toolbar_height), 0,
                     ImGuiWindowFlags_NoScrollbar
                         | ImGuiWindowFlags_NoScrollWithMouse);
  draw_camera_header (registry, rw);
  ImGui::EndChild ();

  draw_hsplitter ("##toolbar_splitter", m_toolbar_height);

  const float spacing = ImGui::GetStyle ().ItemSpacing.x;
  const float avail_w = ImGui::GetContentRegionAvail ().x;
  const float max_height = (avail_w - 24.0F - (10.0F * spacing)) / 8.0F;

  m_toolbar_height
      = std::clamp (m_toolbar_height, 20.0F, std::max (20.0F, max_height));

  if ((m_editor_ctx != nullptr) && (m_runtime_ctx != nullptr)
      && !m_runtime_ctx->is_running) {
    m_editor_ctx->tick_editor_camera_anim (ImGui::GetIO ().DeltaTime);
  }

  if (m_render_texture == nullptr) {
    ImGui::TextDisabled ("No render target");
    ImGui::End ();
    return;
  }

  // --- Fit render texture to available space (keep aspect) ---
  ImVec2 const avail = ImGui::GetContentRegionAvail ();

  float const aspect
      = (m_tex_height == 0) ? 1.0F : (float)m_tex_width / (float)m_tex_height;
  ImVec2 size = avail;

  if (size.y <= 0.0F) {
    size.y = 1.0F;
  }

  if (size.x / size.y > aspect) {
    size.x = size.y * aspect;
  } else {
    size.y = size.x / aspect;
  }

  ImVec2 const cursor = ImGui::GetCursorPos ();
  ImVec2 const offset ((avail.x - size.x) * 0.5F, (avail.y - size.y) * 0.5F);

  // --- Image rect in screen space ---
  ImVec2 const win_pos = ImGui::GetWindowPos ();
  ImVec2 const img_min (win_pos.x + cursor.x + offset.x,
                        win_pos.y + cursor.y + offset.y);
  ImVec2 const img_size = size;

  if (m_editor_ctx != nullptr) {
    m_editor_ctx->last_img_min = glm::vec2 (img_min.x, img_min.y);
    m_editor_ctx->last_img_size = glm::vec2 (img_size.x, img_size.y);
  }

  ImGui::SetCursorPos (ImVec2 (cursor.x + offset.x, cursor.y + offset.y));
  ImGui::Image ((ImTextureID)m_render_texture, size, ImVec2 (0, 0),
                ImVec2 (1, 1));

  // ---------- View gizmo (ENGINE DEFAULT camera only, paused) ----------
  bool show_view_gizmo = false;
  if ((m_editor_ctx != nullptr) && (m_runtime_ctx != nullptr)
      && !m_runtime_ctx->is_running) {
    wsl::comp::singl::editor_context const &ed = *this->m_editor_ctx;
    show_view_gizmo = (ed.game_view_camera_mode
                       == wsl::comp::singl::editor_context::game_view_cam_mode::
                           engine_default);
  }

  if (show_view_gizmo) {
    ImViewGuizmo::SetContext (0x47414D45U); // 'GAME'
    ImViewGuizmo::BeginFrame ();

    auto &style = ImViewGuizmo::GetStyle ();
    style.scale = 0.30F;
    style.animateSnap = true;

    const float gizmo_diameter = 256.0F * style.scale;
    const float half = gizmo_diameter * 0.5F;
    const float pad = 8.0F;

    // ImViewGuizmo expects center pos
    ImVec2 const rotate_center (img_min.x + img_size.x - half - pad,
                                img_min.y + half + pad);

    ImVec2 const tool_anchor = rotate_center;

    glm::vec3 cam_pos = m_editor_ctx->editor_cam_pos;
    glm::quat cam_rot = m_editor_ctx->editor_cam_rot;

    // Orbit around selected entity if possible, otherwise keep current
    // pivot.
    glm::vec3 pivot (0.0F);

    if ((m_selection != nullptr) && m_selection->kind == selection_kind::entity
        && m_selection->selected_entity != entt::null
        && registry.all_of<wsl::comp::world_transform> (
            m_selection->selected_entity)) {
      const wsl::comp::world_transform &wt
          = registry.get<wsl::comp::world_transform> (
              m_selection->selected_entity);
      pivot = extract_translation (wt.value);
    }

    m_orbit_pivot = pivot;

    // Keep camera at a sane distance from the pivot; otherwise free-rotate
    // has no leverage.
    const float min_dist = 0.10F; // tweak (0.05..0.25)

    glm::vec3 v = cam_pos - pivot;
    float d2 = glm::dot (v, v);

    if (d2 < min_dist * min_dist) {
      if (d2 < 1e-8F) {
        // Push back using the camera forward direction if possible.
        glm::vec3 forward = cam_rot * glm::vec3 (0.0F, 0.0F, -1.0F);
        if (glm::length2 (forward) < 1e-8F) {
          forward = glm::vec3 (0.0F, 0.0F, -1.0F);
        }
        v = -glm::normalize (forward);
        d2 = 1.0F;
      }
      cam_pos = pivot + (v / std::sqrt (d2)) * min_dist;
    }

    bool modified = false;

    modified
        |= ImViewGuizmo::Rotate (cam_pos, cam_rot, pivot, rotate_center, 0.01F);

    ImVec2 btn_pos (tool_anchor.x + half
                        - ((style.toolButtonRadius * style.scale) * 2.0F),
                    tool_anchor.y + half + 8.0F);

    modified |= ImViewGuizmo::Dolly (cam_pos, cam_rot, btn_pos, 0.05F);

    btn_pos.y += ((style.toolButtonRadius * style.scale) * 2.0F) + 6.0F;
    modified |= ImViewGuizmo::Pan (cam_pos, cam_rot, btn_pos, 0.01F);

    if (modified || ImViewGuizmo::IsUsing ()) {
      m_editor_ctx->editor_cam_pos = cam_pos;
      m_editor_ctx->editor_cam_rot = cam_rot;
      m_editor_ctx->cancel_editor_camera_anim ();
    }
  }

  // ---------- Resolve the SAME camera used to render the Game View ----------
  auto *scene = ((m_runtime_ctx) != nullptr)
                    ? m_runtime_ctx->scene_manager.get_active ()
                    : nullptr;

  wsl::comp::singl::editor_context::resolved_camera rc{};
  bool have_cam = false;
  if ((scene != nullptr) && (m_editor_ctx != nullptr)) {
    have_cam = m_editor_ctx->resolve_game_view_camera (registry, scene, rc);
  }

  const bool img_hovered = ImGui::IsMouseHoveringRect (
      img_min, ImVec2 (img_min.x + img_size.x, img_min.y + img_size.y), false);

  bool block_picking = false;
  if (show_view_gizmo) {
    block_picking = (ImViewGuizmo::IsUsing () || ImViewGuizmo::IsOver ());
  }

  if (!block_picking) {
    block_picking = ImGuizmo::IsUsing () || ImGuizmo::IsOver ();
  }

  if (!m_runtime_ctx->is_running && have_cam && (m_selection != nullptr)
      && img_hovered && !block_picking
      && ImGui::IsMouseClicked (ImGuiMouseButton_Left)) {

    entt::entity const picked = pick_entity_from_game_view (
        registry, m_runtime_ctx, rc, img_min, img_size, ImGui::GetMousePos ());

    if (picked != entt::null) {
      m_selection->select_entity (picked);
      m_editor_ctx->selected_entity = picked;
    } else {
      m_selection->clear_entity_singleton ();
      m_editor_ctx->selected_entity = entt::null;
    }
  }

  if ((scene != nullptr) && (m_editor_ctx != nullptr)) {
    have_cam = m_editor_ctx->resolve_game_view_camera (registry, scene, rc);
  }

  // ---------- ImGuizmo (entity transform gizmo, paused) ----------
  bool block_entity_gizmo = false;
  if (show_view_gizmo) {
    // Only block when the view gizmo is actually shown (avoid stale state)
    block_entity_gizmo = (ImViewGuizmo::IsUsing () || ImViewGuizmo::IsOver ());
  }

  // Detect whether the active camera is a 2D camera.
  bool const is_2d_camera
      = (rc.entity != entt::null)
        && registry.all_of<wsl::comp::camera_2d> (rc.entity);

  if (!is_2d_camera && (m_runtime_ctx != nullptr) && !m_runtime_ctx->is_running
      && have_cam) {
    ImGuizmo::BeginFrame ();

    // IMPORTANT: set rect to the IMAGE rect, not the whole window
    ImGuizmo::SetDrawlist ();
    ImGuizmo::SetRect (img_min.x, img_min.y, img_size.x, img_size.y);

    glm::mat4 view = rc.view;
    glm::mat4 proj = rc.proj;

    if (m_show_grid) {
      glm::vec3 const forward = -glm::vec3 (view[0][2], view[1][2], view[2][2]);
      float const tilt = std::abs (forward.y);

      // Distance to the point the camera is looking at on the ground.
      float const ground_t
          = (tilt > 0.01F) ? (std::abs (rc.world_pos.y) / tilt) : 500.0F;

      /*!
       * The visibility radius scales with the distance to the look-at point.
       * Formula: d * sqrt(d) + 10, capped at 250 units.
       */
      float const base_fog_radius
          = std::min (250.0F, (ground_t * std::sqrt (ground_t)) + 10.0F);

      glm::vec3 const flat_fwd
          = (glm::length (glm::vec2 (forward.x, forward.z)) > 0.001F)
                ? glm::normalize (glm::vec3 (forward.x, 0.0F, forward.z))
                : glm::vec3 (0, 0, 0);

      // Shift fog center towards gaze to keep focus area solid.
      float const shift_dist = std::min (ground_t, base_fog_radius * 0.5F);
      glm::vec3 const fog_center = glm::vec3 (rc.world_pos.x, 0, rc.world_pos.z)
                                   + flat_fwd * shift_dist;

      // Update editor context with grid parameters for the 3D pass.
      m_editor_ctx->grid_visible = true;
      m_editor_ctx->grid_camera_pos = rc.world_pos;
      m_editor_ctx->grid_fog_center = fog_center;
      m_editor_ctx->grid_fog_radius = base_fog_radius;

      /*
      // Debug Grid Parameters (Top-Left overlay)
      ImGui::SetCursorScreenPos (ImVec2 (img_min.x + 10, img_min.y + 10));
      ImGui::BeginGroup ();
      ImGui::TextColored (ImVec4 (1, 1, 0, 1), "--- Grid Debug ---");
      ImGui::TextColored (ImVec4 (1, 1, 1, 1), "Tilt:       %.3f", tilt);
      ImGui::TextColored (ImVec4 (1, 1, 1, 1), "Gaze Dist:  %.1f", ground_t);
      ImGui::TextColored (ImVec4 (1, 1, 1, 1), "Fog Radius: %.1f",
      base_fog_radius); ImGui::TextColored (ImVec4 (1, 1, 1, 1), "Fog Shift:
      %.1f", shift_dist); ImGui::EndGroup ();
      */
    } else {
      m_editor_ctx->grid_visible = false;
    }
    if ((m_selection != nullptr) && m_selection->kind == selection_kind::entity
        && !block_entity_gizmo) {
      entt::entity const e = m_selection->selected_entity;

      if (registry.all_of<wsl::comp::transform, wsl::comp::world_transform> (
              e)) {
        wsl::comp::transform &tr = registry.get<wsl::comp::transform> (e);
        wsl::comp::world_transform const &wt
            = registry.get<wsl::comp::world_transform> (e);

        glm::mat4 world = wt.value;

        ImGuizmo::SetOrthographic (false);
        ImGuizmo::Manipulate (glm::value_ptr (view), glm::value_ptr (proj),
                              m_current_op, ImGuizmo::WORLD,
                              glm::value_ptr (world));

        if (ImGuizmo::IsUsing ()) {
          glm::mat4 local = world;

          if (auto *h = registry.try_get<wsl::comp::hierarchy> (e)) {
            if (h->parent != entt::null
                && registry.all_of<wsl::comp::world_transform> (h->parent)) {
              const wsl::comp::world_transform &pwt
                  = registry.get<wsl::comp::world_transform> (h->parent);
              local = glm::inverse (static_cast<glm::mat4> (pwt.value)) * world;
            }
          }

          glm::vec3 pos;
          glm::vec3 scl;
          glm::quat rot;
          extract_trs (local, pos, rot, scl);

          tr.position = wsl::math::vec3f{ pos };
          tr.rotation = wsl::math::quatf{ rot };
          tr.scale = wsl::math::vec3f{ scl };
        }
      }
    }

    if (ImGui::IsWindowFocused (ImGuiFocusedFlags_RootAndChildWindows)) {
      if (ImGui::IsKeyPressed (ImGuiKey_W)) {
        m_current_op = ImGuizmo::TRANSLATE;
      }
      if (ImGui::IsKeyPressed (ImGuiKey_E)) {
        m_current_op = ImGuizmo::ROTATE;
      }
      if (ImGui::IsKeyPressed (ImGuiKey_R)) {
        m_current_op = ImGuizmo::SCALE;
      }
    }
  } else if (is_2d_camera) {
    // When a 2D camera is active, hide the 3D ground grid.
    m_editor_ctx->grid_visible = false;
  }

  ImGui::End ();
}

static void
draw_icon_button (wsl::comp::singl::editor_context *editor_ctx,
                  wsl::rsc::image_id icon_id, const char *tooltip,
                  bool *clicked, bool enabled = true, bool active = false,
                  float size = -1.0F)
{
  if (size < 0) {
    size = ImGui::GetFrameHeight ();
  }

  if (!enabled) {
    ImGui::BeginDisabled ();
  }

  if (active) {
    ImGui::PushStyleColor (ImGuiCol_Button,
                           ImGui::GetStyleColorVec4 (ImGuiCol_ButtonHovered));
  }

  const float padding = 4.0F;
  const ImVec2 image_size (size - (padding * 2.0F), size - (padding * 2.0F));

  auto handle = editor_ctx->editor_resources.get (icon_id);
  if (!handle || ((*handle).texture == nullptr)) {
    editor_ctx->editor_resources.load (icon_id);
    if (ImGui::Button ("??", ImVec2 (size, size))) {
      if (clicked != nullptr) {
        *clicked = true;
      }
    }
  } else {
    ImGui::PushStyleVar (ImGuiStyleVar_FramePadding, ImVec2 (padding, padding));
    if (ImGui::ImageButton (tooltip, (ImTextureID)(*handle).texture,
                            image_size)) {
      if (clicked != nullptr) {
        *clicked = true;
      }
    }
    ImGui::PopStyleVar ();
  }

  if (active) {
    ImGui::PopStyleColor ();
  }

  if (ImGui::IsItemHovered (ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip ("%s", tooltip);
  }

  if (!enabled) {
    ImGui::EndDisabled ();
  }
}

void
game_view::draw_camera_header (entt::registry &registry,
                               wsl::gfx::render_window & /*unused*/)
{
  wsl::comp::singl::runtime_context &runtime_ctx = *this->m_runtime_ctx;

  // Center icons
  const float btn_size = m_toolbar_height;
  const float spacing = ImGui::GetStyle ().ItemSpacing.x;
  // 8 buttons (Play, Stop, T, R, S, Reset, Focus, Grid)
  // 3 dummies (8.0f each)
  // Spacings: between every element (10 total)
  float const group_w = (8.0F * btn_size) + 24.0F + (10.0F * spacing);

  float const avail = ImGui::GetContentRegionAvail ().x;
  float const offset = (avail - group_w) * 0.5F;
  if (offset > 0.0F) {
    ImGui::SetCursorPosX (ImGui::GetCursorPosX () + offset);
  }

  ImGui::BeginGroup ();

  if (runtime_ctx.in_play_session) {
    if (runtime_ctx.is_running) {
      bool do_pause = false;
      draw_icon_button (m_editor_ctx, m_editor_ctx->icon_pause, "Pause",
                        &do_pause, true, false, btn_size);
      if (do_pause) {
        runtime_ctx.set_running (false);
      }
    } else {
      bool do_play = false;
      draw_icon_button (m_editor_ctx, m_editor_ctx->icon_play, "Play", &do_play,
                        true, false, btn_size);
      if (do_play) {
        runtime_ctx.set_running (true);
        ImGui::SetWindowFocus ();
      }
    }
    ImGui::SameLine ();
    bool do_stop = false;
    draw_icon_button (m_editor_ctx, m_editor_ctx->icon_stop, "Stop", &do_stop,
                      true, false, btn_size);
    if (do_stop) {
      runtime_ctx.stop ();
    }
  } else {
    bool do_play = false;
    draw_icon_button (m_editor_ctx, m_editor_ctx->icon_play, "Play", &do_play,
                      true, false, btn_size);
    if (do_play) {
      runtime_ctx.set_running (true);
      ImGui::SetWindowFocus ();
    }
    ImGui::SameLine ();
    draw_icon_button (m_editor_ctx, m_editor_ctx->icon_stop, "Stop", nullptr,
                      false, false, btn_size);
  }

  // ===================== gizmo mode buttons =====================
  ImGui::SameLine ();
  ImGui::Dummy (ImVec2 (8.0F, 0.0F));
  ImGui::SameLine ();

  auto toggle_op = [this] (ImGuizmo::OPERATION op) {
    if (m_current_op == op) {
      m_current_op = (ImGuizmo::OPERATION)0; // hide
    } else {
      m_current_op = op;
    }
  };

  if (runtime_ctx.is_running) {
    ImGui::BeginDisabled ();
  }

  const bool tr_on = (m_current_op == ImGuizmo::TRANSLATE);
  const bool rot_on = (m_current_op == ImGuizmo::ROTATE);
  const bool scl_on = (m_current_op == ImGuizmo::SCALE);

  bool clicked_tr = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_translate, "Translate (W)",
                    &clicked_tr, !runtime_ctx.is_running, tr_on, btn_size);
  if (clicked_tr) {
    toggle_op (ImGuizmo::TRANSLATE);
  }

  ImGui::SameLine ();

  bool clicked_rot = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_rotate, "Rotate (E)",
                    &clicked_rot, !runtime_ctx.is_running, rot_on, btn_size);
  if (clicked_rot) {
    toggle_op (ImGuizmo::ROTATE);
  }

  ImGui::SameLine ();

  bool clicked_scl = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_scale, "Scale (R)",
                    &clicked_scl, !runtime_ctx.is_running, scl_on, btn_size);
  if (clicked_scl) {
    toggle_op (ImGuizmo::SCALE);
  }

  ImGui::SameLine ();
  ImGui::Dummy (ImVec2 (8.0F, 0.0F));
  ImGui::SameLine ();

  // ===================== camera combo =====================
  auto *scene = m_runtime_ctx->scene_manager.get_active ();
  if (runtime_ctx.is_running) {
    ImGui::BeginDisabled ();
  }

  wsl::comp::singl::editor_context &ed = *this->m_editor_ctx;
  const char *combo_preview = "Default";
  char combo_buf[256];
  if (ed.game_view_selected_camera != entt::null && scene != nullptr) {
    auto const &reg = scene->get_registry ();
    if (reg.valid (ed.game_view_selected_camera)) {
      std::snprintf (
          combo_buf, sizeof (combo_buf), "%s",
          scene->get_entity_name (ed.game_view_selected_camera).c_str ());
      combo_preview = combo_buf;
    }
  }

  if (ImGui::BeginCombo ("##camera_combo", combo_preview,
                         ImGuiComboFlags_WidthFitPreview)) {
    if (ImGui::Selectable ("Default",
                           ed.game_view_selected_camera == entt::null)) {
      ed.game_view_selected_camera = entt::null;
      ed.game_view_camera_mode = wsl::comp::singl::editor_context::
          game_view_cam_mode::engine_default;
      ed.game_view_camera_entity = entt::null;
    }
    if (ed.game_view_selected_camera == entt::null) {
      ImGui::SetItemDefaultFocus ();
    }

    if (scene != nullptr) {
      auto const &reg = scene->get_registry ();
      // Collect unique camera entities (both 3D cameras and 2D cameras)
      std::unordered_set<entt::entity> seen;
      auto cam_view = reg.view<wsl::comp::camera> ();
      for (entt::entity const e : cam_view) {
        std::string const &name = scene->get_entity_name (e);
        bool const selected = (e == ed.game_view_selected_camera);
        if (ImGui::Selectable (name.c_str (), selected)) {
          ed.game_view_selected_camera = e;
          ed.game_view_camera_mode
              = wsl::comp::singl::editor_context::game_view_cam_mode::entity;
          ed.game_view_camera_entity = e;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus ();
        }
        seen.emplace (e);
      }
      auto cam2d_view = reg.view<wsl::comp::camera_2d> ();
      for (entt::entity const e : cam2d_view) {
        if (seen.contains (e)) {
          continue;
        }
        std::string const &name = scene->get_entity_name (e);
        bool const selected = (e == ed.game_view_selected_camera);
        if (ImGui::Selectable (name.c_str (), selected)) {
          ed.game_view_selected_camera = e;
          ed.game_view_camera_mode
              = wsl::comp::singl::editor_context::game_view_cam_mode::entity;
          ed.game_view_camera_entity = e;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus ();
        }
      }
    }
    ImGui::EndCombo ();
  }

  if (runtime_ctx.is_running) {
    ImGui::EndDisabled ();
  }

  ImGui::SameLine ();

  // ===================== reset camera button =====================
  bool clicked_reset = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_reset_cam, "Reset Camera",
                    &clicked_reset, !runtime_ctx.is_running, false, btn_size);

  if (clicked_reset && (m_editor_ctx != nullptr)) {
    m_orbit_pivot = glm::vec3 (0.0F, 0.0F, 0.0F);
    m_editor_ctx->reset_editor_camera ();

    ed.game_view_camera_mode
        = wsl::comp::singl::editor_context::game_view_cam_mode::engine_default;
    ed.game_view_camera_entity = entt::null;
    ed.game_view_selected_camera = entt::null;
  }

  ImGui::SameLine ();

  bool const can_focus = (!runtime_ctx.is_running && (m_editor_ctx != nullptr)
                          && (m_selection != nullptr)
                          && m_selection->kind == selection_kind::entity
                          && m_selection->selected_entity != entt::null
                          && registry.all_of<wsl::comp::world_transform> (
                              m_selection->selected_entity));

  bool clicked_focus = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_focus_cam,
                    "Focus on Selection (F)", &clicked_focus, can_focus, false,
                    btn_size);

  if (clicked_focus && can_focus) {
    wsl::comp::singl::editor_context &ed = *this->m_editor_ctx;
    ed.game_view_camera_mode
        = wsl::comp::singl::editor_context::game_view_cam_mode::engine_default;
    ed.game_view_camera_entity = entt::null;
    ed.game_view_selected_camera = entt::null;

    const wsl::comp::world_transform &wt
        = registry.get<wsl::comp::world_transform> (
            m_selection->selected_entity);
    glm::vec3 const target = extract_translation (wt.value);

    m_orbit_pivot = target;
    m_editor_ctx->focus_editor_camera_to_point (target);
  }

  ImGui::SameLine ();
  ImGui::Dummy (ImVec2 (8.0F, 0.0F));
  ImGui::SameLine ();

  bool clicked_grid = false;
  draw_icon_button (m_editor_ctx, m_editor_ctx->icon_grid, "Toggle Grid (G)",
                    &clicked_grid, true, m_show_grid, btn_size);

  if (clicked_grid) {
    m_show_grid = !m_show_grid;
  }

  if (runtime_ctx.is_running) {
    ImGui::EndDisabled ();
  }

  ImGui::EndGroup ();
}

void
game_view::set_selection (ecs_selection *sel)
{
  m_selection = sel;
}

game_view::game_view (wsl::comp::singl::runtime_context *runtime_ctx,
                      wsl::comp::singl::editor_context *editor_ctx)
    : m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx)
{
}

} // namespace editor
