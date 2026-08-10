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
        x * ENGINE_TO_UNREAL_SCALE,
        z * ENGINE_TO_UNREAL_SCALE,
        y * ENGINE_TO_UNREAL_SCALE,
    )


def _scale(values):
    x, y, z = _vec3(values, (1.0, 1.0, 1.0))
    return unreal.Vector(x, z, y)


def _rotation(values):
    x, y, z = _vec3(values)
    return unreal.Rotator(
        math.degrees(x),
        math.degrees(z),
        math.degrees(y),
    )


def _velocity(values):
    x, y, z = _vec3(values)
    return unreal.Vector(
        x * ENGINE_TO_UNREAL_SCALE,
        z * ENGINE_TO_UNREAL_SCALE,
        y * ENGINE_TO_UNREAL_SCALE,
    )


def _angular_velocity(values):
    x, y, z = _vec3(values)
    return unreal.Vector(
        math.degrees(x),
        math.degrees(z),
        math.degrees(y),
    )


def _dx3d_position(vector):
    return [
        float(vector.x) / ENGINE_TO_UNREAL_SCALE,
        float(vector.z) / ENGINE_TO_UNREAL_SCALE,
        float(vector.y) / ENGINE_TO_UNREAL_SCALE,
    ]


def _dx3d_scale(vector):
    return [
        float(vector.x),
        float(vector.z),
        float(vector.y),
    ]


def _dx3d_rotation(rotator):
    return [
        math.radians(float(rotator.pitch)),
        math.radians(float(rotator.roll)),
        math.radians(float(rotator.yaw)),
    ]


def _dx3d_velocity(vector):
    return [
        float(vector.x) / ENGINE_TO_UNREAL_SCALE,
        float(vector.z) / ENGINE_TO_UNREAL_SCALE,
        float(vector.y) / ENGINE_TO_UNREAL_SCALE,
    ]


def _dx3d_angular_velocity(vector):
    return [
        math.radians(float(vector.x)),
        math.radians(float(vector.z)),
        math.radians(float(vector.y)),
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

    actor = _spawn_actor(
        unreal.StaticMeshActor,
        _location(transform.get("position")),
        _rotation(transform.get("rotation")),
    )

    actor.set_actor_scale3d(
        _scale(transform.get("scale"))
    )

    component = actor.static_mesh_component
    component.set_static_mesh(mesh)

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


def _apply_rigid_body(actor, level_object):
    rigid_body = level_object.get("rigidBody") or {}

    if not rigid_body.get("enabled", False):
        return

    component = getattr(actor, "static_mesh_component", None)

    if not component:
        return

    if hasattr(component, "set_collision_enabled"):
        component.set_collision_enabled(
            unreal.CollisionEnabled.QUERY_AND_PHYSICS
        )

    is_static = bool(rigid_body.get("isStatic", False))

    if hasattr(component, "set_simulate_physics"):
        component.set_simulate_physics(not is_static)

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


def _export_transform(actor):
    return {
        "position": _dx3d_position(actor.get_actor_location()),
        "rotation": _dx3d_rotation(actor.get_actor_rotation()),
        "scale": _dx3d_scale(actor.get_actor_scale3d()),
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

    if color:
        red = float(getattr(color, "r", getattr(color, "red", 1.0)))
        green = float(getattr(color, "g", getattr(color, "green", 1.0)))
        blue = float(getattr(color, "b", getattr(color, "blue", 1.0)))
    else:
        red = green = blue = 1.0

    try:
        intensity = float(component.get_editor_property("intensity")) / 10.0
    except Exception:
        intensity = 1.0

    try:
        cast_shadows = bool(component.get_editor_property("cast_shadows"))
    except Exception:
        cast_shadows = True

    return {
        "color": [red, green, blue],
        "intensity": intensity,
        "ambientStrength": 0.2,
        "shadowArea": 30.0,
        "castShadows": cast_shadows,
    }


def _export_material(actor):
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
            "color": list(DEFAULT_MATERIAL_COLOR),
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

    color = list(DEFAULT_MATERIAL_COLOR)

    try:
        material = component.get_material(0)
    except Exception:
        material = None

    if material and hasattr(material, "get_editor_property"):
        try:
            material_color = material.get_editor_property("base_color")
            color = [
                float(material_color.r),
                float(material_color.g),
                float(material_color.b),
                float(material_color.a),
            ]
        except Exception:
            pass

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
    type_name = _infer_actor_type(actor)

    if not type_name:
        return None

    level_object = {
        "name": _get_actor_label(actor),
        "type": type_name,
        "transform": _export_transform(actor),
    }

    if type_name == "directionalLight":
        level_object["light"] = _export_light(actor)
    else:
        level_object["material"] = _export_material(actor)

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
        imported_count = 0

        for level_object in objects[:10000]:
            if _spawn_object(level_object):
                imported_count += 1

        unreal.log(
            "DX3D importer created {0} actor(s) from {1}.".format(
                imported_count,
                level_file,
            )
        )


if __name__ == "__main__":
    unreal.log(
        "DX3D level tools loaded. Run import_dx3d_level(r\"C:/path/to/Scene.level\") or export_dx3d_level(r\"C:/path/to/Scene.level\")."
    )
