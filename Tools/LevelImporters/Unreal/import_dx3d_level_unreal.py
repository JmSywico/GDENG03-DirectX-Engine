import json
import math
import os
from contextlib import nullcontext

import unreal


ENGINE_TO_UNREAL_SCALE = 100.0
DEFAULT_MATERIAL_COLOR = [0.72, 0.72, 0.72, 1.0]
DEFAULT_UV_TILING = [1.0, 1.0]
DEFAULT_UV_OFFSET = [0.0, 0.0]
MATERIAL_PACKAGE_PATH = "/Game/DX3DImported/Materials"
LEVEL_PACKAGE_PATH = "/Game/DX3DImported/Levels"
LEVEL_FILE_FILTER = (
    "DX3D Level Files (*.level)|*.level|"
    "JSON Files (*.json)|*.json|"
    "All Files (*.*)|*.*"
)


STATIC_MESH_CANDIDATES = {
    "cube": [
        "/Engine/BasicShapes/Cube.Cube",
    ],
    "plane": [
        "/Engine/BasicShapes/Plane.Plane",
    ],
    "sphere": [
        "/Engine/BasicShapes/Sphere.Sphere",
    ],
    "capsule": [
        "/Engine/BasicShapes/Capsule.Capsule",
        "/Engine/EngineMeshes/Capsule.Capsule",
    ],
}


FALLBACK_TYPE_COLORS = {
    "cube": [0.50, 0.56, 0.62, 1.0],
    "plane": [0.36, 0.42, 0.38, 1.0],
    "sphere": [0.55, 0.65, 0.82, 1.0],
    "capsule": [0.62, 0.54, 0.74, 1.0],
}


SKIPPED_EXPORT_NAME_PARTS = (
    "sky",
    "skysphere",
    "sky_sphere",
    "skyatmosphere",
    "sky_atmosphere",
    "skylight",
    "sky_light",
    "fog",
    "cloud",
    "atmosphere",
)


def _load_asset(path):
    if hasattr(unreal, "load_asset"):
        return unreal.load_asset(path)

    editor_asset_library = getattr(
        unreal,
        "EditorAssetLibrary",
        None,
    )

    if editor_asset_library:
        return editor_asset_library.load_asset(path)

    return None


def _project_directory():
    paths_class = getattr(
        unreal,
        "Paths",
        None,
    )

    if paths_class and hasattr(paths_class, "project_dir"):
        return paths_class.project_dir()

    system_library = getattr(
        unreal,
        "SystemLibrary",
        None,
    )

    if (
        system_library and
        hasattr(system_library, "get_project_directory")
    ):
        return system_library.get_project_directory()

    return os.getcwd()


def _default_level_file_path():
    return os.path.join(
        _project_directory(),
        "Scene.level",
    )


def _get_file_dialog_flags():
    file_dialog_flags = getattr(
        unreal,
        "FileDialogFlags",
        None,
    )

    if file_dialog_flags and hasattr(file_dialog_flags, "NONE"):
        return file_dialog_flags.NONE

    return 0


def _get_desktop_platform():
    desktop_platform_class = getattr(
        unreal,
        "DesktopPlatform",
        None,
    )

    if not desktop_platform_class:
        return None

    try:
        return desktop_platform_class()
    except Exception:
        return desktop_platform_class


def _first_dialog_file(result):
    if not result:
        return ""

    if isinstance(result, tuple):
        if len(result) >= 2:
            accepted = result[0]
            files = result[1]

            if accepted and files:
                return files[0]

            return ""

        return ""

    if isinstance(result, list):
        return result[0] if result else ""

    try:
        return result[0] if len(result) else ""
    except Exception:
        return ""


def _open_level_file_dialog():
    desktop_platform = _get_desktop_platform()

    if (
        not desktop_platform or
        not hasattr(desktop_platform, "open_file_dialog")
    ):
        fallback_path = _default_level_file_path()

        unreal.log_warning(
            "DX3D importer could not open a file dialog in this Unreal version. "
            "Trying {0} instead.".format(fallback_path)
        )
        return fallback_path

    try:
        return _first_dialog_file(
            desktop_platform.open_file_dialog(
                None,
                "Import DX3D .level",
                _project_directory(),
                "",
                LEVEL_FILE_FILTER,
                _get_file_dialog_flags(),
            )
        )
    except Exception as error:
        fallback_path = _default_level_file_path()

        unreal.log_warning(
            "DX3D importer file dialog failed: {0}. "
            "Trying {1} instead.".format(
                error,
                fallback_path,
            )
        )
        return fallback_path


def _save_level_file_dialog():
    desktop_platform = _get_desktop_platform()

    if (
        not desktop_platform or
        not hasattr(desktop_platform, "save_file_dialog")
    ):
        fallback_path = _default_level_file_path()

        unreal.log_warning(
            "DX3D exporter could not open a save dialog in this Unreal version. "
            "Writing {0} instead.".format(fallback_path)
        )
        return fallback_path

    try:
        file_path = _first_dialog_file(
            desktop_platform.save_file_dialog(
                None,
                "Export DX3D .level",
                _project_directory(),
                "Scene.level",
                LEVEL_FILE_FILTER,
                _get_file_dialog_flags(),
            )
        )
    except Exception as error:
        fallback_path = _default_level_file_path()

        unreal.log_warning(
            "DX3D exporter save dialog failed: {0}. "
            "Writing {1} instead.".format(
                error,
                fallback_path,
            )
        )
        return fallback_path

    if file_path and not os.path.splitext(file_path)[1]:
        file_path += ".level"

    return file_path


