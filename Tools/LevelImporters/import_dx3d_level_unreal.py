import json
import math
import os
from contextlib import nullcontext

import unreal


ENGINE_TO_UNREAL_SCALE = 100.0


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


def _load_static_mesh(type_name):
    for path in STATIC_MESH_CANDIDATES.get(type_name, []):
        mesh = _load_asset(path)

        if mesh:
            return mesh

    return None


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

    _apply_rigid_body(
        actor,
        level_object,
    )

    return actor


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
    default_level_file = os.path.join(
        unreal.Paths.project_dir(),
        "Scene.level",
    )

    import_dx3d_level(default_level_file)
