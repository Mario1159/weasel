#include "event.hpp"

namespace wsl
{

event_kind
engine_event::classify (uint32_t sdl_type)
{
  switch (sdl_type) {
  case SDL_EVENT_MOUSE_MOTION:
    return event_kind::mouse_motion;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    return event_kind::mouse_button_down;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    return event_kind::mouse_button_up;
  case SDL_EVENT_MOUSE_WHEEL:
    return event_kind::mouse_wheel;
  case SDL_EVENT_KEY_DOWN:
    return event_kind::key_down;
  case SDL_EVENT_KEY_UP:
    return event_kind::key_up;
  case SDL_EVENT_TEXT_INPUT:
    return event_kind::text_input;
  case SDL_EVENT_QUIT:
    return event_kind::quit;
  default:
    if (sdl_type >= SDL_EVENT_WINDOW_FIRST
        && sdl_type <= SDL_EVENT_WINDOW_LAST) {
      return event_kind::window;
    }
    return event_kind::other;
  }
}

} // namespace wsl
