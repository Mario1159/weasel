#include "scene.hpp"
#include "comp/singl/ui_manager.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "rsc/resource_ref.hpp"
#include "sys/system.hpp"
#include "wsl/log/log.hpp"
#include <SDL3/SDL_events.h>
#include <algorithm>
#include <entt/core/fwd.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace entt::literals;

#include "../comp/hierarchy.hpp"
#include "../comp/prefab_instance.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "../comp/singl/runtime_context.hpp"

namespace wsl
{

namespace rsc
{

entt::registry &
scene::get_registry ()
{
  return m_registry;
}

const std::string &
scene::get_name () const
{
  return m_name;
}

void
scene::set_name (std::string name)
{
  m_name = std::move (name);
}

sys::ecs_system &
scene::add_system_instance (std::unique_ptr<sys::ecs_system> sys,
                            bool initialize_if_running)
{
  entt::id_type new_type_id = sys->get_type_id ();
  for (const auto &existing : systems) {
    if (existing && existing->get_type_id () == new_type_id) {
      return *existing;
    }
  }

  sys::ecs_system *system = sys.get ();
  systems.emplace_back (std::move (sys));

  on_system_added (*system);

  if (initialize_if_running && m_initialized) {
    system->refresh_activation (&m_registry, is_playing ());
  }

  return *system;
}

void
scene::remove_system (sys::ecs_system *system)
{
  auto it = std::find_if (systems.begin (), systems.end (),
                          [system] (const std::unique_ptr<sys::ecs_system> &s) {
                            return s.get () == system;
                          });

  if (it != systems.end ()) {
    if (m_initialized) {
      (*it)->shutdown (&m_registry);
    }
    systems.erase (it);
  }
}

void
scene::on_system_added (sys::ecs_system &system)
{
  if (m_runtime_ctx == nullptr) {
    return;
  }

  m_runtime_ctx->signal_hub().clear_system_declarations (system.get_type_id ());
  system.register_signals (m_runtime_ctx->signal_hub());
  system.register_event_handlers (m_runtime_ctx->signal_hub());
  system.register_iterations (m_runtime_ctx->signal_hub());
}

void
scene::init ()
{
  m_initialized = true;
  refresh_system_states ();
  wsl::log::sys ()->debug ("Scene '{}' initialized ({} systems)", m_name,
                           systems.size ());
}

void
scene::pause ()
{
  m_running = false;
  if (m_initialized) {
    refresh_system_states ();
  }
}

void
scene::resume ()
{
  if (m_initialized && !m_running) {
    m_running = true;
    refresh_system_states ();
  }
}

void
scene::update (double dt)
{
  if (!m_initialized) {
    return;
  }

  for (std::unique_ptr<sys::ecs_system> &sys : systems) {
    if (!sys) {
      continue;
    }

    sys->update (&m_registry, dt);
  }
}

void
scene::handle_events (const SDL_Event &e)
{
  if (!m_initialized) {
    return;
  }

  for (std::unique_ptr<sys::ecs_system> &sys : systems) {
    if (!sys) {
      continue;
    }

    sys->event_handler (&m_registry, e);
  }
}

void
scene::shutdown_systems ()
{
  for (std::unique_ptr<sys::ecs_system> &sys : systems) {
    if (!sys) {
      continue;
    }

    sys->shutdown (&m_registry);
  }
}

bool
scene::is_playing () const
{
  return (m_runtime_ctx != nullptr) ? m_runtime_ctx->is_running() : m_running;
}

void
scene::refresh_system_states ()
{
  if (!m_initialized) {
    return;
  }

  const bool playing = is_playing ();
  m_running = playing;

  for (std::unique_ptr<sys::ecs_system> &sys : systems) {
    if (!sys) {
      continue;
    }

    sys->refresh_activation (&m_registry, playing);
  }
}

void
scene::stop_and_clear ()
{
  m_registry.clear ();
  shutdown_systems ();
  m_initialized = false;
  m_running = false;
  systems.clear ();
  reset_scene_context ();
}

void
scene::clear ()
{
  m_registry.clear ();
  shutdown_systems ();
  systems.clear ();
  reset_scene_context ();
}

void
scene::clear_registry ()
{
  m_registry.clear ();
  m_entity_names.clear ();
  reset_scene_context ();

  if (m_initialized) {
    refresh_system_states ();
  }
}

void
scene::add_resource (io::resource_type type, entt::id_type id)
{
  for (const io::resource_ref &r : m_load_list) {
    if (r.type == type && r.id == id) {
      wsl::log::rsc ()->debug ("Resource type={} id={} already in load list",
                               (int)type, id);
      return;
    }
  }

  wsl::log::rsc ()->trace ("Adding resource (type={}) id={} to load list",
                           (int)type, id);
  m_load_list.push_back ({ type, id });

  // If this scene is the active one, load immediately
  if (m_runtime_ctx != nullptr) {
    scene const *active_scene = m_runtime_ctx->scene_manager().get_active ();
    wsl::log::rsc ()->trace ("Active scene={} this={}", (void *)active_scene,
                             (void *)this);
    if (active_scene == this) {
      wsl::log::rsc ()->trace ("Calling resource_manager.load");
      rsc::resource_manager &res_mgr = m_runtime_ctx->resource_manager();
      res_mgr.load ({ type, id });
    }
  }
}

void
scene::remove_resource (io::resource_type type, entt::id_type id)
{
  m_load_list.erase (std::remove_if (m_load_list.begin (), m_load_list.end (),
                                     [&] (const io::resource_ref &r) {
                                       return r.type == type && r.id == id;
                                     }),
                     m_load_list.end ());
}

bool
scene::has_resource (io::resource_type type, entt::id_type id) const
{
  for (const io::resource_ref &r : m_load_list) {
    if (r.type == type && r.id == id) {
      return true;
    }
  }
  return false;
}

const std::vector<io::resource_ref> &
scene::get_load_list () const
{
  return m_load_list;
}

void
scene::set_entity_name (entt::entity e, std::string name)
{
  m_entity_names[e] = std::move (name);
}

const std::string &
scene::get_entity_name ([[maybe_unused]] entt::entity e) const
{
  static const std::string unnamed = "<unnamed>";
  if (std::unordered_map<entt::entity, std::string>::const_iterator const it
      = m_entity_names.find (e);
      it != m_entity_names.end ()) {
    return it->second;
  }
  return unnamed;
}

void
scene::remove_entity_name (entt::entity e)
{
  m_entity_names.erase (e);
}

std::unordered_map<entt::entity, std::string> &
scene::get_entity_names ()
{
  return m_entity_names;
}

const std::unordered_map<entt::entity, std::string> &
scene::get_entity_names () const
{
  return m_entity_names;
}

scene::scene (comp::singl::runtime_context *runtime_ctx,
              comp::singl::editor_context *editor_ctx, const std::string &name)
    : m_name (name), m_runtime_ctx (runtime_ctx), m_editor_ctx (editor_ctx)
{
  ensure_context_bindings ();
}

void
scene::ensure_context_bindings ()
{
  auto &ctx = m_registry.ctx ();

  if (m_runtime_ctx != nullptr) {
    if (ctx.contains<comp::singl::runtime_context *> ()) {
      ctx.erase<comp::singl::runtime_context *> ();
    }
    ctx.emplace<comp::singl::runtime_context *> (m_runtime_ctx);
  }

  if (m_editor_ctx != nullptr) {
    if (ctx.contains<comp::singl::editor_context *> ()) {
      ctx.erase<comp::singl::editor_context *> ();
    }
    ctx.emplace<comp::singl::editor_context *> (m_editor_ctx);
  }

  if (m_runtime_ctx != nullptr) {
    if (ctx.contains<scene_manager *> ()) {
      ctx.erase<scene_manager *> ();
    }
    ctx.emplace<scene_manager *> (&m_runtime_ctx->scene_manager());

    if (ctx.contains<resource_manager_view *> ()) {
      ctx.erase<resource_manager_view *> ();
    }
    ctx.emplace<resource_manager_view *> (
        &m_runtime_ctx->resource_manager_view());

    if (ctx.contains<comp::singl::ui_manager *> ()) {
      ctx.erase<comp::singl::ui_manager *> ();
    }
    ctx.emplace<comp::singl::ui_manager *> (&m_runtime_ctx->ui_manager());

    m_runtime_ctx->singleton_registry().apply_core_singletons (m_registry);
  }
}

void
scene::reset_scene_context ()
{
  if (m_runtime_ctx != nullptr) {
    m_runtime_ctx->singleton_registry().reset_scene_registry (m_registry);
  }

  ensure_context_bindings ();
}

std::vector<sys::ecs_system *>
scene::get_systems ()
{
  std::vector<sys::ecs_system *> out;
  out.reserve (systems.size ());

  for (std::unique_ptr<sys::ecs_system> const &sys : systems) {
    out.push_back (sys.get ());
  }

  return out;
}

entt::entity
scene::copy_entity (scene &src_scene, entt::entity src_entity,
                    entt::entity dst_parent, bool is_instantiating_prefab,
                    scene_id prefab_id)
{
  entt::registry &src_reg = src_scene.get_registry ();
  entt::registry &dst_reg = get_registry ();

  entt::entity const dst_entity = dst_reg.create ();
  set_entity_name (dst_entity, src_scene.get_entity_name (src_entity));

  // Copy all components by iterating over storages
  for (auto [id, storage] : src_reg.storage ()) {
    if (storage.contains (src_entity)) {
      // Handle hierarchy separately
      if (id == entt::type_hash<comp::hierarchy>::value ()) {
        continue;
      }

      if (m_runtime_ctx->component_registry().copy_world_component (
              src_reg, src_entity, dst_reg, dst_entity, id)) {
        continue;
      }

      // Fallback to meta-based generic copy
      entt::meta_type const type = entt::resolve (id);
      if (type) {
        auto comp_any = type.func ("get"_hs).invoke (src_reg, src_entity);
        if (comp_any) {
          type.func ("emplace_or_replace"_hs)
              .invoke (dst_reg, dst_entity, comp_any);
        }
      }
    }
  }

  if (is_instantiating_prefab) {
    dst_reg.emplace_or_replace<comp::prefab_instance> (dst_entity, prefab_id,
                                                       src_entity);
  }

  if (src_reg.all_of<comp::hierarchy> (src_entity)) {
    auto &src_h = src_reg.get<comp::hierarchy> (src_entity);
    auto &dst_h = dst_reg.emplace_or_replace<comp::hierarchy> (dst_entity);
    dst_h.parent = dst_parent;

    entt::entity src_child = src_h.first;
    entt::entity prev_dst_child = entt::null;
    while (src_child != entt::null) {
      entt::entity const dst_child = copy_entity (
          src_scene, src_child, dst_entity, is_instantiating_prefab, prefab_id);
      if (prev_dst_child == entt::null) {
        dst_h.first = dst_child;
      } else {
        dst_reg.get<comp::hierarchy> (prev_dst_child).next = dst_child;
      }
      prev_dst_child = dst_child;
      src_child = src_reg.get<comp::hierarchy> (src_child).next;
    }
  }

  return dst_entity;
}

} // namespace rsc

} // namespace wsl