def _sanitize_asset_name(value):
    result = []

    for character in value:
        if character.isalnum() or character == "_":
            result.append(character)
        else:
            result.append("_")

    name = "".join(result).strip("_")

    if not name:
        name = "DX3D_Level"

    if name[0].isdigit():
        name = "DX3D_" + name

    return name


def _ensure_editor_asset_directory(package_path):
    editor_asset_library = getattr(
        unreal,
        "EditorAssetLibrary",
        None,
    )

    if editor_asset_library:
        editor_asset_library.make_directory(package_path)


def _unreal_asset_exists(package_path):
    editor_asset_library = getattr(
        unreal,
        "EditorAssetLibrary",
        None,
    )

    if not editor_asset_library:
        return False

    asset_name = package_path.rsplit("/", 1)[-1]

    return (
        editor_asset_library.does_asset_exist(package_path) or
        editor_asset_library.does_asset_exist(
            "{0}.{1}".format(package_path, asset_name)
        )
    )


def _make_unique_level_asset_path(level_file):
    base_name = _sanitize_asset_name(
        os.path.splitext(os.path.basename(level_file))[0]
    )

    package_path = "{0}/{1}".format(
        LEVEL_PACKAGE_PATH,
        base_name,
    )

    if not _unreal_asset_exists(package_path):
        return package_path

    suffix = 1

    while True:
        candidate_path = "{0}/{1}_{2}".format(
            LEVEL_PACKAGE_PATH,
            base_name,
            suffix,
        )

        if not _unreal_asset_exists(candidate_path):
            return candidate_path

        suffix += 1


def _create_new_level_for_import(level_file):
    _ensure_editor_asset_directory(LEVEL_PACKAGE_PATH)

    level_asset_path = _make_unique_level_asset_path(level_file)

    level_editor_subsystem_class = getattr(
        unreal,
        "LevelEditorSubsystem",
        None,
    )

    if (
        level_editor_subsystem_class and
        hasattr(unreal, "get_editor_subsystem")
    ):
        level_editor_subsystem = unreal.get_editor_subsystem(
            level_editor_subsystem_class
        )

        if (
            level_editor_subsystem and
            hasattr(level_editor_subsystem, "new_level")
        ):
            if not level_editor_subsystem.new_level(level_asset_path):
                raise RuntimeError(
                    "Unreal could not create level: {0}".format(
                        level_asset_path
                    )
                )

            return level_asset_path

    editor_level_library = getattr(
        unreal,
        "EditorLevelLibrary",
        None,
    )

    if (
        editor_level_library and
        hasattr(editor_level_library, "new_level")
    ):
        if not editor_level_library.new_level(level_asset_path):
            raise RuntimeError(
                "Unreal could not create level: {0}".format(
                    level_asset_path
                )
            )

        return level_asset_path

    raise RuntimeError(
        "No supported Unreal editor level creation API was found."
    )


def _spawn_actor(actor_class, location, rotation):
    editor_actor_subsystem_class = getattr(
        unreal,
        "EditorActorSubsystem",
        None,
    )

    if (
        editor_actor_subsystem_class and
        hasattr(unreal, "get_editor_subsystem")
    ):
        editor_actor_subsystem = unreal.get_editor_subsystem(
            editor_actor_subsystem_class
        )

        if editor_actor_subsystem:
            return editor_actor_subsystem.spawn_actor_from_class(
                actor_class,
                location,
                rotation,
            )

    editor_level_library = getattr(
        unreal,
        "EditorLevelLibrary",
        None,
    )

    if editor_level_library:
        return editor_level_library.spawn_actor_from_class(
            actor_class,
            location,
            rotation,
        )

    raise RuntimeError(
        "No supported Unreal editor actor spawning API was found."
    )


def _name(value):
    name_class = getattr(unreal, "Name", None)

    if name_class:
        return name_class(value)

    return value


def _vec3(values, fallback=(0.0, 0.0, 0.0)):
    if not values or len(values) < 3:
        return fallback

    return (
        float(values[0]),
        float(values[1]),
        float(values[2]),
    )


def _vec4(values, fallback=(1.0, 1.0, 1.0, 1.0)):
    if not values or len(values) < 4:
        return fallback

    return (
        float(values[0]),
        float(values[1]),
        float(values[2]),
        float(values[3]),
    )


def _vec2(values, fallback=(1.0, 1.0)):
    if not values or len(values) < 2:
        return fallback

    return (
        float(values[0]),
        float(values[1]),
    )


def _clamp01(value):
    return max(0.0, min(float(value), 1.0))


def _normalized_color_component(value):
    component = float(value)

    if component > 1.0:
        component /= 255.0

    return _clamp01(component)


def _normalized_color(values, fallback):
    if not values:
        return list(fallback)

    try:
        red = _normalized_color_component(values[0])
        green = _normalized_color_component(values[1])
        blue = _normalized_color_component(values[2])

        alpha = (
            _normalized_color_component(values[3])
            if len(values) > 3
            else 1.0
        )
    except Exception:
        return list(fallback)

    return [
        red,
        green,
        blue,
        alpha,
    ]


