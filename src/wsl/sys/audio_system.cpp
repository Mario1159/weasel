#include "audio_system.hpp"
#include "../comp/audio.hpp"
#include "reg/sig/signal_hub.hpp"
#include "../comp/singl/runtime_context.hpp"
#include "sys/system.hpp"
#include <SDL3_mixer/SDL_mixer.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <string>

namespace wsl
{

namespace sys
{

audio_system::audio_system (const std::string &name) : ecs_system_t (name) {}

audio_system::~audio_system () { stop_all_loops (); }

void
audio_system::register_signals (reg::sig::signal_hub &hub)
{
  hub.declare_signal<comp::audio::play, audio_system, comp::audio> (
      +[] (const void *sig) -> entt::entity {
        return static_cast<const comp::audio::play *> (sig)->entity;
      });
  hub.declare_signal<comp::audio::stop, audio_system, comp::audio> (
      +[] (const void *sig) -> entt::entity {
        return static_cast<const comp::audio::stop *> (sig)->entity;
      });
  hub.declare_signal<comp::audio::pause, audio_system, comp::audio> (
      +[] (const void *sig) -> entt::entity {
        return static_cast<const comp::audio::pause *> (sig)->entity;
      });
  hub.declare_signal<comp::audio::resume, audio_system, comp::audio> (
      +[] (const void *sig) -> entt::entity {
        return static_cast<const comp::audio::resume *> (sig)->entity;
      });
  hub.declare_signal<comp::audio::set_volume, audio_system, comp::audio> (
      +[] (const void *sig) -> entt::entity {
        return static_cast<const comp::audio::set_volume *> (sig)->entity;
      });
}

void
audio_system::register_event_handlers (reg::sig::signal_hub &hub)
{
  hub.declare_connectable_handler<comp::audio::play, audio_system,
                                  comp::audio> (
      "on_play", +[] (sys::ecs_system &sys, entt::registry &reg,
                      entt::entity ent, const void *sig) {
        static_cast<audio_system &> (sys).on_play (
            reg, ent, *static_cast<const comp::audio::play *> (sig));
      });
  hub.declare_connectable_handler<comp::audio::stop, audio_system,
                                  comp::audio> (
      "on_stop", +[] (sys::ecs_system &sys, entt::registry &reg,
                      entt::entity ent, const void *sig) {
        static_cast<audio_system &> (sys).on_stop (
            reg, ent, *static_cast<const comp::audio::stop *> (sig));
      });
  hub.declare_connectable_handler<comp::audio::pause, audio_system,
                                  comp::audio> (
      "on_pause", +[] (sys::ecs_system &sys, entt::registry &reg,
                       entt::entity ent, const void *sig) {
        static_cast<audio_system &> (sys).on_pause (
            reg, ent, *static_cast<const comp::audio::pause *> (sig));
      });
  hub.declare_connectable_handler<comp::audio::resume, audio_system,
                                  comp::audio> (
      "on_resume", +[] (sys::ecs_system &sys, entt::registry &reg,
                        entt::entity ent, const void *sig) {
        static_cast<audio_system &> (sys).on_resume (
            reg, ent, *static_cast<const comp::audio::resume *> (sig));
      });
  hub.declare_connectable_handler<comp::audio::set_volume, audio_system,
                                  comp::audio> (
      "on_set_volume", +[] (sys::ecs_system &sys, entt::registry &reg,
                            entt::entity ent, const void *sig) {
        static_cast<audio_system &> (sys).on_set_volume (
            reg, ent, *static_cast<const comp::audio::set_volume *> (sig));
      });
}

void
audio_system::on_play (entt::registry &registry, entt::entity entity,
                       const comp::audio::play & /*event*/)
{
  if (auto *audio = registry.try_get<comp::audio> (entity)) {
    audio->playing = true;
  }
}

void
audio_system::on_stop (entt::registry &registry, entt::entity entity,
                       const comp::audio::stop & /*event*/)
{
  if (auto *audio = registry.try_get<comp::audio> (entity)) {
    audio->playing = false;
  }
}

void
audio_system::on_pause (entt::registry &registry, entt::entity entity,
                        const comp::audio::pause & /*event*/)
{
  (void)registry;
  if (m_active_loops.contains (entity)) {
    MIX_PauseTrack (m_active_loops[entity]);
  }
}

void
audio_system::on_resume (entt::registry &registry, entt::entity entity,
                         const comp::audio::resume & /*event*/)
{
  (void)registry;
  if (m_active_loops.contains (entity)) {
    MIX_ResumeTrack (m_active_loops[entity]);
  }
}

void
audio_system::on_set_volume (entt::registry &registry, entt::entity entity,
                             const comp::audio::set_volume &event)
{
  if (auto *audio = registry.try_get<comp::audio> (entity)) {
    audio->volume = event.volume;
    if (m_active_loops.contains (entity)) {
      MIX_SetTrackGain (m_active_loops[entity], audio->volume);
    }
  }
}

void
audio_system::on_update (entt::registry &registry, double /*dt*/)
{
  auto &ctx = registry.ctx ();
  if (!ctx.contains<comp::singl::runtime_context *> ()) {
    return;
  }
  auto &runtime_ctx = *ctx.get<comp::singl::runtime_context *> ();
  MIX_Mixer *mixer = runtime_ctx.resource_manager ().mixer ();

  if (mixer == nullptr) {
    return;
  }

  auto view = registry.view<comp::audio> ();

  for (auto entity : view) {
    auto &audio = view.get<comp::audio> (entity);

    // Skip if no resource assigned
    if (audio.audio_resource.value == entt::null) {
      if (m_active_loops.contains (entity)) {
        MIX_DestroyTrack (m_active_loops[entity]);
        m_active_loops.erase (entity);
      }
      audio.playing = false;
      continue;
    }

    // Handle Start/Stop
    if (audio.playing && !audio.was_playing) {
      // START
      MIX_Audio *resource
          = runtime_ctx.resource_manager ().get (audio.audio_resource);
      if (resource != nullptr) {
        if (audio.loop) {
          // If already looping something else, stop it.
          if (m_active_loops.contains (entity)) {
            MIX_DestroyTrack (m_active_loops[entity]);
          }
          MIX_Track *track = MIX_CreateTrack (mixer);
          MIX_SetTrackAudio (track, resource);
          MIX_SetTrackLoops (track, -1); // Infinite loop
          MIX_SetTrackGain (track, audio.volume);
          MIX_PlayTrack (track, 0); // 0 for default options
          m_active_loops[entity] = track;
        } else {
          MIX_PlayAudio (mixer, resource);
          // For one-shots, we set playing back to false immediately
          audio.playing = false;
        }
      }
    } else if (!audio.playing && audio.was_playing) {
      // STOP
      if (m_active_loops.contains (entity)) {
        MIX_DestroyTrack (m_active_loops[entity]);
        m_active_loops.erase (entity);
      }
    }

    // Update volume for active loops
    if (audio.loop && m_active_loops.contains (entity)) {
      MIX_SetTrackGain (m_active_loops[entity], audio.volume);
    }

    audio.was_playing = audio.playing;
  }

  // Cleanup loops for entities that no longer have the audio component
  for (auto it = m_active_loops.begin (); it != m_active_loops.end ();) {
    if (!registry.valid (it->first)
        || !registry.all_of<comp::audio> (it->first)) {
      MIX_DestroyTrack (it->second);
      it = m_active_loops.erase (it);
    } else {
      ++it;
    }
  }
}

void
audio_system::on_init (entt::registry &registry)
{
  auto view = registry.view<comp::audio> ();
  for (auto entity : view) {
    auto &audio = view.get<comp::audio> (entity);
    if (audio.play_on_start) {
      audio.playing = true;
    }
  }
}

void
audio_system::on_inactive (entt::registry & /*registry*/)
{
  stop_all_loops ();
}

void
audio_system::stop_all_loops ()
{
  for (auto &[entity, track] : m_active_loops) {
    MIX_DestroyTrack (track);
  }
  m_active_loops.clear ();
}

} // namespace sys

} // namespace wsl
