#include "wsl_api_module.hpp"
#include "das_interop.hpp"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_handle.h"
#include "daScript/ast/ast_interop.h"
#include "daScript/daScriptModule.h"

#include "wsl/comp/transform.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/component_meta.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/event.hpp"
#include "wsl/log/log.hpp"
#include "wsl/ray.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/reg/component_registry.hpp"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_keyboard.h>
#include <entt/core/hashed_string.hpp>
#include <cstring>

namespace wsl::das
{

namespace
{

entt::registry *g_registry = nullptr;
const engine_event *g_current_event = nullptr;
double g_delta_time = 0.0;

entt::registry *
get_registry ()
{
  return g_registry;
}

// ── Entity operations ──

uint32_t
wsl_entity_create ()
{
  auto *reg = get_registry ();
  if (!reg) {
    return NULL_ENTITY_ID;
  }
  return static_cast<uint32_t> (reg->create ());
}

void
wsl_entity_destroy (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (reg->valid (e)) {
    reg->destroy (e);
  }
}

bool
wsl_entity_valid (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  return reg->valid (static_cast<entt::entity> (entity));
}

bool
wsl_entity_is_null (uint32_t entity)
{
  return entity == NULL_ENTITY_ID;
}

uint32_t
wsl_null_entity ()
{
  return NULL_ENTITY_ID;
}

// ── Generic component queries ──

bool
wsl_has_component (uint32_t type_id, uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e)) {
    return false;
  }
  // Check entt storage first (C++ components)
  auto *storage = reg->storage (type_id);
  if (storage && storage->contains (e)) {
    return true;
  }
  // Check das component storage
  if (reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
    if (runtime_ctx) {
      return runtime_ctx->component_registry ().das_component_contains (type_id,
                                                                        e);
    }
  }
  return false;
}

// ── Per-component add/remove ──

bool
wsl_add_transform (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || reg->all_of<comp::transform> (e)) {
    return false;
  }
  reg->emplace<comp::transform> (e);
  return true;
}

bool
wsl_remove_transform (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return false;
  }
  reg->remove<comp::transform> (e);
  return true;
}

bool
wsl_add_camera (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || reg->all_of<comp::camera> (e)) {
    return false;
  }
  reg->emplace<comp::camera> (e);
  return true;
}

bool
wsl_remove_camera (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return false;
  }
  reg->remove<comp::camera> (e);
  return true;
}

bool
wsl_add_hierarchy (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || reg->all_of<comp::hierarchy> (e)) {
    return false;
  }
  reg->emplace<comp::hierarchy> (e);
  return true;
}

bool
wsl_remove_hierarchy (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::hierarchy> (e)) {
    return false;
  }
  reg->remove<comp::hierarchy> (e);
  return true;
}

bool
wsl_add_world_transform (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || reg->all_of<comp::world_transform> (e)) {
    return false;
  }
  reg->emplace<comp::world_transform> (e);
  return true;
}

bool
wsl_remove_world_transform (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::world_transform> (e)) {
    return false;
  }
  reg->remove<comp::world_transform> (e);
  return true;
}

// ── Generic component add/remove ──

bool
wsl_add_component (uint32_t type_id, uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e)) {
    return false;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return false;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return false;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (type_id);
  if (!desc) {
    return false;
  }
  if (desc->is_das_component) {
    return comp_reg.das_component_add (type_id, e);
  }
  if (desc->emplace_default) {
    return desc->emplace_default (*reg, e);
  }
  return false;
}

bool
wsl_remove_component (uint32_t type_id, uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e)) {
    return false;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return false;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return false;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (type_id);
  if (!desc) {
    return false;
  }
  if (desc->is_das_component) {
    return comp_reg.das_component_remove (type_id, e);
  }
  if (desc->remove) {
    return desc->remove (*reg, e);
  }
  return false;
}

// ── Camera field access ──

float
wsl_get_camera_fov (uint32_t camera)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 60.0f;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return 60.0f;
  }
  return reg->get<comp::camera> (e).fov ();
}

void
wsl_set_camera_fov (uint32_t camera, float fov)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return;
  }
  reg->get<comp::camera> (e).fov () = fov;
}

float
wsl_get_camera_near (uint32_t camera)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.5f;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return 0.5f;
  }
  return reg->get<comp::camera> (e).near ();
}

void
wsl_set_camera_near (uint32_t camera, float near_val)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return;
  }
  reg->get<comp::camera> (e).near () = near_val;
}

float
wsl_get_camera_far (uint32_t camera)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 50.0f;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return 50.0f;
  }
  return reg->get<comp::camera> (e).far ();
}

void
wsl_set_camera_far (uint32_t camera, float far_val)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return;
  }
  reg->get<comp::camera> (e).far () = far_val;
}

float
wsl_get_camera_aspect_ratio (uint32_t camera)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 1.0f;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return 1.0f;
  }
  return reg->get<comp::camera> (e).aspect_ratio ();
}

void
wsl_set_camera_aspect_ratio (uint32_t camera, float aspect)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (camera);
  if (!reg->valid (e) || !reg->all_of<comp::camera> (e)) {
    return;
  }
  reg->get<comp::camera> (e).aspect_ratio () = aspect;
}

// ── Transform operations (per-component getters) ──

void
wsl_set_position (uint32_t entity, float x, float y, float z)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return;
  }
  auto &t = reg->get<comp::transform> (e);
  t.position = glm::vec3 (x, y, z);
}