def _float_list_tag(values):
    return ",".join("{0:.6g}".format(float(value)) for value in values)


def _parse_float_list_tag(text, expected_count, fallback):
    if not text:
        return list(fallback)

    try:
        values = [
            float(part)
            for part in text.split(",")
        ]
    except Exception:
        return list(fallback)

    if len(values) < expected_count:
        return list(fallback)

    return values[:expected_count]


def _linear_color(color):
    red = _clamp01(color[0])
    green = _clamp01(color[1])
    blue = _clamp01(color[2])
    alpha = _clamp01(color[3] if len(color) > 3 else 1.0)

    if hasattr(unreal, "LinearColor"):
        return unreal.LinearColor(red, green, blue, alpha)

    return unreal.Color(
        int(red * 255.0),
        int(green * 255.0),
        int(blue * 255.0),
        int(alpha * 255.0),
    )


def _level_material(level_object):
    material = level_object.get("material") or {}

    color = list(
        _vec4(
            material.get("color"),
            tuple(DEFAULT_MATERIAL_COLOR),
        )
    )

    return {
        "texture": material.get("texture", "") or "",
        "uvTiling": list(
            _vec2(
                material.get("uvTiling"),
                tuple(DEFAULT_UV_TILING),
            )
        ),
        "uvOffset": list(
            _vec2(
                material.get("uvOffset"),
                tuple(DEFAULT_UV_OFFSET),
            )
        ),
        "color": color,
    }


def _location(values):
    x, y, z = _vec3(values)
    return unreal.Vector(
        z * ENGINE_TO_UNREAL_SCALE,
        x * ENGINE_TO_UNREAL_SCALE,
        y * ENGINE_TO_UNREAL_SCALE,
    )


def _scale(values):
    x, y, z = _vec3(values, (1.0, 1.0, 1.0))
    return unreal.Vector(z, x, y)


def _mesh_scale(values, type_name):
    return _scale(values)


def _is_rigid_body_enabled(level_object):
    rigid_body = level_object.get("rigidBody") or {}
    return bool(rigid_body.get("enabled", False))


def _clean_rotation_angle(value):
    angle = math.fmod(float(value), math.tau)

    if angle > math.pi:
        angle -= math.tau

    if angle < -math.pi:
        angle += math.tau

    right_angle = math.pi * 0.5

    for quarter_turn in range(-4, 5):
        snapped = quarter_turn * right_angle

        if abs(angle - snapped) <= 0.06:
            return snapped

    return angle


def _is_near_angle(value, target):
    return abs(
        _clean_rotation_angle(value) - target
    ) <= 0.06


def _axis_aligned_wall_scale(level_object):
    if (
        (level_object.get("type") or "").lower() != "cube" or
        _is_rigid_body_enabled(level_object)
    ):
        return None

    transform = level_object.get("transform") or {}
    rotation = _vec3(transform.get("rotation"))
    scale = list(
        _vec3(
            transform.get("scale"),
            (1.0, 1.0, 1.0),
        )
    )

    if (
        not _is_near_angle(rotation[0], 0.0) or
        not _is_near_angle(rotation[2], 0.0)
    ):
        return None

    right_angle = math.pi * 0.5
    y_angle = _clean_rotation_angle(rotation[1])
    y_quarter_turn = round(y_angle / right_angle)

    if abs(y_angle - y_quarter_turn * right_angle) > 0.06:
        return None

    horizontal_long_side = max(
        abs(scale[0]),
        abs(scale[2]),
    )
    horizontal_short_side = min(
        abs(scale[0]),
        abs(scale[2]),
    )

    if (
        horizontal_long_side <= 0.0 or
        horizontal_short_side >
        horizontal_long_side * 0.20
    ):
        return None

    if abs(y_quarter_turn) % 2 == 1:
        scale[0], scale[2] = scale[2], scale[0]

    return scale


def _rotation(values):
    x, y, z = _vec3(values)
    return unreal.Rotator(
        math.degrees(_clean_rotation_angle(x)),
        math.degrees(_clean_rotation_angle(y)),
        math.degrees(_clean_rotation_angle(z)),
    )


def _velocity(values):
    x, y, z = _vec3(values)
    return unreal.Vector(
        z * ENGINE_TO_UNREAL_SCALE,
        x * ENGINE_TO_UNREAL_SCALE,
        y * ENGINE_TO_UNREAL_SCALE,
    )


def _angular_velocity(values):
    x, y, z = _vec3(values)
    return unreal.Vector(
        math.degrees(z),
        math.degrees(x),
        math.degrees(y),
    )


def _dx3d_position(vector):
    return [
        float(vector.y) / ENGINE_TO_UNREAL_SCALE,
        float(vector.z) / ENGINE_TO_UNREAL_SCALE,
        float(vector.x) / ENGINE_TO_UNREAL_SCALE,
    ]


def _dx3d_scale(vector):
    return [
        float(vector.y),
        float(vector.z),
        float(vector.x),
    ]


def _dx3d_mesh_scale(vector, type_name):
    return _dx3d_scale(vector)


def _dx3d_rotation(rotator):
    return [
        math.radians(float(rotator.pitch)),
        math.radians(float(rotator.yaw)),
        math.radians(float(rotator.roll)),
    ]


