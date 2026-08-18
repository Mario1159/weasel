#include "wsl_api_module.hpp"
#include "wsl_api_component_accessors.hpp"
#include "das_interop.hpp"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_handle.h"
#include "daScript/ast/ast_interop.h"
#include "daScript/daScriptModule.h"
#include "daScript/misc/arraytype.h"
#include "daScript/simulate/aot.h"

#include "wsl/comp/transform.hpp"
#include "wsl/comp/camera.hpp"
#include "wsl/comp/hierarchy.hpp"
#include "wsl/comp/world_transform.hpp"
#include "wsl/comp/component_meta.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/rigid_body.hpp"
#include "wsl/comp/character_body.hpp"
#include "wsl/comp/transform_2d.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/sprite_2d.hpp"
#include "wsl/comp/model_instance_3d.hpp"
#include "wsl/comp/point_light.hpp"
#include "wsl/comp/directional_light.hpp"
#include "wsl/comp/spot_light.hpp"
#include "wsl/comp/audio.hpp"
#include "wsl/event.hpp"
#include "wsl/log/log.hpp"
#include "wsl/ray.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/resource_manager.hpp"
#include "wsl/reg/component_registry.hpp"
#include "wsl/reg/sig/signal_hub.hpp"
#include "wsl/phys/physics_engine.hpp"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_keyboard.h>
#include <entt/core/hashed_string.hpp>
#include <chrono>
#include <cstring>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/EActivation.h>

MAKE_TYPE_FACTORY (Transform, wsl::das::TransformProxy)
MAKE_TYPE_FACTORY (Transform2D, wsl::das::Transform2DProxy)
MAKE_TYPE_FACTORY (Camera2D, wsl::das::Camera2DProxy)
MAKE_TYPE_FACTORY (Sprite2D, wsl::das::Sprite2DProxy)
MAKE_TYPE_FACTORY (PointLight, wsl::das::PointLightProxy)
MAKE_TYPE_FACTORY (DirectionalLight, wsl::das::DirectionalLightProxy)
MAKE_TYPE_FACTORY (SpotLight, wsl::das::SpotLightProxy)

