#pragma once

#include "reg/sig/signal_hub.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace input
{

/*!
 * \brief Represents a key binding with scancode, keycode, and modifier support.
 *
 * Supports two modes:
 * - Scancode mode: physical key location (WASD-style movement)
 * - Keycode mode: specific symbol ("press I to open inventory")
 */
class key_binding
{
public:
  SDL_Keymod mod = SDL_KMOD_NONE;
  SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
  SDL_Keycode keycode = SDLK_UNKNOWN;

  /*!
   * \brief Checks if this binding uses scancode (physical key location).
   * \return `true` if scancode is set, otherwise `false`.
   */
  constexpr bool
  is_scancode () const
  {
    return scancode != SDL_SCANCODE_UNKNOWN;
  }

  /*!
   * \brief Checks if this binding uses keycode (specific symbol).
   * \return `true` if keycode is set, otherwise `false`.
   */
  constexpr bool
  is_keycode () const
  {
    return keycode != SDLK_UNKNOWN;
  }

  /*!
   * \brief Checks if this binding matches a keyboard event.
   * \param e keyboard event to check.
   * \return `true` if the event matches this binding, otherwise `false`.
   */
  bool
  matches_event (const SDL_KeyboardEvent &e) const
  {
    if (mod != SDL_KMOD_NONE) {
      if ((e.mod & mod) == 0) {
        return false;
      }
    }

    if (is_scancode ()) {
      return e.key == SDL_GetKeyFromScancode (scancode, mod, true);
    }

    if (is_keycode ()) {
      return e.key == keycode;
    }

    return false;
  }

  /*!
   * \brief Gets a human-readable name for this binding.
   * \return Pointer to a null-terminated string with the key name.
   */
  [[nodiscard]] const char *
  get_name () const
  {
    if (is_keycode ()) {
      return SDL_GetKeyName (keycode);
    }
    if (is_scancode ()) {
      return SDL_GetScancodeName (scancode);
    }
    return "Unknown";
  }
};

/*!
 * \brief Maps action names to key bindings.
 */
class action_map
{
public:
  std::unordered_map<std::string, key_binding> bindings;
};

/*!
 * \brief Wraps SDL keyboard state for per-frame input checking.
 *
 * Use this for continuous input (movement) to get smooth input without
 * key repeat delays. Call refresh() at the start of each frame.
 */
class keyboard_state
{
public:
  explicit keyboard_state () = default;

  /*!
   * \brief Refreshes the keyboard state from SDL.
   *
   * Call this once per frame before checking input state.
   */
  void
  refresh ()
  {
    m_states = SDL_GetKeyboardState (&m_num_keys);
  }

  /*!
   * \brief Checks if a scancode key is currently pressed.
   * \param scancode SDL scancode to check.
   * \return `true` if the key is pressed, otherwise `false`.
   */
  [[nodiscard]] bool
  is_down (SDL_Scancode scancode) const
  {
    if (m_states == nullptr) {
      return false;
    }
    return m_states[scancode] != 0;
  }

  /*!
   * \brief Checks if a key binding is currently pressed.
   * \param binding key binding to check.
   * \return `true` if the key is pressed, otherwise `false`.
   */
  [[nodiscard]] bool
  is_down (key_binding binding) const
  {
    return is_down (binding.scancode);
  }

  /*!
   * \brief Gets a directional axis from two opposing scancodes.
   *
   * Example: get_axis(SDL_SCANCODE_W, SDL_SCANCODE_S) returns 1 when W
   * is pressed, -1 when S is pressed, or 0 when neither or both are pressed.
   * \param positive scancode for positive direction.
   * \param negative scancode for negative direction.
   * \return -1, 0, or 1 depending on which keys are pressed.
   */
  [[nodiscard]] int
  get_axis (SDL_Scancode positive, SDL_Scancode negative) const
  {
    int axis = 0;
    if (is_down (positive)) {
      axis += 1;
    }
    if (is_down (negative)) {
      axis -= 1;
    }
    return axis;
  }

  /*!
   * \brief Gets a directional axis from two key bindings.
   * \param positive key binding for positive direction.
   * \param negative key binding for negative direction.
   * \return -1, 0, or 1 depending on which keys are pressed.
   */
  [[nodiscard]] int
  get_axis (const key_binding &positive, const key_binding &negative) const
  {
    return get_axis (positive.scancode, negative.scancode);
  }

private:
  const bool *m_states = nullptr;
  int m_num_keys = 0;
};

/*!
 * \brief Event emitted when a key is pressed.
 */
struct key_pressed
{
  SDL_Scancode scancode;
  SDL_Keycode keycode;
  SDL_Keymod mod;
  bool repeat;
};

/*!
 * \brief Event emitted when a key is released.
 */
struct key_released
{
  SDL_Scancode scancode;
  SDL_Keycode keycode;
  SDL_Keymod mod;
};

/*!
 * \brief Event emitted when text input is received.
 */
struct text_input
{
  std::string text;
};

/*!
 * \brief Event emitted when the mouse moves.
 */
struct mouse_motion
{
  int x;
  int y;
  int xrel;
  int yrel;
};

/*!
 * \brief Event emitted when a mouse button is pressed or released.
 */
struct mouse_button
{
  Uint8 button;
  bool down;
  int x;
  int y;
};

/*!
 * \brief Event emitted when the mouse wheel is scrolled.
 */
struct mouse_wheel
{
  int x;
  int y;
  bool flipped;
};

/*!
 * \brief Input system that processes SDL events and updates input state.
 *
 * This class bridges SDL3 input events with the weasel signal infrastructure.
 * It processes SDL events and can emitinput events through the signal hub.
 */
class input_system
{
public:
  /*!
   * \brief Constructs an input system.
   * \param hub signal hub for emitting input events.
   */
  explicit input_system (reg::sig::signal_hub &hub);

  /*!
   * \brief Processes an SDL event.
   * \param event SDL event to process.
   */
  void
  process_event (const SDL_Event &event);

  /*!
   * \brief Refreshes the keyboard state.
   *
   * Call this once per frame before checking keyboard state.
   */
  void
  refresh ();

  /*!
   * \brief Gets the current keyboard state.
   * \return Reference to the keyboard state.
   */
  keyboard_state &
  get_keyboard_state ()
  {
    return m_keyboard_state;
  }

  /*!
   * \brief Gets the current keyboard state.
   * \return Const reference to the keyboard state.
   */
  const keyboard_state &
  get_keyboard_state () const
  {
    return m_keyboard_state;
  }

private:
  reg::sig::signal_hub &m_hub;
  keyboard_state m_keyboard_state;
};

}
}