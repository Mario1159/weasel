#include "input.hpp"

#include <SDL3/SDL_keyboard.h>

namespace wsl
{

namespace input
{

input_system::input_system (reg::sig::signal_hub &hub)
    : m_hub (hub)
{
}

void
input_system::process_event (const SDL_Event &event)
{
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN: {
    key_pressed evt{
        .scancode = event.key.scancode,
        .keycode = event.key.key,
        .mod = event.key.mod,
        .repeat = event.key.repeat,
    };
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

   case SDL_EVENT_KEY_UP: {
     key_released evt{
         .scancode = event.key.scancode,
         .keycode = event.key.key,
         .mod = event.key.mod,
     };
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

   case SDL_EVENT_TEXT_INPUT: {
     text_input evt{.text = event.text.text};
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

   case SDL_EVENT_MOUSE_MOTION: {
     mouse_motion evt{
         .x = static_cast<int>(event.motion.x),
         .y = static_cast<int>(event.motion.y),
         .xrel = static_cast<int>(event.motion.xrel),
         .yrel = static_cast<int>(event.motion.yrel),
     };
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

   case SDL_EVENT_MOUSE_BUTTON_DOWN:
   case SDL_EVENT_MOUSE_BUTTON_UP: {
     mouse_button evt{
         .button = event.button.button,
         .down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
         .x = static_cast<int>(event.button.x),
         .y = static_cast<int>(event.button.y),
     };
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

   case SDL_EVENT_MOUSE_WHEEL: {
     mouse_wheel evt{
         .x = static_cast<int>(event.wheel.x),
         .y = static_cast<int>(event.wheel.y),
         .flipped = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED,
     };
     wsl::reg::sig::emit (m_hub, evt);
     break;
   }

  default:
    break;
  }
}

void
input_system::refresh ()
{
  m_keyboard_state.refresh ();
}

}
}
