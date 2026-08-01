options remove_unused_symbols = false
module weasel_ecs

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
//       def override on_event() : void {
//           // event handling code
//       }
//
//       def override on_inactive() : void {
//           // cleanup code
//       }
//   }
//
// The C++ engine detects the "System" class and instantiates it via the
// class adapter, which dispatches virtual method calls to these overrides.

class EcsSystem {
    def on_init() : void { }
    def on_update(dt : float) : void { }
    def on_event() : void { }
    def on_inactive() : void { }
}