namespace wsl::das
{

namespace
{

thread_local entt::registry *g_registry = nullptr;
const engine_event *g_current_event = nullptr;

entt::registry *
get_registry ()
{
  return g_registry;
}

template <typename T>
T &
fallback_value ()
{
  static thread_local T value{};
  return value;
}

} // anonymous namespace

// ── Component accessor proxy helpers ──

bool
TransformProxy::valid () const
{
  return comp != nullptr;
}

void
TransformProxy::bind (comp::transform *component)
{
  comp = component;
}

::das::float3 &
TransformProxy::position ()
{
  return comp ? *reinterpret_cast<::das::float3 *> (&comp->position)
              : fallback_value<::das::float3> ();
}

::das::float4 &
TransformProxy::rotation ()
{
  return comp ? *reinterpret_cast<::das::float4 *> (&comp->rotation)
              : fallback_value<::das::float4> ();
}

::das::float3 &
TransformProxy::scale ()
{
  return comp ? *reinterpret_cast<::das::float3 *> (&comp->scale)
              : fallback_value<::das::float3> ();
}

bool
Transform2DProxy::valid () const
{
  return comp != nullptr;
}

void
Transform2DProxy::bind (comp::transform_2d *component)
{
  comp = component;
}

::das::float2 &
Transform2DProxy::position ()
{
  return comp ? *reinterpret_cast<::das::float2 *> (&comp->position)
              : fallback_value<::das::float2> ();
}

::das::float2 &
Transform2DProxy::scale ()
{
  return comp ? *reinterpret_cast<::das::float2 *> (&comp->scale)
              : fallback_value<::das::float2> ();
}

float &
Transform2DProxy::rotation ()
{
  return comp ? comp->rotation : fallback_value<float> ();
}

bool
Camera2DProxy::valid () const
{
  return comp != nullptr;
}

void
Camera2DProxy::bind (comp::camera_2d *component)
{
  comp = component;
}

float &
Camera2DProxy::zoom ()
{
  return comp ? comp->zoom : fallback_value<float> ();
}

bool
Sprite2DProxy::valid () const
{
  return comp != nullptr;
}

void
Sprite2DProxy::bind (comp::sprite_2d *component)
{
  comp = component;
}

::das::float4 &
Sprite2DProxy::color ()
{
  return comp ? *reinterpret_cast<::das::float4 *> (&comp->color)
              : fallback_value<::das::float4> ();
}

::das::float2 &
Sprite2DProxy::size ()
{
  return comp ? *reinterpret_cast<::das::float2 *> (&comp->size)
              : fallback_value<::das::float2> ();
}

bool
PointLightProxy::valid () const
{
  return comp != nullptr;
}

void
PointLightProxy::bind (comp::point_light *component)
{
  comp = component;
}

::das::float3 &
PointLightProxy::color ()
{
  return comp ? *reinterpret_cast<::das::float3 *> (&comp->color)
              : fallback_value<::das::float3> ();
}

float &
PointLightProxy::intensity ()
{
  return comp ? comp->intensity : fallback_value<float> ();
}

bool
DirectionalLightProxy::valid () const
{
  return comp != nullptr;
}

void
DirectionalLightProxy::bind (comp::directional_light *component)
{
  comp = component;
}

::das::float3 &
DirectionalLightProxy::color ()
{
  return comp ? *reinterpret_cast<::das::float3 *> (&comp->color)
              : fallback_value<::das::float3> ();
}

float &
DirectionalLightProxy::intensity ()
{
  return comp ? comp->intensity : fallback_value<float> ();
}

bool
SpotLightProxy::valid () const
{
  return comp != nullptr;
}

void
SpotLightProxy::bind (comp::spot_light *component)
{
  comp = component;
}

::das::float3 &
SpotLightProxy::color ()
{
  return comp ? *reinterpret_cast<::das::float3 *> (&comp->color)
              : fallback_value<::das::float3> ();
}

float &
SpotLightProxy::intensity ()
{
  return comp ? comp->intensity : fallback_value<float> ();
}

namespace
{

struct TransformProxyAnnotation
    : ::das::ManagedStructureAnnotation<TransformProxy, false, false>
{
  explicit TransformProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<TransformProxy, false, false> (
            "Transform", lib, "wsl::das::TransformProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (position)> ("position", "position");
    addProperty<DAS_BIND_MANAGED_PROP (rotation)> ("rotation", "rotation");
    addProperty<DAS_BIND_MANAGED_PROP (scale)> ("scale", "scale");
  }
};

struct Transform2DProxyAnnotation
    : ::das::ManagedStructureAnnotation<Transform2DProxy, false, false>
{
  explicit Transform2DProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<Transform2DProxy, false, false> (
            "Transform2D", lib, "wsl::das::Transform2DProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (position)> ("position", "position");
    addProperty<DAS_BIND_MANAGED_PROP (scale)> ("scale", "scale");
    addProperty<DAS_BIND_MANAGED_PROP (rotation)> ("rotation", "rotation");
  }
};

struct Camera2DProxyAnnotation
    : ::das::ManagedStructureAnnotation<Camera2DProxy, false, false>
{
  explicit Camera2DProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<Camera2DProxy, false, false> (
            "Camera2D", lib, "wsl::das::Camera2DProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (zoom)> ("zoom", "zoom");
  }
};

struct Sprite2DProxyAnnotation
    : ::das::ManagedStructureAnnotation<Sprite2DProxy, false, false>
{
  explicit Sprite2DProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<Sprite2DProxy, false, false> (
            "Sprite2D", lib, "wsl::das::Sprite2DProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (color)> ("color", "color");
    addProperty<DAS_BIND_MANAGED_PROP (size)> ("size", "size");
  }
};

struct PointLightProxyAnnotation
    : ::das::ManagedStructureAnnotation<PointLightProxy, false, false>
{
  explicit PointLightProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<PointLightProxy, false, false> (
            "PointLight", lib, "wsl::das::PointLightProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (color)> ("color", "color");
    addProperty<DAS_BIND_MANAGED_PROP (intensity)> ("intensity", "intensity");
  }
};

struct DirectionalLightProxyAnnotation
    : ::das::ManagedStructureAnnotation<DirectionalLightProxy, false, false>
{
  explicit DirectionalLightProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<DirectionalLightProxy, false, false> (
            "DirectionalLight", lib, "wsl::das::DirectionalLightProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (color)> ("color", "color");
    addProperty<DAS_BIND_MANAGED_PROP (intensity)> ("intensity", "intensity");
  }
};

struct SpotLightProxyAnnotation
    : ::das::ManagedStructureAnnotation<SpotLightProxy, false, false>
{
  explicit SpotLightProxyAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<SpotLightProxy, false, false> (
            "SpotLight", lib, "wsl::das::SpotLightProxy")
  {
    addProperty<DAS_BIND_MANAGED_PROP (color)> ("color", "color");
    addProperty<DAS_BIND_MANAGED_PROP (intensity)> ("intensity", "intensity");
  }
};

} // anonymous namespace

void
register_component_accessors (::das::Module &mod, ::das::ModuleLibrary &lib)
{
  mod.addAnnotation (new TransformProxyAnnotation (lib));
  mod.addAnnotation (new Transform2DProxyAnnotation (lib));
  mod.addAnnotation (new Camera2DProxyAnnotation (lib));
  mod.addAnnotation (new Sprite2DProxyAnnotation (lib));
  mod.addAnnotation (new PointLightProxyAnnotation (lib));
  mod.addAnnotation (new DirectionalLightProxyAnnotation (lib));
  mod.addAnnotation (new SpotLightProxyAnnotation (lib));

  addExtern<DAS_BIND_FUN (get_transform_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_transform_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_transform_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_transform_2d_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_transform_2d_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_transform_2d_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_camera_2d_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_camera_2d_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_camera_2d_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_sprite_2d_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_sprite_2d_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_sprite_2d_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_point_light_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_point_light_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_point_light_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_directional_light_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_directional_light_accessor",
      ::das::SideEffects::accessExternal,
      "wsl::das::get_directional_light_accessor")
      ->args ({ "entity", "at" });

  addExtern<DAS_BIND_FUN (get_spot_light_accessor),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      mod, lib, "get_spot_light_accessor", ::das::SideEffects::accessExternal,
      "wsl::das::get_spot_light_accessor")
      ->args ({ "entity", "at" });
}

