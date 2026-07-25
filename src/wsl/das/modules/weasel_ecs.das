options remove_unused_symbols = false

// Base class for daslang ECS systems.
// Systems inherit from this and override the lifecycle methods.
// C++ calls these methods through the class adapter pattern.

class EcsSystem {
    def abstract on_init() : void
    def abstract on_update(dt : float) : void
    def abstract on_event() : void
    def abstract on_inactive() : void
}