def _dx3d_velocity(vector):
    return [
        float(vector.y) / ENGINE_TO_UNREAL_SCALE,
        float(vector.z) / ENGINE_TO_UNREAL_SCALE,
        float(vector.x) / ENGINE_TO_UNREAL_SCALE,
    ]


def _dx3d_angular_velocity(vector):
    return [
        math.radians(float(vector.y)),
        math.radians(float(vector.z)),
        math.radians(float(vector.x)),
    ]


def _load_static_mesh(type_name):
    for path in STATIC_MESH_CANDIDATES.get(type_name, []):
        mesh = _load_asset(path)

        if mesh:
            return mesh

    return None


def _get_actor_label(actor):
    if hasattr(actor, "get_actor_label"):
        return actor.get_actor_label()

    if hasattr(actor, "get_name"):
        return actor.get_name()

    return str(actor)


def _should_skip_export_actor(actor):
    label = _get_actor_label(actor).lower()

    return any(
        name_part in label
        for name_part in SKIPPED_EXPORT_NAME_PARTS
    )


def _get_actor_tags(actor):
    try:
        tags = actor.get_editor_property("tags")
    except Exception:
        tags = getattr(actor, "tags", [])

    return [str(tag) for tag in tags or []]


def _set_actor_tags(actor, tags):
    try:
        actor.set_editor_property(
            "tags",
            [_name(tag) for tag in tags],
        )
    except Exception:
        try:
            actor.tags = [_name(tag) for tag in tags]
        except Exception:
            pass


def _set_dx3d_tags(actor, type_name, level_object):
    tags = [
        tag for tag in _get_actor_tags(actor)
        if not tag.startswith("DX3D_")
    ]

    tags.append("DX3D_TYPE:{0}".format(type_name))

    if type_name not in ("directionallight", "directional_light", "light"):
        material = _level_material(level_object)
        tags.append(
            "DX3D_COLOR:{0}".format(
                _float_list_tag(material["color"])
            )
        )
        tags.append(
            "DX3D_UV_TILING:{0}".format(
                _float_list_tag(material["uvTiling"])
            )
        )
        tags.append(
            "DX3D_UV_OFFSET:{0}".format(
                _float_list_tag(material["uvOffset"])
            )
        )

        if material["texture"]:
            tags.append("DX3D_TEXTURE:{0}".format(material["texture"]))

    rigid_body = level_object.get("rigidBody") or {}

    if rigid_body.get("enabled", False):
        tags.append("DX3D_RIGIDBODY")

        if rigid_body.get("isStatic", False):
            tags.append("DX3D_STATIC")

    _set_actor_tags(actor, tags)


def _ensure_material_directory():
    editor_asset_library = getattr(
        unreal,
        "EditorAssetLibrary",
        None,
    )

    if (
        editor_asset_library and
        hasattr(editor_asset_library, "does_directory_exist") and
        not editor_asset_library.does_directory_exist(MATERIAL_PACKAGE_PATH)
    ):
        editor_asset_library.make_directory(MATERIAL_PACKAGE_PATH)


def _material_asset_name(color):
    values = [
        int(round(_clamp01(color[index]) * 255.0))
        for index in range(4)
    ]

    return "DX3D_Mat_{0:02X}{1:02X}{2:02X}{3:02X}".format(*values)


def _load_material_asset(name):
    return (
        _load_asset("{0}/{1}.{1}".format(MATERIAL_PACKAGE_PATH, name)) or
        _load_asset("{0}/{1}".format(MATERIAL_PACKAGE_PATH, name))
    )


def _create_color_material(color):
    asset_tools_helpers = getattr(
        unreal,
        "AssetToolsHelpers",
        None,
    )

    material_factory_class = getattr(
        unreal,
        "MaterialFactoryNew",
        None,
    )

    material_class = getattr(
        unreal,
        "Material",
        None,
    )

    material_editing_library = getattr(
        unreal,
        "MaterialEditingLibrary",
        None,
    )

    expression_class = getattr(
        unreal,
        "MaterialExpressionConstant4Vector",
        None,
    )

    material_property_class = getattr(
        unreal,
        "MaterialProperty",
        None,
    )

    if (
        not asset_tools_helpers or
        not material_factory_class or
        not material_class or
        not material_editing_library or
        not expression_class or
        not material_property_class
    ):
        return None

    name = _material_asset_name(color)
    existing_material = _load_material_asset(name)

    if existing_material:
        return existing_material

    _ensure_material_directory()

    try:
        asset_tools = asset_tools_helpers.get_asset_tools()
        material = asset_tools.create_asset(
            name,
            MATERIAL_PACKAGE_PATH,
            material_class,
            material_factory_class(),
        )

        if not material:
            return None

        expression = material_editing_library.create_material_expression(
            material,
            expression_class,
            -400,
            0,
        )

        expression.set_editor_property(
            "constant",
            _linear_color(color),
        )

        material_editing_library.connect_material_property(
            expression,
            "",
            material_property_class.MP_BASE_COLOR,
        )

        material_editing_library.recompile_material(material)

        editor_asset_library = getattr(
            unreal,
            "EditorAssetLibrary",
            None,
        )

        if editor_asset_library and hasattr(
            editor_asset_library,
            "save_loaded_asset",
        ):
            editor_asset_library.save_loaded_asset(material)

        return material
    except Exception as error:
        unreal.log_warning(
            "DX3D importer could not create color material: {0}".format(error)
        )

    return None


