Getting Started
===============

Writing Your First das System
-----------------------------

Create a ``my_system.das`` file:

.. code-block:: cpp

    options gen2
    require weasel_api

    def on_init()
        log_info("My system initialized!")

    def on_update(dt : float)
        let e = entity_create()
        add_component(e, Transform())
        let t = get_component_or(e, Transform())
        t.position = float3(0.0, 1.0, 0.0)
        log_info("Created entity at y=1")

    def on_event()
        let kind = get_event_kind()
        if kind == EVENT_MOUSE_BUTTON_DOWN
            let x = get_event_mouse_x()
            let y = get_event_mouse_y()
            log_info("Mouse click at " + string(x) + "," + string(y))

    def on_inactive()
        log_info("My system going inactive")

Lifecycle
---------

Every das script system can override four lifecycle callbacks:

- **on_init()** -- Called once when the system is first loaded.
- **on_update(dt : float)** -- Called every frame with the delta time.
- **on_event()** -- Called when an engine event occurs (mouse, keyboard, etc.).
- **on_inactive()** -- Called when the system is disabled or unloaded.

Entity Operations
-----------------

.. code-block:: cpp

    let e = entity_create()           // Create a new entity
    entity_destroy(e)                 // Destroy an entity
    let valid = entity_valid(e)       // Check if entity is valid
    let null = null_entity()          // Get the null entity constant

Transforms
----------

.. code-block:: cpp

    add_component(e, Transform())      // Add transform component
    let t = get_component_or(e, Transform())
    t.position = float3(x, y, z)       // Set world position
    t.scale = float3(x, y, z)          // Set scale
    t.rotation = float4(0.0, 0.0, 0.0, 1.0)  // Set rotation (quaternion)

Component System
----------------

.. code-block:: cpp

    let tid = get_component_type_id("Transform")  // Lookup type ID
    let has = has_component(e, Transform())        // Check ownership (typed)
    add_camera(e)                                 // Add camera component
    remove_camera(e)                              // Remove camera component

Events
------

.. code-block:: cpp

    let kind = get_event_kind()
    if kind == EVENT_MOUSE_MOTION
        let dx = get_event_mouse_dx()
        let dy = get_event_mouse_dy()
    if kind == EVENT_MOUSE_BUTTON_DOWN
        let btn = get_event_mouse_button()