TransformProxy
get_transform_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  TransformProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::transform> (proxy.entity) : nullptr);
  return proxy;
}

Transform2DProxy
get_transform_2d_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  Transform2DProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::transform_2d> (proxy.entity) : nullptr);
  return proxy;
}

Camera2DProxy
get_camera_2d_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  Camera2DProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::camera_2d> (proxy.entity) : nullptr);
  return proxy;
}

Sprite2DProxy
get_sprite_2d_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  Sprite2DProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::sprite_2d> (proxy.entity) : nullptr);
  return proxy;
}

PointLightProxy
get_point_light_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  PointLightProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::point_light> (proxy.entity) : nullptr);
  return proxy;
}

DirectionalLightProxy
get_directional_light_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  DirectionalLightProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::directional_light> (proxy.entity)
                  : nullptr);
  return proxy;
}

SpotLightProxy
get_spot_light_accessor (uint32_t entity, ::das::LineInfoArg *at)
{
  (void)at;
  SpotLightProxy proxy{};
  proxy.entity = static_cast<entt::entity> (entity);
  auto *reg = get_registry ();
  proxy.bind (reg ? reg->try_get<comp::spot_light> (proxy.entity) : nullptr);
  return proxy;
}

namespace
{

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
      return runtime_ctx->component_registry ().das_component_contains (
          *reg, type_id, e);
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
    return comp_reg.das_component_add (*reg, type_id, e);
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
    return comp_reg.das_component_remove (*reg, type_id, e);
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

void
wsl_set_entity_name (uint32_t entity, const char *name)
{
  auto *reg = get_registry ();
  if (!reg || !name) {
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

  auto e = static_cast<entt::entity> (entity);
  if (reg->valid (e)) {
    scene->set_entity_name (e, name);
  }
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

uint32_t
wsl_type_id_transform_2d ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::transform_2d>::value ());
}

uint32_t
wsl_type_id_camera_2d ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::camera_2d>::value ());
}

uint32_t
wsl_type_id_sprite_2d ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::sprite_2d>::value ());
}

uint32_t
wsl_type_id_point_light ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::point_light>::value ());
}

uint32_t
wsl_type_id_directional_light ()
{
  return static_cast<uint32_t> (
      entt::type_hash<comp::directional_light>::value ());
}

