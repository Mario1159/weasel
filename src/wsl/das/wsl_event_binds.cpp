#include "wsl_event_binds.hpp"

#include "wsl/das/wsl_api_module.hpp"
#include "wsl/event/message_bus.hpp"
#include "wsl/input.hpp"
#include "wsl/comp/component_meta.hpp"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_handle.h"
#include "daScript/ast/ast_interop.h"
#include "daScript/daScriptModule.h"
#include "daScript/misc/arraytype.h"
#include "daScript/simulate/aot.h"

#include <cstring>

// ── Proxy value types exposed to Daslang ──
// Scripts read engine input messages through these value structs. Each holds a
// copy of the posted `wsl::input::*` POD and exposes read-only getters so the
// Daslang struct mirrors the field layout without tying scripts to SDL types.

namespace wsl::das
{

struct MouseMotionMsg
{
  wsl::input::mouse_motion v{};
  void
  bind (const wsl::input::mouse_motion &o)
  {
    v = o;
  }
  int
  x ()
  {
    return v.x;
  }
  int
  y ()
  {
    return v.y;
  }
  int
  xrel ()
  {
    return v.xrel;
  }
  int
  yrel ()
  {
    return v.yrel;
  }
};

struct MouseButtonMsg
{
  wsl::input::mouse_button v{};
  void
  bind (const wsl::input::mouse_button &o)
  {
    v = o;
  }
  int
  button ()
  {
    return static_cast<int> (v.button);
  }
  bool
  down ()
  {
    return v.down;
  }
  int
  x ()
  {
    return v.x;
  }
  int
  y ()
  {
    return v.y;
  }
};

struct MouseWheelMsg
{
  wsl::input::mouse_wheel v{};
  void
  bind (const wsl::input::mouse_wheel &o)
  {
    v = o;
  }
  int
  x ()
  {
    return v.x;
  }
  int
  y ()
  {
    return v.y;
  }
  bool
  flipped ()
  {
    return v.flipped;
  }
};

struct KeyPressedMsg
{
  wsl::input::key_pressed v{};
  void
  bind (const wsl::input::key_pressed &o)
  {
    v = o;
  }
  uint32_t
  scancode ()
  {
    return static_cast<uint32_t> (v.scancode);
  }
  int32_t
  keycode ()
  {
    return static_cast<int32_t> (v.keycode);
  }
  uint32_t
  mod ()
  {
    return static_cast<uint32_t> (v.mod);
  }
  bool
  repeat ()
  {
    return v.repeat;
  }
};

struct KeyReleasedMsg
{
  wsl::input::key_released v{};
  void
  bind (const wsl::input::key_released &o)
  {
    v = o;
  }
  uint32_t
  scancode ()
  {
    return static_cast<uint32_t> (v.scancode);
  }
  int32_t
  keycode ()
  {
    return static_cast<int32_t> (v.keycode);
  }
  uint32_t
  mod ()
  {
    return static_cast<uint32_t> (v.mod);
  }
};

struct TextInputMsg
{
  wsl::input::text_input v{};
  void
  bind (const wsl::input::text_input &o)
  {
    v = o;
  }
};

struct WindowResizedMsg
{
  wsl::input::window_resized v{};
  void
  bind (const wsl::input::window_resized &o)
  {
    v = o;
  }
  int
  width ()
  {
    return v.width;
  }
  int
  height ()
  {
    return v.height;
  }
};

struct QuitRequestedMsg
{
  wsl::input::quit_requested v{};
  void
  bind (const wsl::input::quit_requested &o)
  {
    v = o;
  }
  bool
  requested ()
  {
    return v.requested;
  }
};

} // namespace wsl::das