void
wsl_set_rotation_euler (uint32_t entity, float pitch, float yaw, float roll)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return;
  }
  auto &t = reg->get<comp::transform> (e);
  t.set_rotation_xyz (glm::vec3 (pitch, yaw, roll));
}

void
wsl_set_scale (uint32_t entity, float x, float y, float z)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return;
  }
  auto &t = reg->get<comp::transform> (e);
  t.scale = glm::vec3 (x, y, z);
}

float
wsl_get_position_x (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).position.x ();
}

float
wsl_get_position_y (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).position.y ();
}

float
wsl_get_position_z (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).position.z ();
}

float
wsl_get_rotation_pitch (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  math::vec3f euler = reg->get<comp::transform> (e).get_rotation_xyz ();
  return euler.x ();
}

float
wsl_get_rotation_yaw (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  math::vec3f euler = reg->get<comp::transform> (e).get_rotation_xyz ();
  return euler.y ();
}

float
wsl_get_rotation_roll (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  math::vec3f euler = reg->get<comp::transform> (e).get_rotation_xyz ();
  return euler.z ();
}

float
wsl_get_scale_x (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).scale.x ();
}

float
wsl_get_scale_y (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).scale.y ();
}

float
wsl_get_scale_z (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::transform> (e)) {
    return 0.0f;
  }
  return reg->get<comp::transform> (e).scale.z ();
}

// ── Scene operations ──

uint32_t
wsl_find_entity_by_name (const char *name)
{
  auto *reg = get_registry ();
  if (!reg || !name) {
    return NULL_ENTITY_ID;
  }

  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return NULL_ENTITY_ID;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return NULL_ENTITY_ID;
  }

  auto *scene = runtime_ctx->scene_manager ().get_active ();
  if (!scene) {
    return NULL_ENTITY_ID;
  }

  for (const auto &[entity, entity_name] : scene->get_entity_names ()) {
    if (entity_name == name) {
      return static_cast<uint32_t> (entity);
    }
  }
  return NULL_ENTITY_ID;
}

uint32_t
wsl_get_active_camera ()
{
  auto *reg = get_registry ();
  if (!reg) {
    return NULL_ENTITY_ID;
  }

  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return NULL_ENTITY_ID;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return NULL_ENTITY_ID;
  }

  auto *scene = runtime_ctx->scene_manager ().get_active ();
  if (!scene) {
    return NULL_ENTITY_ID;
  }

  return static_cast<uint32_t> (scene->camera);
}

void
wsl_set_active_camera (uint32_t entity)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }

  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return;
  }

  auto *scene = runtime_ctx->scene_manager ().get_active ();
  if (!scene) {
    return;
  }

  scene->camera = static_cast<entt::entity> (entity);
}

uint32_t
wsl_instantiate_prefab (const char *path)
{
  auto *reg = get_registry ();
  if (!reg || !path) {
    return NULL_ENTITY_ID;
  }

  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return NULL_ENTITY_ID;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return NULL_ENTITY_ID;
  }

  const entt::id_type prefab_hash = entt::hashed_string{ path };
  rsc::scene_id prefab_id{ prefab_hash };
  entt::entity e
      = runtime_ctx->resource_manager ().instantiate_prefab (prefab_id);
  return static_cast<uint32_t> (e);
}

// ── Editor viewport (global-state) ──

float g_editor_img_min_x = 0.0f;
float g_editor_img_min_y = 0.0f;
float g_editor_img_size_x = 0.0f;
float g_editor_img_size_y = 0.0f;

void
wsl_refresh_editor_viewport ()
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return;
  }
  auto *editor_ctx = runtime_ctx->editor_ctx ();
  if (!editor_ctx) {
    g_editor_img_min_x = 0.0f;
    g_editor_img_min_y = 0.0f;
    g_editor_img_size_x = 0.0f;
    g_editor_img_size_y = 0.0f;
    return;
  }
  auto const &mn = editor_ctx->last_img_min ();
  auto const &sz = editor_ctx->last_img_size ();
  g_editor_img_min_x = mn.x;
  g_editor_img_min_y = mn.y;
  g_editor_img_size_x = sz.x;
  g_editor_img_size_y = sz.y;
}

float
wsl_get_editor_img_min_x ()
{
  return g_editor_img_min_x;
}
float
wsl_get_editor_img_min_y ()
{
  return g_editor_img_min_y;
}
float
wsl_get_editor_img_size_x ()
{
  return g_editor_img_size_x;
}
float
wsl_get_editor_img_size_y ()
{
  return g_editor_img_size_y;
}

// ── Component type ID constants ──

uint32_t
wsl_type_id_transform ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::transform>::value ());
}

uint32_t
wsl_type_id_camera ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::camera>::value ());
}

uint32_t
wsl_type_id_hierarchy ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::hierarchy>::value ());
}

uint32_t
wsl_type_id_world_transform ()
{
  return static_cast<uint32_t> (
      entt::type_hash<comp::world_transform>::value ());
}

// ── Event query functions ──

uint32_t
wsl_get_event_kind ()
{
  if (!g_current_event) {
    return 0;
  }
  return static_cast<uint32_t> (g_current_event->kind ());
}

float
wsl_get_event_mouse_dx ()
{
  if (!g_current_event
      || g_current_event->kind () != event_kind::mouse_motion) {
    return 0.0f;
  }
  return static_cast<float> (g_current_event->as_mouse_motion ().xrel);
}

