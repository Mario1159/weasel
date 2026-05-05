#pragma once

#include "system.hpp"
#include "../comp/audio.hpp"
#include <SDL3_mixer/SDL_mixer.h>
#include <unordered_map>


namespace wsl
{

namespace sys
{

class audio_system : public sys::ecs_system_t<audio_system>
{
public:
  explicit audio_system (const std::string &name);
  ~audio_system () override; 

  void on_update (entt::registry &registry, double dt) override;
  void on_init (entt::registry &registry) override;
  void on_inactive (entt::registry &registry) override;

  void register_signals (reg::sig::signal_hub &hub) override;
  void register_event_handlers (reg::sig::signal_hub &hub) override;

  // Event handlers
  void on_play (entt::registry &registry, entt::entity entity,
                const comp::audio::play &event);
  void on_stop (entt::registry &registry, entt::entity entity,
                const comp::audio::stop &event);
  void on_pause (entt::registry &registry, entt::entity entity,
                 const comp::audio::pause &event);
  void on_resume (entt::registry &registry, entt::entity entity,
                  const comp::audio::resume &event);
  void on_set_volume (entt::registry &registry, entt::entity entity,
                      const comp::audio::set_volume &event);

private:
  // Tracks active looping tracks per entity.
  std::unordered_map<entt::entity, MIX_Track *> m_active_loops;

  void stop_all_loops ();
};

} // namespace sys

} // namespace wsl