uint32_t
wsl_type_id_spot_light ()
{
  return static_cast<uint32_t> (entt::type_hash<comp::spot_light>::value ());
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

// ── Generic entity iteration (DECS-style `query` primitive) ──

void
each_entity_id_with (const ::das::TArray<uint32_t> &type_ids,
                     const ::das::TBlock<void, uint32_t> &blk,
                     ::das::Context *context, ::das::LineInfoArg *at)
{
  auto *reg = get_registry ();
  if (!reg || type_ids.size == 0) {
    return;
  }
  auto *runtime_ctx = reg->ctx ().contains<comp::singl::runtime_context *> ()
                          ? reg->ctx ().get<comp::singl::runtime_context *> ()
                          : nullptr;

  struct candidate
  {
    const entt::sparse_set *native = nullptr;
    const ::wsl::reg::das_component_storage::pool *das = nullptr;
    std::size_t size = 0;
  };

  std::vector<candidate> candidates;
  candidates.reserve (type_ids.size);
  auto *component_registry
      = runtime_ctx != nullptr ? &runtime_ctx->component_registry () : nullptr;

  for (uint32_t i = 0; i < type_ids.size; ++i) {
    const entt::id_type type_id = entt::id_type{ type_ids[i] };
    const auto *desc = component_registry != nullptr
                           ? component_registry->find_world_component (type_id)
                           : nullptr;
    candidate term{};

    if (desc != nullptr && desc->is_das_component) {
      if (component_registry == nullptr) {
        return;
      }
      term.das = component_registry->das_component_pool (*reg, type_id);
      if (term.das == nullptr) {
        return;
      }
      term.size = term.das->entries.size ();
    } else {
      term.native = reg->storage (type_id);
      if (term.native == nullptr) {
        return;
      }
      term.size = term.native->size ();
    }

    candidates.push_back (term);
  }

  std::size_t candidate_index = 0;
  for (std::size_t i = 1; i < candidates.size (); ++i) {
    if (candidates[i].size < candidates[candidate_index].size) {
      candidate_index = i;
    }
  }

  auto invoke_if_matching = [&] (entt::entity entity) {
    for (uint32_t i = 0; i < type_ids.size; ++i) {
      if (!wsl_has_component (type_ids[i], static_cast<uint32_t> (entity))) {
        return;
      }
    }
    ::das::das_invoke<void>::invoke<uint32_t> (context, at, blk,
                                               static_cast<uint32_t> (entity));
  };

  const candidate &selected = candidates[candidate_index];
  if (selected.native != nullptr) {
    for (entt::entity entity : *selected.native) {
      invoke_if_matching (entity);
    }
  } else {
    for (const auto &[entity, block] : selected.das->entries) {
      (void)block;
      invoke_if_matching (entity);
    }
  }
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

// ── Generic component type lookup ──

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

void *
wsl_get_component_data (uint32_t entity, uint32_t type_id)
{
  auto *reg = get_registry ();
  if (!reg || !reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return nullptr;
  }

  auto *runtime_ctx = reg->ctx ().get<comp::singl::runtime_context *> ();
  if (!runtime_ctx) {
    return nullptr;
  }

  const auto *info = runtime_ctx->component_registry ().find_world_component (
      static_cast<entt::id_type> (type_id));
  if (!info || !info->is_das_component) {
    return nullptr;
  }

  return runtime_ctx->component_registry ().das_component_data (
      *reg, static_cast<entt::id_type> (type_id),
      static_cast<entt::entity> (entity));
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

// ── Runtime context / subsystem accessors ──

comp::singl::runtime_context *
try_get_runtime_context ()
{
  auto *reg = get_registry ();
  if (!reg) {
    return nullptr;
  }
  if (!reg->ctx ().contains<comp::singl::runtime_context *> ()) {
    return nullptr;
  }
  return reg->ctx ().get<comp::singl::runtime_context *> ();
}

phys::engine *
try_get_physics_engine ()
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return nullptr;
  }
  auto *pm = rc->get_active_physics_manager ();
  if (!pm) {
    return nullptr;
  }
  return pm->try_engine ();
}

// ── Time ──

float
wsl_get_elapsed_time ()
{
  static const auto start = std::chrono::steady_clock::now ();
  return static_cast<float> (
      std::chrono::duration<double> (std::chrono::steady_clock::now () - start)
          .count ());
}

float
wsl_get_time ()
{
  return static_cast<float> (wsl_get_elapsed_time ());
}

// ── Physics: rigid body ──

void
wsl_apply_impulse (uint32_t entity, float x, float y, float z)
{
  auto *eng = try_get_physics_engine ();
  auto *reg = get_registry ();
  if (!eng || !reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::rigid_body> (e)) {
    return;
  }
  auto &rb = reg->get<comp::rigid_body> (e);
  if (rb.body_id.IsInvalid ()) {
    return;
  }
  eng->get_body_interface ().AddImpulse (rb.body_id, JPH::Vec3 (x, y, z));
  eng->get_body_interface ().ActivateBody (rb.body_id);
}

void
wsl_apply_force (uint32_t entity, float x, float y, float z)
{
  auto *eng = try_get_physics_engine ();
  auto *reg = get_registry ();
  if (!eng || !reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::rigid_body> (e)) {
    return;
  }
  auto &rb = reg->get<comp::rigid_body> (e);
  if (rb.body_id.IsInvalid ()) {
    return;
  }
  eng->get_body_interface ().AddForce (rb.body_id, JPH::Vec3 (x, y, z));
  eng->get_body_interface ().ActivateBody (rb.body_id);
}

// ── Audio ──

void
wsl_audio_play (uint32_t entity)
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return;
  }
  ::wsl::reg::sig::emit (
      rc->signal_hub (),
      comp::audio::play{ static_cast<entt::entity> (entity) });
}