float
wsl_get_event_mouse_dy ()
{
  if (!g_current_event
      || g_current_event->kind () != event_kind::mouse_motion) {
    return 0.0f;
  }
  return static_cast<float> (g_current_event->as_mouse_motion ().yrel);
}

int
wsl_get_event_mouse_x ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::mouse_button_down
          && g_current_event->kind () != event_kind::mouse_button_up)) {
    return 0;
  }
  return g_current_event->as_mouse_button ().x;
}

int
wsl_get_event_mouse_y ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::mouse_button_down
          && g_current_event->kind () != event_kind::mouse_button_up)) {
    return 0;
  }
  return g_current_event->as_mouse_button ().y;
}

uint32_t
wsl_get_event_mouse_button ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::mouse_button_down
          && g_current_event->kind () != event_kind::mouse_button_up)) {
    return 0;
  }
  return g_current_event->as_mouse_button ().button;
}

// ── Keyboard event functions ──

int
wsl_get_event_key_scancode ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::key_down
          && g_current_event->kind () != event_kind::key_up)) {
    return 0;
  }
  return static_cast<int> (g_current_event->as_keyboard ().scancode);
}

int
wsl_get_event_key_keycode ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::key_down
          && g_current_event->kind () != event_kind::key_up)) {
    return 0;
  }
  return static_cast<int> (g_current_event->as_keyboard ().key);
}

bool
wsl_get_event_key_repeat ()
{
  if (!g_current_event
      || (g_current_event->kind () != event_kind::key_down
          && g_current_event->kind () != event_kind::key_up)) {
    return false;
  }
  return g_current_event->as_keyboard ().repeat;
}

// ── Keyboard state ──

bool
wsl_is_key_pressed (int scancode)
{
  int num_keys = 0;
  const bool *state = SDL_GetKeyboardState (&num_keys);
  if (!state || scancode < 0 || scancode >= num_keys) {
    return false;
  }
  return state[scancode];
}

// ── SDL window operations ──

bool
wsl_set_relative_mouse_mode (bool enabled)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return false;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx || !runtime_ctx->window ().handler ()) {
    return false;
  }
  return SDL_SetWindowRelativeMouseMode (runtime_ctx->window ().handler (),
                                         enabled);
}

bool
wsl_cursor_visible ()
{
  return SDL_CursorVisible ();
}

void
wsl_show_cursor ()
{
  SDL_ShowCursor ();
}

void
wsl_hide_cursor ()
{
  SDL_HideCursor ();
}

// ── Window size (global-state) ──

uint32_t g_window_w = 0;
uint32_t g_window_h = 0;

void
wsl_refresh_window_size ()
{
  auto *reg = get_registry ();
  if (!reg) {
    g_window_w = 0;
    g_window_h = 0;
    return;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    g_window_w = 0;
    g_window_h = 0;
    return;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    g_window_w = 0;
    g_window_h = 0;
    return;
  }
  runtime_ctx->window ().get_size (g_window_w, g_window_h);
}

uint32_t
wsl_get_window_width ()
{
  return g_window_w;
}

uint32_t
wsl_get_window_height ()
{
  return g_window_h;
}

// ── Entity iteration (global-state) ──

uint32_t g_entity_buffer[512] = {};
uint32_t g_entity_count = 0;

void
wsl_refresh_entities_with_transform ()
{
  auto *reg = get_registry ();
  if (!reg) {
    g_entity_count = 0;
    return;
  }
  g_entity_count = 0;
  auto view = reg->view<comp::transform> ();
  for (auto e : view) {
    if (g_entity_count >= 512) {
      break;
    }
    g_entity_buffer[g_entity_count++] = static_cast<uint32_t> (e);
  }
}

uint32_t
wsl_get_entity_count ()
{
  return g_entity_count;
}

uint32_t
wsl_get_entity_at (uint32_t index)
{
  if (index >= g_entity_count) {
    return NULL_ENTITY_ID;
  }
  return g_entity_buffer[index];
}

// ── Component type lookup ──

