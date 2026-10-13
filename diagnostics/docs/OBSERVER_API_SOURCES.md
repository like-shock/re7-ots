# API sources checked during development (2026-10-13)

- https://refdocs.praydog.com/api/sdk.html — get_primary_camera.
- https://refdocs.praydog.com/api/types/REManagedObject.html — get_type_definition, get_address, call; managed wrapper reference handling.
- https://refdocs.praydog.com/api/types/RETypeDefinition.html — get_method/get_parent_type/is_a.
- https://refdocs.praydog.com/api/types/RETransform.html — get_position/get_rotation.
- https://refdocs.praydog.com/api/re.html — on_application_entry/on_frame/on_draw_ui/on_script_reset.
- https://refdocs.praydog.com/examples/Example-Snippets.html — documented get_Transform usage, os.clock availability.
- https://refdocs.praydog.com/api/json.html — load_file/dump_string.
- https://refdocs.praydog.com/api/fs.html — relative io.open under reframework/data, safety wrapping.

Actual user REFramework distribution version and exact runtime method support remain to be observed. Native prefix ABI reused unchanged from the user's working diagnostic; no native SDK layout extension was introduced. get_Transform is checked through the runtime type hierarchy before calling; an unavailable method stays UNKNOWN.

## v0.2.1 camera getter correction

- https://github.com/originalnicodr/RELit/blob/main/RELit.lua — `move_light_to_camera` resolves primary Camera -> `get_GameObject` -> `get_Transform`; only the accessor relationship was consulted. No RELit source is redistributed by this patch.
- https://github.com/praydog/REFramework/blob/master/src/mods/FreeCam.cpp — framework source uses Camera -> GameObject -> Transform.

The user's v0.2.0 trace returned UNKNOWN_TRANSFORM_METHOD for every camera sample. v0.2.0 incorrectly searched for get_Transform on the Camera component itself. The corrected adapter checks the inherited get_GameObject method on Camera, then get_Transform on the returned GameObject. Runtime camera validation is pending a new trace.
