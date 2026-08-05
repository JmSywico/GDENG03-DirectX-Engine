#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class ImportDX3DLevelUnity
{
    [MenuItem("Tools/DX3D/Import .level")]
    private static void ImportLevelFromMenu()
    {
        var path = EditorUtility.OpenFilePanel(
            "Import DX3D .level",
            "",
            "level");

        if (string.IsNullOrEmpty(path))
            return;

        ImportLevel(path);
    }

    public static void ImportLevel(string path)
    {
        if (!File.Exists(path))
            throw new FileNotFoundException(path);

        var level = JsonUtility.FromJson<LevelFile>(
            File.ReadAllText(path));

        if (level == null || level.objects == null)
            throw new InvalidDataException("Invalid DX3D .level file.");

        var undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Import DX3D .level");

        var root = new GameObject(
            Path.GetFileNameWithoutExtension(path));

        Undo.RegisterCreatedObjectUndo(
            root,
            "Create DX3D Level Root");

        foreach (var levelObject in level.objects)
        {
            var importedObject = CreateObject(levelObject);

            if (importedObject == null)
                continue;

            Undo.RegisterCreatedObjectUndo(
                importedObject,
                "Create DX3D Object");

            importedObject.transform.SetParent(
                root.transform,
                true);
        }

        Undo.CollapseUndoOperations(undoGroup);
    }

    private static GameObject CreateObject(LevelObject levelObject)
    {
        if (levelObject == null)
            return null;

        var type = (levelObject.type ?? string.Empty).ToLowerInvariant();
        GameObject gameObject;

        switch (type)
        {
            case "cube":
                gameObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
                break;

            case "plane":
                gameObject = GameObject.CreatePrimitive(PrimitiveType.Plane);
                break;

            case "sphere":
                gameObject = GameObject.CreatePrimitive(PrimitiveType.Sphere);
                break;

            case "capsule":
                gameObject = GameObject.CreatePrimitive(PrimitiveType.Capsule);
                break;

            case "directionallight":
            case "directional_light":
            case "light":
                gameObject = new GameObject();
                gameObject.AddComponent<Light>().type = LightType.Directional;
                break;

            default:
                return null;
        }

        gameObject.name = string.IsNullOrEmpty(levelObject.name)
            ? levelObject.type
            : levelObject.name;

        ApplyTransform(
            gameObject,
            levelObject,
            type);

        ApplyLight(
            gameObject,
            levelObject);

        ApplyRigidBody(
            gameObject,
            levelObject);

        return gameObject;
    }

    private static void ApplyTransform(
        GameObject gameObject,
        LevelObject levelObject,
        string type)
    {
        var transform = levelObject.transform;

        var position = ToVector3(
            transform?.position,
            Vector3.zero);

        var rotationRadians = ToVector3(
            transform?.rotation,
            Vector3.zero);

        var scale = ToVector3(
            transform?.scale,
            Vector3.one);

        gameObject.transform.position = position;
        gameObject.transform.eulerAngles =
            rotationRadians * Mathf.Rad2Deg;

        if (type == "plane")
        {
            gameObject.transform.localScale =
                new Vector3(
                    scale.x / 10.0f,
                    scale.y,
                    scale.z / 10.0f);
        }
        else
        {
            gameObject.transform.localScale = scale;
        }
    }

    private static void ApplyLight(
        GameObject gameObject,
        LevelObject levelObject)
    {
        var light = gameObject.GetComponent<Light>();

        if (light == null || levelObject.light == null)
            return;

        var color = ToVector3(
            levelObject.light.color,
            Vector3.one);

        light.color = new Color(
            color.x,
            color.y,
            color.z,
            1.0f);

        light.intensity =
            levelObject.light.intensity;

        light.shadows = levelObject.light.castShadows
            ? LightShadows.Soft
            : LightShadows.None;
    }

    private static void ApplyRigidBody(
        GameObject gameObject,
        LevelObject levelObject)
    {
        var rigidBodyData = levelObject.rigidBody;

        if (rigidBodyData == null ||
            !rigidBodyData.enabled)
        {
            return;
        }

        var rigidBody =
            Undo.AddComponent<Rigidbody>(gameObject);

        rigidBody.mass = Mathf.Max(
            0.001f,
            rigidBodyData.mass);

        rigidBody.useGravity =
            rigidBodyData.useGravity;

        rigidBody.isKinematic =
            rigidBodyData.isStatic;

        SetRigidbodyVectorProperty(
            rigidBody,
            "linearVelocity",
            "velocity",
            ToVector3(
                rigidBodyData.velocity,
                Vector3.zero));

        SetRigidbodyVectorProperty(
            rigidBody,
            "angularVelocity",
            "angularVelocity",
            ToVector3(
                rigidBodyData.angularVelocity,
                Vector3.zero));

        var collider =
            gameObject.GetComponent<Collider>();

        if (collider == null)
            return;

        if (IsPlane(levelObject) &&
            collider is MeshCollider meshCollider)
        {
            UnityEngine.Object.DestroyImmediate(
                meshCollider);

            var boxCollider =
                Undo.AddComponent<BoxCollider>(
                    gameObject);

            boxCollider.size =
                new Vector3(
                    10.0f,
                    0.1f,
                    10.0f);

            boxCollider.center =
                new Vector3(
                    0.0f,
                    -0.05f,
                    0.0f);

            collider = boxCollider;
        }

        ApplyColliderMaterial(
            collider,
            gameObject.name + " Material",
            rigidBodyData.restitution,
            rigidBodyData.friction);

        var colliderData =
            rigidBodyData.collider;

        if (colliderData == null)
            return;

        var offset = ToVector3(
            colliderData.offset,
            Vector3.zero);

        if (collider is BoxCollider boxCollider)
        {
            boxCollider.center = offset;

            if (!colliderData.matchRenderScale)
            {
                boxCollider.size = ToVector3(
                    colliderData.size,
                    Vector3.one);
            }
        }
        else if (collider is SphereCollider sphereCollider)
        {
            sphereCollider.center = offset;

            if (!colliderData.matchRenderScale)
            {
                var size = ToVector3(
                    colliderData.size,
                    Vector3.one);

                sphereCollider.radius =
                    Mathf.Max(
                        size.x,
                        size.y,
                        size.z) * 0.5f;
            }
        }
        else if (collider is CapsuleCollider capsuleCollider)
        {
            capsuleCollider.center = offset;
            capsuleCollider.direction = 1;

            if (!colliderData.matchRenderScale)
            {
                var size = ToVector3(
                    colliderData.size,
                    new Vector3(
                        1.0f,
                        2.0f,
                        1.0f));

                capsuleCollider.radius =
                    Mathf.Max(
                        size.x,
                        size.z) * 0.5f;

                capsuleCollider.height =
                    Mathf.Max(
                        size.y,
                        capsuleCollider.radius * 2.0f);
            }
        }
    }

    private static Vector3 ToVector3(
        float[] values,
        Vector3 fallback)
    {
        if (values == null || values.Length < 3)
            return fallback;

        return new Vector3(
            values[0],
            values[1],
            values[2]);
    }

    private static bool IsPlane(
        LevelObject levelObject)
    {
        return string.Equals(
            levelObject.type,
            "plane",
            StringComparison.OrdinalIgnoreCase);
    }

    private static void SetRigidbodyVectorProperty(
        Rigidbody rigidBody,
        string preferredPropertyName,
        string fallbackPropertyName,
        Vector3 value)
    {
        var rigidBodyType = typeof(Rigidbody);

        var property = rigidBodyType.GetProperty(
            preferredPropertyName);

        if (property == null || !property.CanWrite)
        {
            property = rigidBodyType.GetProperty(
                fallbackPropertyName);
        }

        if (property != null && property.CanWrite)
        {
            property.SetValue(
                rigidBody,
                value,
                null);
        }
    }

    private static void ApplyColliderMaterial(
        Collider collider,
        string materialName,
        float restitution,
        float friction)
    {
#if UNITY_6000_0_OR_NEWER
        var material = new PhysicsMaterial(materialName)
        {
            bounciness = restitution,
            dynamicFriction = friction,
            staticFriction = friction
        };
#else
        var material = new PhysicMaterial(materialName)
        {
            bounciness = restitution,
            dynamicFriction = friction,
            staticFriction = friction
        };
#endif

        collider.material = material;
    }

    [Serializable]
    public sealed class LevelFile
    {
        public string format;
        public int version;
        public LevelObject[] objects;
    }

    [Serializable]
    public sealed class LevelObject
    {
        public string name;
        public string type;
        public TransformData transform;
        public LightData light;
        public RigidBodyData rigidBody;
    }

    [Serializable]
    public sealed class TransformData
    {
        public float[] position;
        public float[] rotation;
        public float[] scale;
    }

    [Serializable]
    public sealed class LightData
    {
        public float[] color = { 1.0f, 1.0f, 1.0f };
        public float intensity = 1.0f;
        public bool castShadows = true;
    }

    [Serializable]
    public sealed class RigidBodyData
    {
        public bool enabled;
        public float[] velocity;
        public float[] angularVelocity;
        public float mass = 1.0f;
        public float restitution = 0.45f;
        public float friction = 0.20f;
        public bool useGravity = true;
        public bool isStatic;
        public ColliderData collider;
    }

    [Serializable]
    public sealed class ColliderData
    {
        public string shape = "box";
        public float[] size;
        public float[] offset;
        public bool matchRenderScale = true;
    }
}
#endif