uint32_t
wsl_get_component_type_id (const char *display_name)
{
  auto *reg = get_registry ();
  if (!reg || !display_name) {
    return 0;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return 0;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return 0;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (display_name);
  if (!desc) {
    return 0;
  }
  return static_cast<uint32_t> (desc->type_id);
}

// ── Entity iteration by component ──

void
wsl_refresh_entities_with_component (uint32_t type_id)
{
  auto *reg = get_registry ();
  if (!reg) {
    g_entity_count = 0;
    return;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    g_entity_count = 0;
    return;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    g_entity_count = 0;
    return;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (type_id);
  if (!desc) {
    g_entity_count = 0;
    return;
  }
  g_entity_count = 0;
  for (auto e : reg->storage<entt::entity> ()) {
    bool has = false;
    if (desc->is_das_component) {
      has = comp_reg.das_component_contains (type_id, e);
    } else if (desc->contains) {
      has = desc->contains (*reg, e);
    }
    if (has) {
      if (g_entity_count >= 512) {
        break;
      }
      g_entity_buffer[g_entity_count++] = static_cast<uint32_t> (e);
    }
  }
}

// ── Component field access ──

float
wsl_get_component_field_f (uint32_t entity, uint32_t type_id, int offset)
{
  auto *reg = get_registry ();
  if (!reg) {
    return 0.0f;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return 0.0f;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return 0.0f;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (type_id);
  if (!desc) {
    return 0.0f;
  }
  auto e = static_cast<entt::entity> (entity);
  bool has = false;
  if (desc->is_das_component) {
    has = comp_reg.das_component_contains (type_id, e);
  } else if (desc->contains) {
    has = desc->contains (*reg, e);
  }
  if (!has) {
    return 0.0f;
  }
  const uint8_t *data = comp_reg.das_component_data (type_id, e);
  if (!data) {
    return 0.0f;
  }
  if (offset < 0
      || static_cast<uint32_t> (offset) + sizeof (float)
             > static_cast<uint32_t> (desc->das_struct_size)) {
    return 0.0f;
  }
  float value = 0.0f;
  std::memcpy (&value, data + offset, sizeof (float));
  return value;
}

void
wsl_set_component_field_f (uint32_t entity, uint32_t type_id, int offset,
                           float value)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *desc = comp_reg.find_world_component (type_id);
  if (!desc) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  bool has = false;
  if (desc->is_das_component) {
    has = comp_reg.das_component_contains (type_id, e);
  } else if (desc->contains) {
    has = desc->contains (*reg, e);
  }
  if (!has) {
    return;
  }
  uint8_t *data = comp_reg.das_component_data (type_id, e);
  if (!data) {
    return;
  }
  if (offset < 0
      || static_cast<uint32_t> (offset) + sizeof (float)
             > static_cast<uint32_t> (desc->das_struct_size)) {
    return;
  }
  std::memcpy (data + offset, &value, sizeof (float));
}

// ── Generic component type lookup (for get_component<T> / set_component<T>) ──

uint32_t
wsl_get_component_type_id_by_name (const char *type_name)
{
  auto *reg = get_registry ();
  if (!reg || !type_name) {
    return 0;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return 0;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return 0;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *info = comp_reg.find_component_type_info (type_name);
  if (!info) {
    return 0;
  }
  return static_cast<uint32_t> (info->type_id);
}

int
wsl_get_component_kind_by_name (const char *type_name)
{
  auto *reg = get_registry ();
  if (!reg || !type_name) {
    return 0;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return 0;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return 0;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *info = comp_reg.find_component_type_info (type_name);
  if (!info) {
    return 0;
  }
  return static_cast<int> (info->kind);
}

int
wsl_get_component_struct_size_by_name (const char *type_name)
{
  auto *reg = get_registry ();
  if (!reg || !type_name) {
    return 0;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return 0;
  }
  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return 0;
  }
  auto &comp_reg = runtime_ctx->component_registry ();
  auto *info = comp_reg.find_component_type_info (type_name);
  if (!info) {
    return 0;
  }
  return static_cast<int> (info->struct_size);
}

void
wsl_get_component_into (uint32_t entity, uint32_t type_id, int kind, void *dest)
{
  auto *reg = get_registry ();
  if (!reg || !dest) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  auto component_kind = static_cast<reg::ComponentKind> (kind);

  if (component_kind == reg::ComponentKind::DAS_SCRIPT) {
    // daScript component: memcpy from raw byte storage
    if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
      return;
    }
    auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
    if (!runtime_ctx) {
      return;
    }
    auto &comp_reg = runtime_ctx->component_registry ();
    if (!comp_reg.das_component_contains (type_id, e)) {
      return;
    }
    const auto *data = comp_reg.das_component_data (type_id, e);
    if (!data) {
      return;
    }
    auto *desc = comp_reg.find_world_component (type_id);
    size_t size = desc ? static_cast<size_t> (desc->das_struct_size) : 0;
    if (size > 0) {
      std::memcpy (dest, data, size);
    }
  } else {
    // C++ component: use the accessor from lookup table
    if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
      return;
    }
    auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
    if (!runtime_ctx) {
      return;
    }
    auto &comp_reg = runtime_ctx->component_registry ();
    auto *info = comp_reg.find_component_type_info_by_id (type_id);
    if (info && info->get) {
      info->get (entity, dest);
    }
  }
}

void
wsl_set_component_from (uint32_t entity, uint32_t type_id, int kind,
                        const void *src)
{
  auto *reg = get_registry ();
  if (!reg || !src) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  auto component_kind = static_cast<reg::ComponentKind> (kind);

  if (component_kind == reg::ComponentKind::DAS_SCRIPT) {
    // daScript component: memcpy to raw byte storage
    if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
      return;
    }
    auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
    if (!runtime_ctx) {
      return;
    }
    auto &comp_reg = runtime_ctx->component_registry ();
    if (!comp_reg.das_component_contains (type_id, e)) {
      return;
    }
    auto *data = comp_reg.das_component_data (type_id, e);
    if (!data) {
      return;
    }
    auto *desc = comp_reg.find_world_component (type_id);
    size_t size = desc ? static_cast<size_t> (desc->das_struct_size) : 0;
    if (size > 0) {
      std::memcpy (data, src, size);
    }
  } else {
    // C++ component: use the accessor from lookup table
    if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
      return;
    }
    auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
    if (!runtime_ctx) {
      return;
    }
    auto &comp_reg = runtime_ctx->component_registry ();
    auto *info = comp_reg.find_component_type_info_by_id (type_id);
    if (info && info->set) {
      info->set (entity, src);
    }
  }
}

// ── Raycasting (global-state) ──