void
wsl_audio_stop (uint32_t entity)
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return;
  }
  ::wsl::reg::sig::emit (
      rc->signal_hub (),
      comp::audio::stop{ static_cast<entt::entity> (entity) });
}

void
wsl_audio_pause (uint32_t entity)
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return;
  }
  ::wsl::reg::sig::emit (
      rc->signal_hub (),
      comp::audio::pause{ static_cast<entt::entity> (entity) });
}

void
wsl_audio_resume (uint32_t entity)
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return;
  }
  ::wsl::reg::sig::emit (
      rc->signal_hub (),
      comp::audio::resume{ static_cast<entt::entity> (entity) });
}

void
wsl_audio_set_volume (uint32_t entity, float volume)
{
  auto *rc = try_get_runtime_context ();
  if (!rc) {
    return;
  }
  ::wsl::reg::sig::emit (
      rc->signal_hub (),
      comp::audio::set_volume{ static_cast<entt::entity> (entity), volume });
}

// ── Model instance ──

void
wsl_set_model (uint32_t entity, const char *path)
{
  auto *rc = try_get_runtime_context ();
  auto *reg = get_registry ();
  if (!rc || !reg || !path) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::model_instance_3d> (e)) {
    return;
  }
  reg->get<comp::model_instance_3d> (e).id
      = rc->resource_manager ().register_model (std::string (path));
}

void
wsl_set_model_material_override (uint32_t entity, const char *path)
{
  auto *rc = try_get_runtime_context ();
  auto *reg = get_registry ();
  if (!rc || !reg || !path) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::model_instance_3d> (e)) {
    return;
  }
  reg->get<comp::model_instance_3d> (e).material_override
      = rc->resource_manager ().register_material (std::string (path));
}

void
wsl_set_model_visibility_range (uint32_t entity, float range)
{
  auto *reg = get_registry ();
  if (!reg) {
    return;
  }
  auto e = static_cast<entt::entity> (entity);
  if (!reg->valid (e) || !reg->all_of<comp::model_instance_3d> (e)) {
    return;
  }
  reg->get<comp::model_instance_3d> (e).visibility_range = range;
}

