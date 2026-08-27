.. _weasel-api:

Weasel Daslang API
==================

This reference is generated from the runtime registration site
(``src/wsl/das/wsl_api_module.cpp``). It lists every function
exposed to Daslang; nothing here is hand-written.

Query
-----

- ``find_entity_by_name(name)`` — *read*
- ``get_active_camera()`` — *read*
- ``get_camera_aspect_ratio(camera)`` — *read*
- ``get_camera_far(camera)`` — *read*
- ``get_camera_fov(camera)`` — *read*
- ``get_camera_near(camera)`` — *read*
- ``get_component_type_id(display_name)`` — *read*
- ``get_editor_img_min_x()`` — *read*
- ``get_editor_img_min_y()`` — *read*
- ``get_editor_img_size_x()`` — *read*
- ``get_editor_img_size_y()`` — *read*
- ``get_elapsed_time()`` — *read*
- ``get_hit_x()`` — *read*
- ``get_hit_y()`` — *read*
- ``get_hit_z()`` — *read*
- ``get_ray_dir_x()`` — *read*
- ``get_ray_dir_y()`` — *read*
- ``get_ray_dir_z()`` — *read*
- ``get_ray_origin_x()`` — *read*
- ``get_ray_origin_y()`` — *read*
- ``get_ray_origin_z()`` — *read*
- ``get_rigid_body_mass(proxy, at)`` — *read*
- ``get_time()`` — *read*
- ``get_window_height()`` — *read*
- ``get_window_width()`` — *read*
- ``has_component(type_id, entity)`` — *read*
- ``is_key_pressed(scancode)`` — *read*

Mutation
--------

- ``add_component()`` — *write*
- ``apply_force(entity, x, y, z)`` — *write*
- ``apply_impulse(entity, x, y, z)`` — *write*
- ``hide_cursor()`` — *write*
- ``refresh_editor_viewport()`` — *write*
- ``refresh_window_size()`` — *write*
- ``remove_component()`` — *write*
- ``set_active_camera(entity)`` — *write*
- ``set_camera_aspect_ratio(camera, aspect)`` — *write*
- ``set_camera_far(camera, far_val)`` — *write*
- ``set_camera_fov(camera, fov)`` — *write*
- ``set_camera_near(camera, near_val)`` — *write*
- ``set_entity_name(entity, name)`` — *write*
- ``set_model(entity, path)`` — *write*
- ``set_model_material_override(entity, path)`` — *write*
- ``set_model_visibility_range(entity, range)`` — *write*
- ``set_relative_mouse_mode(enabled)`` — *write*
- ``show_cursor()`` — *write*

Action
------

- ``TYPE_AREA_3D()`` — *unknown*
- ``TYPE_AUDIO()`` — *unknown*
- ``TYPE_CAMERA()`` — *unknown*
- ``TYPE_CAMERA_2D()`` — *unknown*
- ``TYPE_CHARACTER_BODY()`` — *unknown*
- ``TYPE_DIRECTIONAL_LIGHT()`` — *unknown*
- ``TYPE_HIERARCHY()`` — *unknown*
- ``TYPE_MODEL_INSTANCE_3D()`` — *unknown*
- ``TYPE_POINT_LIGHT()`` — *unknown*
- ``TYPE_PREFAB_INSTANCE()`` — *unknown*
- ``TYPE_RIGID_BODY()`` — *unknown*
- ``TYPE_SPOT_LIGHT()`` — *unknown*
- ``TYPE_SPRITE_2D()`` — *unknown*
- ``TYPE_SUBVIEWPORT()`` — *unknown*
- ``TYPE_TRANSFORM()`` — *unknown*
- ``TYPE_TRANSFORM_2D()`` — *unknown*
- ``TYPE_WORLD_TRANSFORM()`` — *unknown*
- ``_get_component_data(entity, type_id)`` — *read*
- ``_get_component_type_id_by_name(type_name)`` — *read*
- ``audio_pause(entity)`` — *write*
- ``audio_play(entity)`` — *write*
- ``audio_resume(entity)`` — *write*
- ``audio_set_volume(entity, volume)`` — *write*
- ``audio_stop(entity)`` — *write*
- ``cursor_visible()`` — *read*
- ``each_entity_id_with(type_ids, blk, context, at)`` — *unknown*
- ``entity_create()`` — *write*
- ``entity_destroy()`` — *write*
- ``entity_is_null(entity)`` — *unknown*
- ``entity_valid(entity)`` — *read*
- ``event_connect(event_name, system_name, handler_name)`` — *write*
- ``event_declare_sink(event_name, system_name, handler_name)`` — *write*
- ``event_declare_source(event_name, system_name)`` — *write*
- ``event_disconnect(event_name, system_name, handler_name)`` — *write*
- ``instantiate_prefab(path)`` — *write*
- ``log_debug(msg)`` — *write*
- ``log_error(msg)`` — *write*
- ``log_info(msg)`` — *write*
- ``log_warn(msg)`` — *write*
- ``make_pick_ray()`` — *write*
- ``null_entity()`` — *unknown*
- ``ray_plane_intersect()`` — *write*