float g_ray_origin[3] = {};
float g_ray_dir[3] = {};
float g_hit_point[3] = {};

bool
wsl_make_pick_ray (uint32_t camera_entity, float mouse_x, float mouse_y,
                   float vp_x, float vp_y, float vp_w, float vp_h)
{
  auto *reg = get_registry ();
  if (!reg) {
    return false;
  }
  auto e = static_cast<entt::entity> (camera_entity);
  if (!reg->valid (e)
      || !reg->all_of<comp::camera, comp::world_transform> (e)) {
    return false;
  }
  wsl::pick_ray ray
      = wsl::make_pick_ray (*reg, e, mouse_x, mouse_y, vp_x, vp_y, vp_w, vp_h);
  g_ray_origin[0] = ray.origin.x;
  g_ray_origin[1] = ray.origin.y;
  g_ray_origin[2] = ray.origin.z;
  g_ray_dir[0] = ray.dir.x;
  g_ray_dir[1] = ray.dir.y;
  g_ray_dir[2] = ray.dir.z;
  return true;
}

float
wsl_get_ray_origin_x ()
{
  return g_ray_origin[0];
}
float
wsl_get_ray_origin_y ()
{
  return g_ray_origin[1];
}
float
wsl_get_ray_origin_z ()
{
  return g_ray_origin[2];
}

float
wsl_get_ray_dir_x ()
{
  return g_ray_dir[0];
}
float
wsl_get_ray_dir_y ()
{
  return g_ray_dir[1];
}
float
wsl_get_ray_dir_z ()
{
  return g_ray_dir[2];
}

bool
wsl_ray_plane_intersect (float origin_x, float origin_y, float origin_z,
                         float dir_x, float dir_y, float dir_z, float plane_x,
                         float plane_y, float plane_z, float plane_nx,
                         float plane_ny, float plane_nz)
{
  wsl::pick_ray ray;
  ray.origin = glm::vec3 (origin_x, origin_y, origin_z);
  ray.dir = glm::vec3 (dir_x, dir_y, dir_z);
  glm::vec3 plane_point (plane_x, plane_y, plane_z);
  glm::vec3 plane_normal (plane_nx, plane_ny, plane_nz);
  float t_hit = 0.0F;
  glm::vec3 hit_point;
  if (!wsl::ray_plane_intersect (ray, plane_point, plane_normal, t_hit,
                                 hit_point)) {
    return false;
  }
  g_hit_point[0] = hit_point.x;
  g_hit_point[1] = hit_point.y;
  g_hit_point[2] = hit_point.z;
  return true;
}

float
wsl_get_hit_x ()
{
  return g_hit_point[0];
}
float
wsl_get_hit_y ()
{
  return g_hit_point[1];
}
float
wsl_get_hit_z ()
{
  return g_hit_point[2];
}