MAKE_TYPE_FACTORY (MouseMotion, wsl::das::MouseMotionMsg)
MAKE_TYPE_FACTORY (MouseButton, wsl::das::MouseButtonMsg)
MAKE_TYPE_FACTORY (MouseWheel, wsl::das::MouseWheelMsg)
MAKE_TYPE_FACTORY (KeyPressed, wsl::das::KeyPressedMsg)
MAKE_TYPE_FACTORY (KeyReleased, wsl::das::KeyReleasedMsg)
MAKE_TYPE_FACTORY (TextInput, wsl::das::TextInputMsg)
MAKE_TYPE_FACTORY (WindowResized, wsl::das::WindowResizedMsg)
MAKE_TYPE_FACTORY (QuitRequested, wsl::das::QuitRequestedMsg)

namespace
{

template <typename Pod>
size_t
msg_count ()
{
  auto *bus = wsl::das::wsl_api_get_active_message_bus ();
  if (bus == nullptr) {
    return 0;
  }
  auto *buf = bus->find_published (wsl::comp::stable_type_id<Pod> ());
  if (buf == nullptr || buf->bytes.empty () || buf->element_size == 0) {
    return 0;
  }
  return buf->bytes.size () / buf->element_size;
}

template <typename Pod, typename Proxy>
Proxy
msg_at (size_t idx)
{
  Proxy p{};
  auto *bus = wsl::das::wsl_api_get_active_message_bus ();
  if (bus == nullptr) {
    return p;
  }
  auto *buf = bus->find_published (wsl::comp::stable_type_id<Pod> ());
  if (buf == nullptr || buf->bytes.empty () || buf->element_size == 0) {
    return p;
  }
  const size_t n = buf->bytes.size () / buf->element_size;
  if (idx >= n) {
    return p;
  }
  Pod v{};
  std::memcpy (&v, buf->bytes.data () + idx * buf->element_size,
               buf->element_size);
  p.bind (v);
  return p;
}

int
wsl_mouse_motion_count ()
{
  return static_cast<int> (msg_count<wsl::input::mouse_motion> ());
}
wsl::das::MouseMotionMsg
wsl_mouse_motion_at (int idx)
{
  return msg_at<wsl::input::mouse_motion, wsl::das::MouseMotionMsg> (
      static_cast<size_t> (idx));
}

int
wsl_mouse_button_count ()
{
  return static_cast<int> (msg_count<wsl::input::mouse_button> ());
}
wsl::das::MouseButtonMsg
wsl_mouse_button_at (int idx)
{
  return msg_at<wsl::input::mouse_button, wsl::das::MouseButtonMsg> (
      static_cast<size_t> (idx));
}

int
wsl_mouse_wheel_count ()
{
  return static_cast<int> (msg_count<wsl::input::mouse_wheel> ());
}
wsl::das::MouseWheelMsg
wsl_mouse_wheel_at (int idx)
{
  return msg_at<wsl::input::mouse_wheel, wsl::das::MouseWheelMsg> (
      static_cast<size_t> (idx));
}

int
wsl_key_pressed_count ()
{
  return static_cast<int> (msg_count<wsl::input::key_pressed> ());
}
wsl::das::KeyPressedMsg
wsl_key_pressed_at (int idx)
{
  return msg_at<wsl::input::key_pressed, wsl::das::KeyPressedMsg> (
      static_cast<size_t> (idx));
}

int
wsl_key_released_count ()
{
  return static_cast<int> (msg_count<wsl::input::key_released> ());
}
wsl::das::KeyReleasedMsg
wsl_key_released_at (int idx)
{
  return msg_at<wsl::input::key_released, wsl::das::KeyReleasedMsg> (
      static_cast<size_t> (idx));
}

int
wsl_text_input_count ()
{
  return static_cast<int> (msg_count<wsl::input::text_input> ());
}
wsl::das::TextInputMsg
wsl_text_input_at (int idx)
{
  return msg_at<wsl::input::text_input, wsl::das::TextInputMsg> (
      static_cast<size_t> (idx));
}

const char *
wsl_text_input_text (int idx)
{
  static thread_local std::string s_text;
  s_text.clear ();

  auto *bus = wsl::das::wsl_api_get_active_message_bus ();
  if (bus != nullptr) {
    auto *buf = bus->find_published (
        wsl::comp::stable_type_id<wsl::input::text_input> ());
    if (buf != nullptr && !buf->bytes.empty () && buf->element_size > 0) {
      const size_t n = buf->bytes.size () / buf->element_size;
      if (static_cast<size_t> (idx) < n) {
        wsl::input::text_input v{};
        std::memcpy (&v,
                     buf->bytes.data ()
                         + static_cast<size_t> (idx) * buf->element_size,
                     buf->element_size);
        s_text = v.text;
      }
    }
  }
  return s_text.c_str ();
}

int
wsl_window_resized_count ()
{
  return static_cast<int> (msg_count<wsl::input::window_resized> ());
}
wsl::das::WindowResizedMsg
wsl_window_resized_at (int idx)
{
  return msg_at<wsl::input::window_resized, wsl::das::WindowResizedMsg> (
      static_cast<size_t> (idx));
}

int
wsl_quit_requested_count ()
{
  return static_cast<int> (msg_count<wsl::input::quit_requested> ());
}
wsl::das::QuitRequestedMsg
wsl_quit_requested_at (int idx)
{
  return msg_at<wsl::input::quit_requested, wsl::das::QuitRequestedMsg> (
      static_cast<size_t> (idx));
}

// ── Annotations ──

struct MouseMotionMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::MouseMotionMsg, false, false>
{
  explicit MouseMotionMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::MouseMotionMsg, false,
                                          false> ("MouseMotion", lib,
                                                  "wsl::das::MouseMotionMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (x)> ("x", "x");
    addProperty<DAS_BIND_MANAGED_PROP (y)> ("y", "y");
    addProperty<DAS_BIND_MANAGED_PROP (xrel)> ("xrel", "xrel");
    addProperty<DAS_BIND_MANAGED_PROP (yrel)> ("yrel", "yrel");
  }
};

struct MouseButtonMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::MouseButtonMsg, false, false>
{
  explicit MouseButtonMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::MouseButtonMsg, false,
                                          false> ("MouseButton", lib,
                                                  "wsl::das::MouseButtonMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (button)> ("button", "button");
    addProperty<DAS_BIND_MANAGED_PROP (down)> ("down", "down");
    addProperty<DAS_BIND_MANAGED_PROP (x)> ("x", "x");
    addProperty<DAS_BIND_MANAGED_PROP (y)> ("y", "y");
  }
};

struct MouseWheelMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::MouseWheelMsg, false, false>
{
  explicit MouseWheelMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::MouseWheelMsg, false,
                                          false> ("MouseWheel", lib,
                                                  "wsl::das::MouseWheelMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (x)> ("x", "x");
    addProperty<DAS_BIND_MANAGED_PROP (y)> ("y", "y");
    addProperty<DAS_BIND_MANAGED_PROP (flipped)> ("flipped", "flipped");
  }
};

struct KeyPressedMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::KeyPressedMsg, false, false>
{
  explicit KeyPressedMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::KeyPressedMsg, false,
                                          false> ("KeyPressed", lib,
                                                  "wsl::das::KeyPressedMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (scancode)> ("scancode", "scancode");
    addProperty<DAS_BIND_MANAGED_PROP (keycode)> ("keycode", "keycode");
    addProperty<DAS_BIND_MANAGED_PROP (mod)> ("mod", "mod");
    addProperty<DAS_BIND_MANAGED_PROP (repeat)> ("repeat", "repeat");
  }
};

struct KeyReleasedMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::KeyReleasedMsg, false, false>
{
  explicit KeyReleasedMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::KeyReleasedMsg, false,
                                          false> ("KeyReleased", lib,
                                                  "wsl::das::KeyReleasedMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (scancode)> ("scancode", "scancode");
    addProperty<DAS_BIND_MANAGED_PROP (keycode)> ("keycode", "keycode");
    addProperty<DAS_BIND_MANAGED_PROP (mod)> ("mod", "mod");
  }
};

struct TextInputMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::TextInputMsg, false, false>
{
  explicit TextInputMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::TextInputMsg, false,
                                          false> ("TextInput", lib,
                                                  "wsl::das::TextInputMsg")
  {
  }
};

struct WindowResizedMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::WindowResizedMsg, false,
                                        false>
{
  explicit WindowResizedMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::WindowResizedMsg, false,
                                          false> ("WindowResized", lib,
                                                  "wsl::das::WindowResizedMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (width)> ("width", "width");
    addProperty<DAS_BIND_MANAGED_PROP (height)> ("height", "height");
  }
};

struct QuitRequestedMsgAnnotation
    : ::das::ManagedStructureAnnotation<wsl::das::QuitRequestedMsg, false,
                                        false>
{
  explicit QuitRequestedMsgAnnotation (::das::ModuleLibrary &lib)
      : ::das::ManagedStructureAnnotation<wsl::das::QuitRequestedMsg, false,
                                          false> ("QuitRequested", lib,
                                                  "wsl::das::QuitRequestedMsg")
  {
    addProperty<DAS_BIND_MANAGED_PROP (requested)> ("requested", "requested");
  }
};

} // namespace

