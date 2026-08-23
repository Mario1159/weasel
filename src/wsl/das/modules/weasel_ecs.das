options remove_unused_symbols = false
module weasel_ecs

require weasel_api

// Base class for daslang ECS systems.
// Systems inherit from this and override the lifecycle methods.
// C++ calls these methods through the class adapter pattern (tutorial 19).
//
// Usage:
//   require weasel_api
//   require daslib/weasel_ecs
//
//   class MySystem : EcsSystem {
//       // state fields here
//
//       def override on_init() : void {
//           // initialization code
//       }
//
//       def override on_update(dt : float) : void {
//           // per-frame update code
//       }
//
//       def override on_inactive() : void {
//           // cleanup code
//       }
//   }
//
// The C++ engine detects the "System" class and instantiates it via the
// class adapter, which dispatches virtual method calls to these overrides.
//
// Input is delivered through the pull-only message bus, not a per-event
// callback. Read engine input each frame via the `mouse_motion_at`,
// `key_pressed_at`, `text_input_at`, ... accessors (see `weasel_api`),
// e.g.:
//
//       def override on_update(dt : float) : void {
//           for i in range(mouse_motion_count()) {
//               var m = mouse_motion_at(i)
//               m_accumulated_dx += m.xrel
//           }
//       }

class EcsSystem {
    def on_init() : void { }
    def on_update(dt : float) : void { }
    def on_inactive() : void { }
}