class Module_WeaselApi : public ::das::Module
{
public:
  Module_WeaselApi () : Module ("weasel_api")
  {
    ::das::ModuleLibrary lib (this);
    lib.addBuiltInModule ();

    addExtern<DAS_BIND_FUN (wsl_get_delta_time)> (
        *this, lib, "get_delta_time", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_delta_time");

    // Logging
    addExtern<DAS_BIND_FUN (wsl_log_info)> (*this, lib, "log_info",
                                            ::das::SideEffects::modifyExternal,
                                            "wsl::das::wsl_log_info")
        ->arg ("msg");

    addExtern<DAS_BIND_FUN (wsl_log_debug)> (*this, lib, "log_debug",
                                             ::das::SideEffects::modifyExternal,
                                             "wsl::das::wsl_log_debug")
        ->arg ("msg");

    addExtern<DAS_BIND_FUN (wsl_log_warn)> (*this, lib, "log_warn",
                                            ::das::SideEffects::modifyExternal,
                                            "wsl::das::wsl_log_warn")
        ->arg ("msg");

    addExtern<DAS_BIND_FUN (wsl_log_error)> (*this, lib, "log_error",
                                             ::das::SideEffects::modifyExternal,
                                             "wsl::das::wsl_log_error")
        ->arg ("msg");

    // Entity operations
    addExtern<DAS_BIND_FUN (wsl_entity_create)> (
        *this, lib, "entity_create", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_entity_create");

    addExtern<DAS_BIND_FUN (wsl_entity_destroy)> (
        *this, lib, "entity_destroy", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_entity_destroy")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_entity_valid)> (
        *this, lib, "entity_valid", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_entity_valid")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_entity_is_null)> (
        *this, lib, "entity_is_null", ::das::SideEffects::none,
        "wsl::das::wsl_entity_is_null")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_null_entity)> (*this, lib, "null_entity",
                                               ::das::SideEffects::none,
                                               "wsl::das::wsl_null_entity");

    // Generic component queries
    addExtern<DAS_BIND_FUN (wsl_has_component)> (
        *this, lib, "has_component", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_has_component")
        ->args ({ "type_id", "entity" });

    // Per-component add/remove
    addExtern<DAS_BIND_FUN (wsl_add_transform)> (
        *this, lib, "add_transform", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_add_transform")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_remove_transform)> (
        *this, lib, "remove_transform", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_remove_transform")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_add_camera)> (
        *this, lib, "add_camera", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_add_camera")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_remove_camera)> (
        *this, lib, "remove_camera", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_remove_camera")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_add_hierarchy)> (
        *this, lib, "add_hierarchy", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_add_hierarchy")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_remove_hierarchy)> (
        *this, lib, "remove_hierarchy", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_remove_hierarchy")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_add_world_transform)> (
        *this, lib, "add_world_transform", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_add_world_transform")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_remove_world_transform)> (
        *this, lib, "remove_world_transform",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_remove_world_transform")
        ->arg ("entity");

    // Generic component add/remove
    addExtern<DAS_BIND_FUN (wsl_add_component)> (
        *this, lib, "add_component", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_add_component")
        ->args ({ "type_id", "entity" });

    addExtern<DAS_BIND_FUN (wsl_remove_component)> (
        *this, lib, "remove_component", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_remove_component")
        ->args ({ "type_id", "entity" });

    // Transform operations
    addExtern<DAS_BIND_FUN (wsl_set_position)> (
        *this, lib, "set_position", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_position")
        ->args ({ "entity", "x", "y", "z" });

    addExtern<DAS_BIND_FUN (wsl_set_rotation_euler)> (
        *this, lib, "set_rotation", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_rotation_euler")
        ->args ({ "entity", "pitch", "yaw", "roll" });

    addExtern<DAS_BIND_FUN (wsl_set_scale)> (*this, lib, "set_scale",
                                             ::das::SideEffects::modifyExternal,
                                             "wsl::das::wsl_set_scale")
        ->args ({ "entity", "x", "y", "z" });

    addExtern<DAS_BIND_FUN (wsl_get_position_x)> (
        *this, lib, "get_position_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_position_x")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_position_y)> (
        *this, lib, "get_position_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_position_y")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_position_z)> (
        *this, lib, "get_position_z", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_position_z")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_get_rotation_pitch)> (
        *this, lib, "get_rotation_pitch", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_rotation_pitch")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_rotation_yaw)> (
        *this, lib, "get_rotation_yaw", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_rotation_yaw")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_rotation_roll)> (
        *this, lib, "get_rotation_roll", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_rotation_roll")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_get_scale_x)> (
        *this, lib, "get_scale_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_scale_x")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_scale_y)> (
        *this, lib, "get_scale_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_scale_y")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_get_scale_z)> (
        *this, lib, "get_scale_z", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_scale_z")
        ->arg ("entity");

    // Scene operations
    addExtern<DAS_BIND_FUN (wsl_find_entity_by_name)> (
        *this, lib, "find_entity_by_name", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_find_entity_by_name")
        ->arg ("name");

    addExtern<DAS_BIND_FUN (wsl_get_active_camera)> (
        *this, lib, "get_active_camera", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_active_camera");

    addExtern<DAS_BIND_FUN (wsl_set_active_camera)> (
        *this, lib, "set_active_camera", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_active_camera")
        ->arg ("entity");

    addExtern<DAS_BIND_FUN (wsl_instantiate_prefab)> (
        *this, lib, "instantiate_prefab", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_instantiate_prefab")
        ->arg ("path");

    // Editor viewport
    addExtern<DAS_BIND_FUN (wsl_refresh_editor_viewport)> (
        *this, lib, "refresh_editor_viewport",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_refresh_editor_viewport");

    addExtern<DAS_BIND_FUN (wsl_get_editor_img_min_x)> (
        *this, lib, "get_editor_img_min_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_editor_img_min_x");
    addExtern<DAS_BIND_FUN (wsl_get_editor_img_min_y)> (
        *this, lib, "get_editor_img_min_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_editor_img_min_y");
    addExtern<DAS_BIND_FUN (wsl_get_editor_img_size_x)> (
        *this, lib, "get_editor_img_size_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_editor_img_size_x");
    addExtern<DAS_BIND_FUN (wsl_get_editor_img_size_y)> (
        *this, lib, "get_editor_img_size_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_editor_img_size_y");

    // Component type ID constants
    addExtern<DAS_BIND_FUN (wsl_type_id_transform)> (
        *this, lib, "TYPE_TRANSFORM", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_transform");

    addExtern<DAS_BIND_FUN (wsl_type_id_camera)> (
        *this, lib, "TYPE_CAMERA", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_camera");

    addExtern<DAS_BIND_FUN (wsl_type_id_hierarchy)> (
        *this, lib, "TYPE_HIERARCHY", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_hierarchy");

    addExtern<DAS_BIND_FUN (wsl_type_id_world_transform)> (
        *this, lib, "TYPE_WORLD_TRANSFORM", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_world_transform");

    // Event query functions
    addExtern<DAS_BIND_FUN (wsl_get_event_kind)> (
        *this, lib, "get_event_kind", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_kind");

    addExtern<DAS_BIND_FUN (wsl_get_event_mouse_dx)> (
        *this, lib, "get_event_mouse_dx", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_mouse_dx");

    addExtern<DAS_BIND_FUN (wsl_get_event_mouse_dy)> (
        *this, lib, "get_event_mouse_dy", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_mouse_dy");

    addExtern<DAS_BIND_FUN (wsl_get_event_mouse_x)> (
        *this, lib, "get_event_mouse_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_mouse_x");

    addExtern<DAS_BIND_FUN (wsl_get_event_mouse_y)> (
        *this, lib, "get_event_mouse_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_mouse_y");

    addExtern<DAS_BIND_FUN (wsl_get_event_mouse_button)> (
        *this, lib, "get_event_mouse_button",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_mouse_button");

    // Keyboard event functions
    addExtern<DAS_BIND_FUN (wsl_get_event_key_scancode)> (
        *this, lib, "get_event_key_scancode",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_key_scancode");

    addExtern<DAS_BIND_FUN (wsl_get_event_key_keycode)> (
        *this, lib, "get_event_key_keycode", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_key_keycode");

    addExtern<DAS_BIND_FUN (wsl_get_event_key_repeat)> (
        *this, lib, "get_event_key_repeat", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_event_key_repeat");

    // Keyboard state
    addExtern<DAS_BIND_FUN (wsl_is_key_pressed)> (
        *this, lib, "is_key_pressed", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_is_key_pressed")
        ->arg ("scancode");

    // Camera field access
    addExtern<DAS_BIND_FUN (wsl_get_camera_fov)> (
        *this, lib, "get_camera_fov", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_camera_fov")
        ->arg ("camera");
    addExtern<DAS_BIND_FUN (wsl_set_camera_fov)> (
        *this, lib, "set_camera_fov", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_camera_fov")
        ->args ({ "camera", "fov" });
    addExtern<DAS_BIND_FUN (wsl_get_camera_near)> (
        *this, lib, "get_camera_near", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_camera_near")
        ->arg ("camera");
    addExtern<DAS_BIND_FUN (wsl_set_camera_near)> (
        *this, lib, "set_camera_near", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_camera_near")
        ->args ({ "camera", "near_val" });
    addExtern<DAS_BIND_FUN (wsl_get_camera_far)> (
        *this, lib, "get_camera_far", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_camera_far")
        ->arg ("camera");
    addExtern<DAS_BIND_FUN (wsl_set_camera_far)> (
        *this, lib, "set_camera_far", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_camera_far")
        ->args ({ "camera", "far_val" });
    addExtern<DAS_BIND_FUN (wsl_get_camera_aspect_ratio)> (
        *this, lib, "get_camera_aspect_ratio",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_camera_aspect_ratio")
        ->arg ("camera");
    addExtern<DAS_BIND_FUN (wsl_set_camera_aspect_ratio)> (
        *this, lib, "set_camera_aspect_ratio",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_camera_aspect_ratio")
        ->args ({ "camera", "aspect" });

    // SDL window operations
    addExtern<DAS_BIND_FUN (wsl_set_relative_mouse_mode)> (
        *this, lib, "set_relative_mouse_mode",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_relative_mouse_mode")
        ->arg ("enabled");

    addExtern<DAS_BIND_FUN (wsl_cursor_visible)> (
        *this, lib, "cursor_visible", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_cursor_visible");

    addExtern<DAS_BIND_FUN (wsl_show_cursor)> (
        *this, lib, "show_cursor", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_show_cursor");

    addExtern<DAS_BIND_FUN (wsl_hide_cursor)> (
        *this, lib, "hide_cursor", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_hide_cursor");

    addExtern<DAS_BIND_FUN (wsl_refresh_window_size)> (
        *this, lib, "refresh_window_size", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_refresh_window_size");

    addExtern<DAS_BIND_FUN (wsl_get_window_width)> (
        *this, lib, "get_window_width", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_window_width");

    addExtern<DAS_BIND_FUN (wsl_get_window_height)> (
        *this, lib, "get_window_height", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_window_height");

    // Entity iteration
    addExtern<DAS_BIND_FUN (wsl_refresh_entities_with_transform)> (
        *this, lib, "refresh_entities_with_transform",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_refresh_entities_with_transform");

    addExtern<DAS_BIND_FUN (wsl_get_entity_count)> (
        *this, lib, "get_entity_count", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_entity_count");

    addExtern<DAS_BIND_FUN (wsl_get_entity_at)> (
        *this, lib, "get_entity_at", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_entity_at")
        ->arg ("index");

    // Component type lookup
    addExtern<DAS_BIND_FUN (wsl_get_component_type_id)> (
        *this, lib, "get_component_type_id", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_type_id")
        ->arg ("display_name");

    // Entity iteration by component
    addExtern<DAS_BIND_FUN (wsl_refresh_entities_with_component)> (
        *this, lib, "refresh_entities_with_component",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_refresh_entities_with_component")
        ->arg ("type_id");

    // Component field access
    addExtern<DAS_BIND_FUN (wsl_get_component_field_f)> (
        *this, lib, "get_component_field_f", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_field_f")
        ->args ({ "entity", "type_id", "offset" });

    addExtern<DAS_BIND_FUN (wsl_set_component_field_f)> (
        *this, lib, "set_component_field_f", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_component_field_f")
        ->args ({ "entity", "type_id", "offset", "value" });
    // Generic component type lookup (for get_component<T> / set_component<T>)
    addExtern<DAS_BIND_FUN (wsl_get_component_type_id_by_name)> (
        *this, lib, "_get_component_type_id_by_name",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_type_id_by_name")
        ->arg ("type_name");

    addExtern<DAS_BIND_FUN (wsl_get_component_kind_by_name)> (
        *this, lib, "_get_component_kind_by_name",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_kind_by_name")
        ->arg ("type_name");

    addExtern<DAS_BIND_FUN (wsl_get_component_struct_size_by_name)> (
        *this, lib, "_get_component_struct_size_by_name",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_struct_size_by_name")
        ->arg ("type_name");

    addExtern<DAS_BIND_FUN (wsl_get_component_into)> (
        *this, lib, "_get_component_into", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_into")
        ->args ({ "entity", "type_id", "kind", "dest" });

    addExtern<DAS_BIND_FUN (wsl_set_component_from)> (
        *this, lib, "_set_component_from", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_component_from")
        ->args ({ "entity", "type_id", "kind", "src" });
    // Raycasting
    addExtern<DAS_BIND_FUN (wsl_make_pick_ray)> (
        *this, lib, "make_pick_ray", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_make_pick_ray")
        ->args ({ "camera_entity", "mouse_x", "mouse_y", "vp_x", "vp_y", "vp_w",
                  "vp_h" });

    addExtern<DAS_BIND_FUN (wsl_get_ray_origin_x)> (
        *this, lib, "get_ray_origin_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_origin_x");
    addExtern<DAS_BIND_FUN (wsl_get_ray_origin_y)> (
        *this, lib, "get_ray_origin_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_origin_y");
    addExtern<DAS_BIND_FUN (wsl_get_ray_origin_z)> (
        *this, lib, "get_ray_origin_z", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_origin_z");

    addExtern<DAS_BIND_FUN (wsl_get_ray_dir_x)> (
        *this, lib, "get_ray_dir_x", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_dir_x");
    addExtern<DAS_BIND_FUN (wsl_get_ray_dir_y)> (
        *this, lib, "get_ray_dir_y", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_dir_y");
    addExtern<DAS_BIND_FUN (wsl_get_ray_dir_z)> (
        *this, lib, "get_ray_dir_z", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_ray_dir_z");

    addExtern<DAS_BIND_FUN (wsl_ray_plane_intersect)> (
        *this, lib, "ray_plane_intersect", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_ray_plane_intersect")
        ->args ({ "origin_x", "origin_y", "origin_z", "dir_x", "dir_y", "dir_z",
                  "plane_x", "plane_y", "plane_z", "plane_nx", "plane_ny",
                  "plane_nz" });

    addExtern<DAS_BIND_FUN (wsl_get_hit_x)> (*this, lib, "get_hit_x",
                                             ::das::SideEffects::accessExternal,
                                             "wsl::das::wsl_get_hit_x");
    addExtern<DAS_BIND_FUN (wsl_get_hit_y)> (*this, lib, "get_hit_y",
                                             ::das::SideEffects::accessExternal,
                                             "wsl::das::wsl_get_hit_y");
    addExtern<DAS_BIND_FUN (wsl_get_hit_z)> (*this, lib, "get_hit_z",
                                             ::das::SideEffects::accessExternal,
                                             "wsl::das::wsl_get_hit_z");

    // Constants
    ::das::addConstant<uint32_t> (*this, "SDL_BUTTON_LEFT", 1);
    ::das::addConstant<uint32_t> (*this, "SDL_BUTTON_RIGHT", 3);
    ::das::addConstant<uint32_t> (
        *this, "EVENT_MOUSE_MOTION",
        static_cast<uint32_t> (event_kind::mouse_motion));
    ::das::addConstant<uint32_t> (
        *this, "EVENT_MOUSE_BUTTON_DOWN",
        static_cast<uint32_t> (event_kind::mouse_button_down));
    ::das::addConstant<uint32_t> (
        *this, "EVENT_MOUSE_BUTTON_UP",
        static_cast<uint32_t> (event_kind::mouse_button_up));
    ::das::addConstant<uint32_t> (*this, "EVENT_QUIT",
                                  static_cast<uint32_t> (event_kind::quit));
    ::das::addConstant<uint32_t> (*this, "EVENT_KEY_DOWN",
                                  static_cast<uint32_t> (event_kind::key_down));
    ::das::addConstant<uint32_t> (*this, "EVENT_KEY_UP",
                                  static_cast<uint32_t> (event_kind::key_up));

    // Low-level interop functions (addInterop)
    register_interop_functions (*this);
  }

  virtual ::das::ModuleAotType
  aotRequire (::das::TextWriter &tw) const override
  {
    tw << "#include \"wsl/das/wsl_api_module.hpp\"\n";
    return ::das::ModuleAotType::cpp;
  }
};

} // anonymous namespace

static ::das::Module *g_weasel_api = nullptr;

void
register_wsl_api_module (::das::ModuleGroup &module_group)
{
  g_weasel_api = new Module_WeaselApi ();
  module_group.addModule (g_weasel_api);
}

::das::Module *
get_wsl_api_module ()
{
  return g_weasel_api;
}

::das::Module *
create_worker_weasel_api_module ()
{
  return new Module_WeaselApi ();
}

void
wsl_api_set_active_registry (entt::registry *registry)
{
  g_registry = registry;
}

void
wsl_api_set_current_event (const engine_event *ev)
{
  g_current_event = ev;
}

void
wsl_api_set_delta_time (double dt)
{
  g_delta_time = dt;
}

float
wsl_get_delta_time ()
{
  return static_cast<float> (g_delta_time);
}

void
wsl_log_info (const char *msg)
{
  wsl::log::sys ()->info ("{}", msg ? msg : "");
}

void
wsl_log_debug (const char *msg)
{
  wsl::log::sys ()->debug ("{}", msg ? msg : "");
}

void
wsl_log_warn (const char *msg)
{
  wsl::log::sys ()->warn ("{}", msg ? msg : "");
}

void
wsl_log_error (const char *msg)
{
  wsl::log::sys ()->error ("{}", msg ? msg : "");
}

} // namespace wsl::das