def _apply_material(actor, level_object):
    component = getattr(
        actor,
        "static_mesh_component",
        None,
    )

    if not component:
        return

    material_data = _level_material(level_object)
    material = _create_color_material(material_data["color"])

    if material and hasattr(component, "set_material"):
        component.set_material(0, material)


def _get_dx3d_tag_value(actor, prefix):
    for tag in _get_actor_tags(actor):
        if tag.startswith(prefix):
            return tag[len(prefix):]

    return None


def _get_all_level_actors():
    editor_actor_subsystem_class = getattr(
        unreal,
        "EditorActorSubsystem",
        None,
    )

    if (
        editor_actor_subsystem_class and
        hasattr(unreal, "get_editor_subsystem")
    ):
        editor_actor_subsystem = unreal.get_editor_subsystem(
            editor_actor_subsystem_class
        )

        if (
            editor_actor_subsystem and
            hasattr(editor_actor_subsystem, "get_all_level_actors")
        ):
            return editor_actor_subsystem.get_all_level_actors()

    editor_level_library = getattr(
        unreal,
        "EditorLevelLibrary",
        None,
    )

    if (
        editor_level_library and
        hasattr(editor_level_library, "get_all_level_actors")
    ):
        return editor_level_library.get_all_level_actors()

    return []


def _enum_value(enum_class, *names):
    if not enum_class:
        return None

    for name in names:
        if hasattr(enum_class, name):
            return getattr(enum_class, name)

    return None


def _set_mesh_mobility(component, movable):
    mobility_class = getattr(
        unreal,
        "ComponentMobility",
        None,
    )

    mobility = _enum_value(
        mobility_class,
        "MOVABLE" if movable else "STATIC",
        "Movable" if movable else "Static",
    )

    if mobility is None:
        return

    if hasattr(component, "set_mobility"):
        try:
            component.set_mobility(mobility)
            return
        except Exception:
            pass

    if hasattr(component, "set_editor_property"):
        try:
            component.set_editor_property(
                "mobility",
                mobility,
            )
        except Exception:
            pass


def _set_mesh_collision(component, profile_name):
    if hasattr(component, "set_collision_enabled"):
        component.set_collision_enabled(
            unreal.CollisionEnabled.QUERY_AND_PHYSICS
        )

    if hasattr(component, "set_collision_profile_name"):
        try:
            component.set_collision_profile_name(
                profile_name
            )
            return
        except Exception:
            pass

    if hasattr(component, "set_editor_property"):
        try:
            component.set_editor_property(
                "collision_profile_name",
                _name(profile_name),
            )
        except Exception:
            pass


def _spawn_static_mesh_actor(level_object, type_name):
    transform = level_object.get("transform") or {}
    mesh = _load_static_mesh(type_name)

    if not mesh:
        unreal.log_warning(
            "DX3D importer skipped '{0}' because no Unreal mesh was found for type '{1}'.".format(
                level_object.get("name", type_name),
                type_name,
            )
        )
        return None

    wall_scale = _axis_aligned_wall_scale(
        level_object
    )

    actor = _spawn_actor(
        unreal.StaticMeshActor,
        _location(transform.get("position")),
        unreal.Rotator(0.0, 0.0, 0.0)
        if wall_scale
        else _rotation(transform.get("rotation")),
    )

    actor.set_actor_scale3d(
        _mesh_scale(
            wall_scale
            if wall_scale
            else transform.get("scale"),
            type_name,
        )
    )

    component = actor.static_mesh_component
    component.set_static_mesh(mesh)

    if hasattr(actor, "set_actor_enable_collision"):
        actor.set_actor_enable_collision(True)

    _set_mesh_collision(
        component,
        "BlockAll",
    )

    return actor


def _spawn_directional_light(level_object):
    transform = level_object.get("transform") or {}
    light_data = level_object.get("light") or {}

    actor = _spawn_actor(
        unreal.DirectionalLight,
        _location(transform.get("position")),
        _rotation(transform.get("rotation")),
    )

    component = actor.get_component_by_class(
        unreal.DirectionalLightComponent
    )

    if component:
        color = _vec3(
            light_data.get("color"),
            (1.0, 1.0, 1.0),
        )

        if hasattr(unreal, "LinearColor"):
            light_color = unreal.LinearColor(
                float(color[0]),
                float(color[1]),
                float(color[2]),
                1.0,
            )
        else:
            light_color = unreal.Color(
                int(max(0.0, min(color[0], 1.0)) * 255.0),
                int(max(0.0, min(color[1], 1.0)) * 255.0),
                int(max(0.0, min(color[2], 1.0)) * 255.0),
                255,
            )

        component.set_editor_property(
            "light_color",
            light_color,
        )

        component.set_editor_property(
            "intensity",
            float(light_data.get("intensity", 1.0)) * 10.0,
        )

        component.set_editor_property(
            "cast_shadows",
            bool(light_data.get("castShadows", True)),
        )

    return actor


def _is_directional_light_object(level_object):
    type_name = (
        level_object.get("type") or ""
    ).lower()

    return type_name in (
        "directionallight",
        "directional_light",
        "light",
    )


