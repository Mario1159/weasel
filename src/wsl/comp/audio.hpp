#pragma once

#include "../rsc/resource_manager.hpp"
#include "component_meta.hpp"
#include <cereal/cereal.hpp>
#include <entt/entt.hpp>

namespace wsl
{

namespace comp
{

struct audio : world_component
{
  struct play
  {
    entt::entity entity = entt::null;
  };

  struct stop
  {
    entt::entity entity = entt::null;
  };

  struct pause
  {
    entt::entity entity = entt::null;
  };

  struct resume
  {
    entt::entity entity = entt::null;
  };

  struct set_volume
  {
    entt::entity entity = entt::null;
    float volume = 1.0F;
  };

  rsc::audio_id audio_resource{};
  bool loop = false;
  bool play_on_start = true;
  float volume = 1.0F;

  // Runtime state (not serialized)
  bool playing = false;
  bool was_playing = false;

  static void
  register_meta ()
  {
    using namespace entt::literals;

    entt::meta_factory<comp::audio> ()
        .type (entt::type_hash<comp::audio>::value ())
        .custom<comp::meta_info> (meta_info{
            "Audio", "Provides audio playback functionality for the entity.",
            "" })

        .data<&comp::audio::audio_resource> ("audio_resource"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Audio Resource", "The audio file to play.", "" })

        .data<&comp::audio::loop> ("loop"_hs)
        .custom<comp::meta_info> (meta_info{
            "Loop", "Whether the audio should restart when finished.", "" })

        .data<&comp::audio::play_on_start> ("play_on_start"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Play on Start",
                       "Whether the audio should start automatically "
                       "when the scene begins.",
                       "" })

        .data<&comp::audio::volume> ("volume"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Volume", "Playback volume (0.0 to 1.0).", "" })

        .data<&comp::audio::playing> ("playing"_hs)
        .custom<comp::meta_info> (
            meta_info{ "Playing", "Current playback status.", "" });
  }

  template <class Archive>
  void
  serialize (Archive &archive)
  {
    using namespace cereal;
    auto *mgr = rsc::resource_manager::serialization_context::get ();

    if constexpr (std::is_base_of_v<cereal::detail::OutputArchiveBase,
                                    Archive>) {
      std::string path = "None";
      if (mgr) {
        path = mgr->get_resource_path (audio_resource);
      }
      archive (make_nvp ("audio_path", path), make_nvp ("loop", loop),
               make_nvp ("play_on_start", play_on_start),
               make_nvp ("volume", volume));
    } else {
      std::string path;
      archive (make_nvp ("audio_path", path), make_nvp ("loop", loop),
               make_nvp ("play_on_start", play_on_start),
               make_nvp ("volume", volume));

      if (path != "None" && !path.empty () && mgr) {
        audio_resource = mgr->register_audio (path);
      } else {
        audio_resource.value = entt::null;
      }
    }
  }
};

} // namespace comp

} // namespace wsl
