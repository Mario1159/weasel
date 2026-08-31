#pragma once

#include <SDL3/SDL_events.h>
#include <cstdint>

namespace wsl
{

/**
 * High-level event kind used by the engine's event dispatch.
 *
 * Maps the most common SDL3 event types to a small, script-friendly
 * enumeration.  Events that do not map to a named kind are classified
 * as `other`.
 */
enum class event_kind : uint32_t
{
  none = 0,
  mouse_motion,
  mouse_button_down,
  mouse_button_up,
  mouse_wheel,
  key_down,
  key_up,
  text_input,
  window,
  quit,
  other
};

/**
 * Engine-level event wrapper.
 *
 * Wraps an `SDL_Event` and exposes a typed `kind()` accessor so that
 * daslang (and C++) consumers can dispatch without touching SDL
 * headers directly.  Raw SDL data is still accessible through the
 * `sdl()` accessor for backward compatibility.
 *
 * This is a core weasel type — not das-specific.  The conversion from
 * `SDL_Event` happens once at the event-loop boundary (`app::run`).
 */
class engine_event
{
public:
  engine_event () = default;
  explicit engine_event (const SDL_Event &e) : m_sdl (e) {}

  /** Returns the high-level kind of this event. */
  event_kind
  kind () const
  {
    return classify (m_sdl.type);
  }

  /** Returns the raw SDL event type (e.g. `SDL_EVENT_MOUSE_MOTION`). */
  uint32_t
  type () const
  {
    return m_sdl.type;
  }

  /** Returns the underlying SDL event (const). */
  const SDL_Event &
  sdl () const
  {
    return m_sdl;
  }

  /**
 * Returns the underlying SDL event (mutable).
 *
 * Provided for call-sites that need to adjust event fields before
 * forwarding (e.g. coordinate remapping in `render_ui_system`).
 */
  SDL_Event &
  sdl ()
  {
    return m_sdl;
  }

  const SDL_MouseMotionEvent &
  as_mouse_motion () const
  {
    return m_sdl.motion;
  }

  const SDL_MouseButtonEvent &
  as_mouse_button () const
  {
    return m_sdl.button;
  }

  const SDL_MouseWheelEvent &
  as_mouse_wheel () const
  {
    return m_sdl.wheel;
  }

  const SDL_KeyboardEvent &
  as_keyboard () const
  {
    return m_sdl.key;
  }

  const SDL_TextInputEvent &
  as_text_input () const
  {
    return m_sdl.text;
  }

  const SDL_WindowEvent &
  as_window () const
  {
    return m_sdl.window;
  }

private:
  static event_kind classify (uint32_t sdl_type);
  SDL_Event m_sdl{};
};

} // namespace wsl