def _spawn_fallback_directional_light():
    level_object = {
        "name": "DX3D Default Directional Light",
        "type": "directionalLight",
        "transform": {
            "position": [0.0, 6.0, -4.0],
            "rotation": [-0.85, -0.35, 0.0],
            "scale": [1.0, 1.0, 1.0],
        },
        "light": {
            "color": [1.0, 0.96, 0.90],
            "intensity": 1.0,
            "ambientStrength": 0.2,
            "shadowArea": 30.0,
            "castShadows": True,
        },
    }

    actor = _spawn_directional_light(
        level_object
    )

    if actor:
        actor.set_actor_label(
            level_object["name"]
        )

    return actor


def _apply_rigid_body(actor, level_object):
    rigid_body = level_object.get("rigidBody") or {}

    if not rigid_body.get("enabled", False):
        return

    component = getattr(actor, "static_mesh_component", None)

    if not component:
        return

    is_static = bool(rigid_body.get("isStatic", False))

    if hasattr(actor, "set_actor_enable_collision"):
        actor.set_actor_enable_collision(True)

    _set_mesh_mobility(
        component,
        not is_static,
    )

    _set_mesh_collision(
        component,
        "BlockAll" if is_static else "PhysicsActor",
    )

    if hasattr(component, "set_simulate_physics"):
        component.set_simulate_physics(
            not is_static
        )

    if hasattr(component, "set_enable_gravity"):
        component.set_enable_gravity(
            bool(rigid_body.get("useGravity", True))
        )

    if hasattr(component, "set_mass_override_in_kg"):
        component.set_mass_override_in_kg(
            _name(""),
            float(rigid_body.get("mass", 1.0)),
            True,
        )

    if hasattr(component, "recreate_physics_state"):
        try:
            component.recreate_physics_state()
        except Exception:
            pass

    if not is_static:
        if hasattr(component, "set_physics_linear_velocity"):
            component.set_physics_linear_velocity(
                _velocity(rigid_body.get("velocity")),
                False,
            )

        if hasattr(
            component,
            "set_physics_angular_velocity_in_degrees",
        ):
            component.set_physics_angular_velocity_in_degrees(
                _angular_velocity(rigid_body.get("angularVelocity")),
                False,
            )

        if hasattr(component, "wake_all_rigid_bodies"):
            component.wake_all_rigid_bodies()


def _spawn_object(level_object):
    type_name = (
        level_object.get("type") or ""
    ).lower()

    if type_name in ("directionallight", "directional_light", "light"):
        actor = _spawn_directional_light(level_object)
    else:
        actor = _spawn_static_mesh_actor(
            level_object,
            type_name,
        )

    if not actor:
        return None

    label = level_object.get("name") or type_name
    actor.set_actor_label(label)

    _set_dx3d_tags(
        actor,
        type_name,
        level_object,
    )

    _apply_material(
        actor,
        level_object,
    )

    _apply_rigid_body(
        actor,
        level_object,
    )

    return actor


def _infer_actor_type(actor):
    tagged_type = _get_dx3d_tag_value(
        actor,
        "DX3D_TYPE:",
    )

    if tagged_type:
        return tagged_type

    if actor.get_component_by_class(
        unreal.DirectionalLightComponent
    ):
        return "directionalLight"

    component = getattr(
        actor,
        "static_mesh_component",
        None,
    )

    if not component:
        return None

    text = _get_actor_label(actor).lower()

    try:
        mesh = component.get_editor_property(
            "static_mesh"
        )
    except Exception:
        mesh = getattr(component, "static_mesh", None)

    if mesh:
        if hasattr(mesh, "get_name"):
            text += " " + mesh.get_name().lower()

        if hasattr(mesh, "get_path_name"):
            text += " " + mesh.get_path_name().lower()

    if "capsule" in text:
        return "capsule"

    if "sphere" in text:
        return "sphere"

    if "plane" in text:
        return "plane"

    if "cube" in text:
        return "cube"

    return None


def _safe_component_call(component, method_name, fallback):
    if not component or not hasattr(component, method_name):
        return fallback

    try:
        return getattr(component, method_name)()
    except Exception:
        return fallback


def _component_bool_property(component, property_name, fallback):
    try:
        return bool(component.get_editor_property(property_name))
    except Exception:
        return fallback


def _color_from_unreal_value(value):
    if not value:
        return None

    red = getattr(value, "r", getattr(value, "red", None))
    green = getattr(value, "g", getattr(value, "green", None))
    blue = getattr(value, "b", getattr(value, "blue", None))
    alpha = getattr(value, "a", getattr(value, "alpha", 1.0))

    if red is None or green is None or blue is None:
        return None

    return _normalized_color(
        [
            red,
            green,
            blue,
            alpha,
        ],
        DEFAULT_MATERIAL_COLOR,
    )


def _read_material_color(material):
    if not material:
        return None

    if hasattr(material, "get_vector_parameter_value"):
        for parameter_name in (
            "BaseColor",
            "Base Color",
            "Color",
            "Tint",
        ):
            try:
                color = material.get_vector_parameter_value(
                    _name(parameter_name)
                )

                color = _color_from_unreal_value(color)

                if color:
                    return color
            except Exception:
                pass

    if hasattr(material, "get_editor_property"):
        for property_name in (
            "base_color",
            "diffuse_color",
            "color",
        ):
            try:
                color = material.get_editor_property(
                    property_name
                )

                color = _color_from_unreal_value(color)

                if color:
                    return color
            except Exception:
                pass

    return None