namespace wsl::das
{

void
register_event_message_bindings (::das::Module &module,
                                 ::das::ModuleLibrary &lib)
{
  module.addAnnotation (new MouseMotionMsgAnnotation (lib));
  module.addAnnotation (new MouseButtonMsgAnnotation (lib));
  module.addAnnotation (new MouseWheelMsgAnnotation (lib));
  module.addAnnotation (new KeyPressedMsgAnnotation (lib));
  module.addAnnotation (new KeyReleasedMsgAnnotation (lib));
  module.addAnnotation (new TextInputMsgAnnotation (lib));
  module.addAnnotation (new WindowResizedMsgAnnotation (lib));
  module.addAnnotation (new QuitRequestedMsgAnnotation (lib));

  addExtern<DAS_BIND_FUN (wsl_mouse_motion_count)> (
      module, lib, "mouse_motion_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_motion_count");
  addExtern<DAS_BIND_FUN (wsl_mouse_motion_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "mouse_motion_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_motion_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_mouse_button_count)> (
      module, lib, "mouse_button_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_button_count");
  addExtern<DAS_BIND_FUN (wsl_mouse_button_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "mouse_button_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_button_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_mouse_wheel_count)> (
      module, lib, "mouse_wheel_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_wheel_count");
  addExtern<DAS_BIND_FUN (wsl_mouse_wheel_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "mouse_wheel_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_mouse_wheel_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_key_pressed_count)> (
      module, lib, "key_pressed_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_key_pressed_count");
  addExtern<DAS_BIND_FUN (wsl_key_pressed_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "key_pressed_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_key_pressed_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_key_released_count)> (
      module, lib, "key_released_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_key_released_count");
  addExtern<DAS_BIND_FUN (wsl_key_released_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "key_released_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_key_released_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_text_input_count)> (
      module, lib, "text_input_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_text_input_count");
  addExtern<DAS_BIND_FUN (wsl_text_input_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "text_input_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_text_input_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_text_input_text)> (
      module, lib, "text_input_text", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_text_input_text")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_window_resized_count)> (
      module, lib, "window_resized_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_window_resized_count");
  addExtern<DAS_BIND_FUN (wsl_window_resized_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "window_resized_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_window_resized_at")
      ->args ({ "index" });

  addExtern<DAS_BIND_FUN (wsl_quit_requested_count)> (
      module, lib, "quit_requested_count", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_quit_requested_count");
  addExtern<DAS_BIND_FUN (wsl_quit_requested_at),
            ::das::SimNode_ExtFuncCallAndCopyOrMove> (
      module, lib, "quit_requested_at", ::das::SideEffects::accessExternal,
      "wsl::das::wsl_quit_requested_at")
      ->args ({ "index" });
}

} // namespace wsl::das