class Module_WeaselApi : public ::das::Module
{
public:
  Module_WeaselApi () : Module ("weasel_api")
  {
    ::das::ModuleLibrary lib (this);
    lib.addBuiltInModule ();
    register_component_accessors (*this, lib);

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

    // Scene operations
    addExtern<DAS_BIND_FUN (wsl_find_entity_by_name)> (
        *this, lib, "find_entity_by_name", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_find_entity_by_name")
        ->arg ("name");

    addExtern<DAS_BIND_FUN (wsl_set_entity_name)> (
        *this, lib, "set_entity_name", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_entity_name")
        ->args ({ "entity", "name" });

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

    addExtern<DAS_BIND_FUN (wsl_type_id_transform_2d)> (
        *this, lib, "TYPE_TRANSFORM_2D", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_transform_2d");

    addExtern<DAS_BIND_FUN (wsl_type_id_camera_2d)> (
        *this, lib, "TYPE_CAMERA_2D", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_camera_2d");

    addExtern<DAS_BIND_FUN (wsl_type_id_sprite_2d)> (
        *this, lib, "TYPE_SPRITE_2D", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_sprite_2d");

    addExtern<DAS_BIND_FUN (wsl_type_id_point_light)> (
        *this, lib, "TYPE_POINT_LIGHT", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_point_light");

    addExtern<DAS_BIND_FUN (wsl_type_id_directional_light)> (
        *this, lib, "TYPE_DIRECTIONAL_LIGHT", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_directional_light");

    addExtern<DAS_BIND_FUN (wsl_type_id_spot_light)> (
        *this, lib, "TYPE_SPOT_LIGHT", ::das::SideEffects::none,
        "wsl::das::wsl_type_id_spot_light");

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

    // Component type lookup
    addExtern<DAS_BIND_FUN (wsl_get_component_type_id)> (
        *this, lib, "get_component_type_id", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_type_id")
        ->arg ("display_name");

    // Generic query iteration primitive (backs the `query` macro)
    addExtern<DAS_BIND_FUN (each_entity_id_with)> (
        *this, lib, "each_entity_id_with", ::das::SideEffects::invoke,
        "wsl::das::each_entity_id_with")
        ->args ({ "type_ids", "blk", "context", "at" });

    // Generic component type lookup and direct payload access
    addExtern<DAS_BIND_FUN (wsl_get_component_type_id_by_name)> (
        *this, lib, "_get_component_type_id_by_name",
        ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_type_id_by_name")
        ->arg ("type_name");

    addExtern<DAS_BIND_FUN (wsl_get_component_data)> (
        *this, lib, "_get_component_data", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_component_data")
        ->args ({ "entity", "type_id" });

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
    ::das::addConstant<int32_t> (*this, "SDL_SCANCODE_ESCAPE", 41);
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

    // ── Time ──
    addExtern<DAS_BIND_FUN (wsl_get_time)> (*this, lib, "get_time",
                                            ::das::SideEffects::accessExternal,
                                            "wsl::das::wsl_get_time");
    addExtern<DAS_BIND_FUN (wsl_get_elapsed_time)> (
        *this, lib, "get_elapsed_time", ::das::SideEffects::accessExternal,
        "wsl::das::wsl_get_elapsed_time");

    // ── Physics: rigid body ──

    addExtern<DAS_BIND_FUN (wsl_apply_impulse)> (
        *this, lib, "apply_impulse", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_apply_impulse")
        ->args ({ "entity", "x", "y", "z" });
    addExtern<DAS_BIND_FUN (wsl_apply_force)> (
        *this, lib, "apply_force", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_apply_force")
        ->args ({ "entity", "x", "y", "z" });

    // ── Audio ──
    addExtern<DAS_BIND_FUN (wsl_audio_play)> (
        *this, lib, "audio_play", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_audio_play")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_audio_stop)> (
        *this, lib, "audio_stop", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_audio_stop")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_audio_pause)> (
        *this, lib, "audio_pause", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_audio_pause")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_audio_resume)> (
        *this, lib, "audio_resume", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_audio_resume")
        ->arg ("entity");
    addExtern<DAS_BIND_FUN (wsl_audio_set_volume)> (
        *this, lib, "audio_set_volume", ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_audio_set_volume")
        ->args ({ "entity", "volume" });

    // ── Model instance ──
    addExtern<DAS_BIND_FUN (wsl_set_model)> (*this, lib, "set_model",
                                             ::das::SideEffects::modifyExternal,
                                             "wsl::das::wsl_set_model")
        ->args ({ "entity", "path" });
    addExtern<DAS_BIND_FUN (wsl_set_model_material_override)> (
        *this, lib, "set_model_material_override",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_model_material_override")
        ->args ({ "entity", "path" });
    addExtern<DAS_BIND_FUN (wsl_set_model_visibility_range)> (
        *this, lib, "set_model_visibility_range",
        ::das::SideEffects::modifyExternal,
        "wsl::das::wsl_set_model_visibility_range")
        ->args ({ "entity", "range" });

    // Low-level interop functions (addInterop)
    register_interop_functions (*this);
  }

  virtual ::das::ModuleAotType
  aotRequire (::das::TextWriter &tw) const override
  {
    tw << "#include \"wsl/das/wsl_api_module.hpp\"\n";
    tw << "#include \"wsl/das/wsl_api_component_accessors.hpp\"\n";
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