def _fallback_material_color(type_name, actor):
    color = list(
        FALLBACK_TYPE_COLORS.get(
            type_name,
            DEFAULT_MATERIAL_COLOR,
        )
    )

    label = _get_actor_label(actor)
    variation = (sum(ord(ch) for ch in label) % 5) * 0.035

    color[0] = _clamp01(color[0] + variation)
    color[1] = _clamp01(color[1] + variation * 0.5)
    color[2] = _clamp01(color[2] - variation * 0.35)

    return color


def _export_transform(actor, type_name):
    return {
        "position": _dx3d_position(actor.get_actor_location()),
        "rotation": _dx3d_rotation(actor.get_actor_rotation()),
        "scale": _dx3d_mesh_scale(
            actor.get_actor_scale3d(),
            type_name,
        ),
    }


def _export_light(actor):
    component = actor.get_component_by_class(
        unreal.DirectionalLightComponent
    )

    if not component:
        return None

    try:
        color = component.get_editor_property(
            "light_color"
        )
    except Exception:
        color = None

    color = _color_from_unreal_value(color)

    if not color:
        color = [1.0, 1.0, 1.0, 1.0]

    try:
        intensity = float(component.get_editor_property("intensity")) / 10.0
    except Exception:
        intensity = 1.0

    try:
        cast_shadows = bool(component.get_editor_property("cast_shadows"))
    except Exception:
        cast_shadows = True

    return {
        "color": color[:3],
        "intensity": intensity,
        "ambientStrength": 0.2,
        "shadowArea": 30.0,
        "castShadows": cast_shadows,
    }


def _export_material(actor, type_name):
    component = getattr(
        actor,
        "static_mesh_component",
        None,
    )

    if not component:
        return {
            "texture": "",
            "uvTiling": list(DEFAULT_UV_TILING),
            "uvOffset": list(DEFAULT_UV_OFFSET),
            "color": _fallback_material_color(
                type_name,
                actor,
            ),
        }

    tagged_color = _get_dx3d_tag_value(actor, "DX3D_COLOR:")
    tagged_texture = _get_dx3d_tag_value(actor, "DX3D_TEXTURE:") or ""
    tagged_uv_tiling = _get_dx3d_tag_value(actor, "DX3D_UV_TILING:")
    tagged_uv_offset = _get_dx3d_tag_value(actor, "DX3D_UV_OFFSET:")

    if tagged_color:
        return {
            "texture": tagged_texture,
            "uvTiling": _parse_float_list_tag(
                tagged_uv_tiling,
                2,
                DEFAULT_UV_TILING,
            ),
            "uvOffset": _parse_float_list_tag(
                tagged_uv_offset,
                2,
                DEFAULT_UV_OFFSET,
            ),
            "color": _parse_float_list_tag(
                tagged_color,
                4,
                DEFAULT_MATERIAL_COLOR,
            ),
        }

    color = _fallback_material_color(
        type_name,
        actor,
    )

    try:
        material = component.get_material(0)
    except Exception:
        material = None

    material_color = _read_material_color(material)

    if material_color:
        color = material_color

    return {
        "texture": "",
        "uvTiling": list(DEFAULT_UV_TILING),
        "uvOffset": list(DEFAULT_UV_OFFSET),
        "color": color,
    }


def _export_rigid_body(actor):
    component = getattr(
        actor,
        "static_mesh_component",
        None,
    )

    if not component:
        return {
            "enabled": False,
        }

    has_rigid_body_tag = "DX3D_RIGIDBODY" in _get_actor_tags(actor)

    simulate_physics = bool(
        _safe_component_call(
            component,
            "is_simulating_physics",
            False,
        )
    )

    if not has_rigid_body_tag and not simulate_physics:
        return {
            "enabled": False,
        }

    is_static = (
        "DX3D_STATIC" in _get_actor_tags(actor) or
        not simulate_physics
    )

    linear_velocity = _safe_component_call(
        component,
        "get_physics_linear_velocity",
        unreal.Vector(0.0, 0.0, 0.0),
    )

    angular_velocity = _safe_component_call(
        component,
        "get_physics_angular_velocity_in_degrees",
        unreal.Vector(0.0, 0.0, 0.0),
    )

    mass = _safe_component_call(
        component,
        "get_mass",
        1.0,
    )

    use_gravity = _component_bool_property(
        component,
        "enable_gravity",
        True,
    )

    return {
        "enabled": True,
        "velocity": _dx3d_velocity(linear_velocity),
        "angularVelocity": _dx3d_angular_velocity(angular_velocity),
        "mass": max(float(mass), 0.001),
        "restitution": 0.45,
        "friction": 0.20,
        "useGravity": bool(use_gravity),
        "isStatic": bool(is_static),
        "collider": {
            "shape": "box",
            "size": [1.0, 1.0, 1.0],
            "offset": [0.0, 0.0, 0.0],
            "matchRenderScale": True,
        },
    }


def _export_actor(actor):
    if _should_skip_export_actor(actor):
        return None

    type_name = _infer_actor_type(actor)

    if not type_name:
        return None

    level_object = {
        "name": _get_actor_label(actor),
        "type": type_name,
        "transform": _export_transform(
            actor,
            type_name,
        ),
    }

    if type_name == "directionalLight":
        level_object["light"] = _export_light(actor)
    else:
        level_object["material"] = _export_material(
            actor,
            type_name,
        )

    level_object["rigidBody"] = _export_rigid_body(actor)

    return level_object


def export_dx3d_level(level_file):
    actors = _get_all_level_actors()
    objects = []

    for actor in actors:
        level_object = _export_actor(actor)

        if level_object:
            objects.append(level_object)

        if len(objects) >= 10000:
            break

    level = {
        "format": "DX3D_LEVEL",
        "version": 1,
        "objects": objects,
    }

    with open(level_file, "w", encoding="utf-8") as file:
        json.dump(level, file, indent=2)

    unreal.log(
        "DX3D exporter wrote {0} object(s) to {1}.".format(
            len(objects),
            level_file,
        )
    )


def import_dx3d_level(level_file):
    if not os.path.exists(level_file):
        raise RuntimeError(
            "DX3D .level file does not exist: {0}".format(level_file)
        )

    with open(level_file, "r", encoding="utf-8") as file:
        level = json.load(file)

    objects = level.get("objects") or []

    transaction_class = getattr(
        unreal,
        "ScopedEditorTransaction",
        None,
    )

    transaction = (
        transaction_class("Import DX3D .level")
        if transaction_class
        else nullcontext()
    )

    with transaction:
        imported_level_path = _create_new_level_for_import(level_file)
        imported_count = 0
        added_fallback_light = False

        for level_object in objects[:10000]:
            if _spawn_object(level_object):
                imported_count += 1

        if not any(
            _is_directional_light_object(level_object)
            for level_object in objects
        ):
            if _spawn_fallback_directional_light():
                imported_count += 1
                added_fallback_light = True

        if added_fallback_light:
            unreal.log(
                "DX3D importer added a default directional light because the .level file did not contain one."
            )

        unreal.log(
            "DX3D importer created {0} actor(s) in {1} from {2}.".format(
                imported_count,
                imported_level_path,
                level_file,
            )
        )


def import_dx3d_level_from_dialog():
    level_file = _open_level_file_dialog()

    if not level_file:
        return

    if not os.path.exists(level_file):
        unreal.log_warning(
            "DX3D importer could not find {0}. "
            "Copy Scene.level into your Unreal project folder, or run "
            "import_dx3d_level(r\"C:/path/to/Scene.level\") with the full path.".format(
                level_file
            )
        )
        return

    import_dx3d_level(level_file)


def export_dx3d_level_from_dialog():
    level_file = _save_level_file_dialog()

    if not level_file:
        return

    export_dx3d_level(level_file)


def _python_command(function_name):
    module_name = globals().get(
        "__name__",
        "__main__",
    )

    if module_name == "__main__":
        return "{0}()".format(function_name)

    return (
        "import {0}; "
        "{0}.{1}()"
    ).format(
        module_name,
        function_name,
    )


def _add_tool_menu_entry(
    menu,
    section_name,
    name,
    label,
    tool_tip,
    function_name,
):
    tool_menu_entry_class = getattr(
        unreal,
        "ToolMenuEntry",
        None,
    )

    multi_block_type = getattr(
        unreal,
        "MultiBlockType",
        None,
    )

    string_command_type = getattr(
        unreal,
        "ToolMenuStringCommandType",
        None,
    )

    if (
        not tool_menu_entry_class or
        not multi_block_type or
        not string_command_type
    ):
        return False

    entry = tool_menu_entry_class(
        name=name,
        type=multi_block_type.MENU_ENTRY,
    )

    entry.set_label(label)
    entry.set_tool_tip(tool_tip)
    entry.set_string_command(
        string_command_type.PYTHON,
        "",
        _python_command(function_name),
    )

    menu.add_menu_entry(
        section_name,
        entry,
    )

    return True


def install_unreal_editor_menu():
    tool_menus_class = getattr(
        unreal,
        "ToolMenus",
        None,
    )

    if not tool_menus_class:
        unreal.log_warning(
            "DX3D menu was not installed because Unreal ToolMenus is unavailable."
        )
        return False

    menus = tool_menus_class.get()
    tools_menu = menus.extend_menu(
        "LevelEditor.MainMenu.Tools"
    )

    section_name = "DX3D"

    try:
        tools_menu.add_section(
            section_name,
            "DX3D",
        )
    except Exception:
        pass

    installed_import = _add_tool_menu_entry(
        tools_menu,
        section_name,
        "DX3DImportLevel",
        "Import DX3D .level...",
        "Create a fresh Unreal level and import a DX3D .level file.",
        "import_dx3d_level_from_dialog",
    )

    installed_export = _add_tool_menu_entry(
        tools_menu,
        section_name,
        "DX3DExportLevel",
        "Export DX3D .level...",
        "Export supported actors from the current Unreal level to a DX3D .level file.",
        "export_dx3d_level_from_dialog",
    )

    if hasattr(menus, "refresh_all_widgets"):
        menus.refresh_all_widgets()

    if installed_import and installed_export:
        unreal.log(
            "DX3D import/export entries installed under the Tools menu."
        )
        return True

    unreal.log_warning(
        "DX3D menu could not install all entries in this Unreal version."
    )
    return False


if __name__ == "__main__":
    install_unreal_editor_menu()

    unreal.log(
        "DX3D level tools loaded. Use Tools > DX3D, or run "
        "import_dx3d_level(r\"C:/path/to/Scene.level\") / "
        "export_dx3d_level(r\"C:/path/to/Scene.level\")."
    )
