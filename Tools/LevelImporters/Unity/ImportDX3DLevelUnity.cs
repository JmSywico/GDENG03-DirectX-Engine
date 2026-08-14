#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class ImportDX3DLevelUnity
{
    private const int MaximumLevelObjectCount = 10000;
    private const string ImportedRootFolder = "Assets/DX3DImported";
    private const string ImportedTextureFolder = "Assets/DX3DImported/Textures";

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

    [MenuItem("Tools/DX3D/Export .level")]
    private static void ExportLevelFromMenu()
    {
        var path = EditorUtility.SaveFilePanel(
            "Export DX3D .level",
            "",
            "Scene.level",
            "level");

        if (string.IsNullOrEmpty(path))
            return;

        ExportLevel(path);
    }

    public static void ImportLevel(string path)
    {
        if (!File.Exists(path))
            throw new FileNotFoundException(path);

        var level = JsonUtility.FromJson<LevelFile>(
            File.ReadAllText(path));

        if (level == null || level.objects == null)
            throw new InvalidDataException("Invalid DX3D .level file.");

        if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo())
            return;

        var importedScene = EditorSceneManager.NewScene(
            NewSceneSetup.EmptyScene,
            NewSceneMode.Single);

        SceneManager.SetActiveScene(importedScene);

        var undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Import DX3D .level");

        var root = new GameObject(
            Path.GetFileNameWithoutExtension(path));

        SceneManager.MoveGameObjectToScene(
            root,
            importedScene);

        Undo.RegisterCreatedObjectUndo(
            root,
            "Create DX3D Level Root");

        foreach (var levelObject in level.objects)
        {
            var importedObject = CreateObject(
                levelObject,
                path);

            if (importedObject == null)
                continue;

            Undo.RegisterCreatedObjectUndo(
                importedObject,
                "Create DX3D Object");

            SceneManager.MoveGameObjectToScene(
                importedObject,
                importedScene);

            importedObject.transform.SetParent(
                root.transform,
                true);
        }

        Undo.CollapseUndoOperations(undoGroup);
        EditorSceneManager.MarkSceneDirty(importedScene);
    }

    public static void ExportLevel(string path)
    {
        var objects = new List<LevelObject>();
        var scene = SceneManager.GetActiveScene();

        foreach (var rootObject in scene.GetRootGameObjects())
        {
            CollectExportObjects(
                rootObject.transform,
                objects);

            if (objects.Count >= MaximumLevelObjectCount)
                break;
        }

        var level = new LevelFile
        {
            format = "DX3D_LEVEL",
            version = 1,
            objects = objects.ToArray()
        };

        File.WriteAllText(
            path,
            JsonUtility.ToJson(
                level,
                true));

        AssetDatabase.Refresh();

        Debug.Log(
            $"DX3D exporter wrote {objects.Count} object(s) to {path}");
    }

    private static void CollectExportObjects(
        Transform transform,
        List<LevelObject> objects)
    {
        if (objects.Count >= MaximumLevelObjectCount)
            return;

        if (TryCreateLevelObject(
            transform.gameObject,
            out var levelObject))
        {
            objects.Add(levelObject);
        }

        foreach (Transform child in transform)
        {
            CollectExportObjects(
                child,
                objects);

            if (objects.Count >= MaximumLevelObjectCount)
                return;
        }
    }

    private static GameObject CreateObject(
        LevelObject levelObject,
        string levelFilePath)
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

        ApplyMaterial(
            gameObject,
            levelObject,
            levelFilePath);

        ApplyRigidBody(
            gameObject,
            levelObject);

        return gameObject;
    }

    private static bool TryCreateLevelObject(
        GameObject gameObject,
        out LevelObject levelObject)
    {
        levelObject = null;

        var type = GetLevelObjectType(gameObject);

        if (string.IsNullOrEmpty(type))
            return false;

        levelObject = new LevelObject
        {
            name = gameObject.name,
            type = type,
            transform = ExportTransform(
                gameObject,
                type),
            material = ExportMaterial(gameObject),
            rigidBody = ExportRigidBody(
                gameObject,
                type)
        };

        if (type == "directionalLight")
        {
            levelObject.light =
                ExportLight(gameObject);
        }

        return true;
    }

    private static string GetLevelObjectType(
        GameObject gameObject)
    {
        var light = gameObject.GetComponent<Light>();

        if (light != null &&
            light.type == LightType.Directional)
        {
            return "directionalLight";
        }

        var meshFilter =
            gameObject.GetComponent<MeshFilter>();

        var meshName =
            meshFilter != null && meshFilter.sharedMesh != null
            ? meshFilter.sharedMesh.name.ToLowerInvariant()
            : string.Empty;

        var collider =
            gameObject.GetComponent<Collider>();

        if (meshName.Contains("plane"))
            return "plane";

        if (meshName.Contains("cube"))
            return "cube";

        if (meshName.Contains("sphere"))
            return "sphere";

        if (meshName.Contains("capsule"))
            return "capsule";

        if (collider is CapsuleCollider)
            return "capsule";

        if (collider is SphereCollider)
            return "sphere";

        if (collider is BoxCollider)
            return "cube";

        return null;
    }

    private static TransformData ExportTransform(
        GameObject gameObject,
        string type)
    {
        var transform = gameObject.transform;
        var rotationRadians =
            transform.eulerAngles * Mathf.Deg2Rad;
        var scale = transform.lossyScale;

        if (type == "plane")
        {
            scale =
                new Vector3(
                    scale.x * 10.0f,
                    scale.y,
                    scale.z * 10.0f);
        }

        return new TransformData
        {
            position = FromVector3(transform.position),
            rotation = FromVector3(rotationRadians),
            scale = FromVector3(scale)
        };
    }

    private static LightData ExportLight(
        GameObject gameObject)
    {
        var light =
            gameObject.GetComponent<Light>();

        if (light == null)
            return null;

        return new LightData
        {
            color = FromVector3(
                new Vector3(
                    light.color.r,
                    light.color.g,
                    light.color.b)),
            intensity = light.intensity,
            castShadows =
                light.shadows != LightShadows.None
        };
    }

    private static MaterialData ExportMaterial(
        GameObject gameObject)
    {
        var renderer =
            gameObject.GetComponent<Renderer>();

        if (renderer == null ||
            renderer.sharedMaterial == null)
        {
            return null;
        }

        var material =
            renderer.sharedMaterial;

        var color =
            GetMaterialColor(material);

        var texturePath = string.Empty;

        if (material.mainTexture != null)
        {
            texturePath =
                AssetDatabase.GetAssetPath(
                    material.mainTexture);
        }

        if (string.IsNullOrEmpty(texturePath) &&
            IsWhite(color))
        {
            return null;
        }

        return new MaterialData
        {
            texture = texturePath,
            uvTiling = FromVector2(
                material.mainTextureScale),
            uvOffset = FromVector2(
                material.mainTextureOffset),
            color = FromColor(color)
        };
    }

    private static RigidBodyData ExportRigidBody(
        GameObject gameObject,
        string type)
    {
        var rigidBody =
            gameObject.GetComponent<Rigidbody>();

        if (rigidBody == null)
        {
            return new RigidBodyData
            {
                enabled = false
            };
        }

        var collider =
            gameObject.GetComponent<Collider>();

#if UNITY_6000_0_OR_NEWER
        var linearVelocity =
            rigidBody.linearVelocity;
#else
        var linearVelocity =
            rigidBody.velocity;
#endif

        return new RigidBodyData
        {
            enabled = true,
            velocity = FromVector3(linearVelocity),
            angularVelocity = FromVector3(
                rigidBody.angularVelocity),
            mass = Mathf.Max(
                0.001f,
                rigidBody.mass),
            restitution = GetColliderRestitution(collider),
            friction = GetColliderFriction(collider),
            useGravity = rigidBody.useGravity,
            isStatic = rigidBody.isKinematic,
            collider = ExportCollider(
                collider,
                type)
        };
    }

    private static ColliderData ExportCollider(
        Collider collider,
        string type)
    {
        var data = new ColliderData
        {
            shape = "box",
            size = FromVector3(Vector3.one),
            offset = FromVector3(Vector3.zero),
            matchRenderScale = true
        };

        if (collider is BoxCollider boxCollider)
        {
            data.size =
                FromVector3(boxCollider.size);
            data.offset =
                FromVector3(boxCollider.center);

            data.matchRenderScale =
                type != "plane" &&
                Approximately(
                    boxCollider.size,
                    Vector3.one) &&
                Approximately(
                    boxCollider.center,
                    Vector3.zero);
        }
        else if (collider is SphereCollider sphereCollider)
        {
            var diameter =
                sphereCollider.radius * 2.0f;

            data.size =
                FromVector3(
                    new Vector3(
                        diameter,
                        diameter,
                        diameter));
            data.offset =
                FromVector3(sphereCollider.center);
            data.matchRenderScale =
                Approximately(
                    sphereCollider.center,
                    Vector3.zero) &&
                Mathf.Approximately(
                    sphereCollider.radius,
                    0.5f);
        }
        else if (collider is CapsuleCollider capsuleCollider)
        {
            data.size =
                FromVector3(
                    new Vector3(
                        capsuleCollider.radius * 2.0f,
                        capsuleCollider.height,
                        capsuleCollider.radius * 2.0f));
            data.offset =
                FromVector3(capsuleCollider.center);
            data.matchRenderScale =
                Approximately(
                    capsuleCollider.center,
                    Vector3.zero) &&
                Mathf.Approximately(
                    capsuleCollider.radius,
                    0.5f) &&
                Mathf.Approximately(
                    capsuleCollider.height,
                    2.0f);
        }

        return data;
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

    private static void ApplyMaterial(
        GameObject gameObject,
        LevelObject levelObject,
        string levelFilePath)
    {
        var renderer =
            gameObject.GetComponent<Renderer>();

        if (renderer == null)
            return;

        var shader =
            FindCompatibleMaterialShader();

        if (shader == null)
            return;

        var material = new Material(shader);
        var materialData = levelObject.material;

        var color =
            materialData != null
            ? ToColor(
                materialData.color,
                Color.white)
            : Color.white;

        SetMaterialColor(
            material,
            color);

        var uvTiling =
            materialData != null
            ? ToVector2(
                materialData.uvTiling,
                Vector2.one)
            : Vector2.one;

        var uvOffset =
            materialData != null
            ? ToVector2(
                materialData.uvOffset,
                Vector2.zero)
            : Vector2.zero;

        SetMaterialTextureTransform(
            material,
            uvTiling,
            uvOffset);

        if (materialData != null)
        {
            SetMaterialTexture(
                material,
                LoadTexture(
                    materialData.texture,
                    levelFilePath));
        }

        renderer.sharedMaterial =
            material;
    }

    private static Shader FindCompatibleMaterialShader()
    {
        var shader =
            Shader.Find("Universal Render Pipeline/Lit") ??
            Shader.Find("HDRP/Lit") ??
            Shader.Find("Standard") ??
            Shader.Find("Unlit/Texture") ??
            Shader.Find("Unlit/Color");

        if (shader == null)
        {
            Debug.LogWarning(
                "DX3D importer could not find a compatible Unity material shader.");
        }

        return shader;
    }

    private static Texture LoadTexture(
        string texturePath,
        string levelFilePath)
    {
        if (string.IsNullOrEmpty(texturePath))
            return null;

        var normalizedPath =
            texturePath.Replace("\\", "/");

        var assetTexture =
            AssetDatabase.LoadAssetAtPath<Texture>(
                normalizedPath);

        if (assetTexture != null)
            return assetTexture;

        if (!Path.IsPathRooted(normalizedPath))
        {
            var projectRelativePath =
                Path.Combine(
                    "Assets",
                    normalizedPath)
                .Replace("\\", "/");

            assetTexture =
                AssetDatabase.LoadAssetAtPath<Texture>(
                    projectRelativePath);

            if (assetTexture != null)
                return assetTexture;
        }

        var externalPath =
            ResolveExternalTexturePath(
                normalizedPath,
                levelFilePath);

        if (string.IsNullOrEmpty(externalPath))
            return null;

        return ImportExternalTexture(
            externalPath);
    }

    private static string ResolveExternalTexturePath(
        string texturePath,
        string levelFilePath)
    {
        var normalizedPath =
            texturePath.Replace("\\", "/");

        if (Path.IsPathRooted(normalizedPath) &&
            File.Exists(normalizedPath))
        {
            return normalizedPath;
        }

        if (!string.IsNullOrEmpty(levelFilePath))
        {
            var levelDirectory =
                Path.GetDirectoryName(levelFilePath);

            if (!string.IsNullOrEmpty(levelDirectory))
            {
                var levelRelativePath =
                    Path.GetFullPath(
                        Path.Combine(
                            levelDirectory,
                            normalizedPath));

                if (File.Exists(levelRelativePath))
                    return levelRelativePath;
            }
        }

        var workingDirectoryPath =
            Path.GetFullPath(normalizedPath);

        if (File.Exists(workingDirectoryPath))
            return workingDirectoryPath;

        return null;
    }

    private static Texture ImportExternalTexture(
        string sourcePath)
    {
        EnsureImportedTextureFolder();

        var fileName =
            Path.GetFileName(sourcePath);

        if (string.IsNullOrEmpty(fileName))
            return null;

        var assetPath =
            (ImportedTextureFolder + "/" + fileName)
            .Replace("\\", "/");

        var destinationPath =
            Path.Combine(
                Application.dataPath,
                "DX3DImported",
                "Textures",
                fileName);

        try
        {
            File.Copy(
                sourcePath,
                destinationPath,
                true);

            AssetDatabase.ImportAsset(
                assetPath,
                ImportAssetOptions.ForceUpdate);

            return AssetDatabase.LoadAssetAtPath<Texture>(
                assetPath);
        }
        catch (Exception exception)
        {
            Debug.LogWarning(
                $"DX3D importer could not copy texture '{sourcePath}': {exception.Message}");
        }

        return null;
    }

    private static void EnsureImportedTextureFolder()
    {
        if (!AssetDatabase.IsValidFolder(
            ImportedRootFolder))
        {
            AssetDatabase.CreateFolder(
                "Assets",
                "DX3DImported");
        }

        if (!AssetDatabase.IsValidFolder(
            ImportedTextureFolder))
        {
            AssetDatabase.CreateFolder(
                ImportedRootFolder,
                "Textures");
        }
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

            var planeBoxCollider =
                Undo.AddComponent<BoxCollider>(
                    gameObject);

            planeBoxCollider.size =
                new Vector3(
                    10.0f,
                    0.1f,
                    10.0f);

            planeBoxCollider.center =
                new Vector3(
                    0.0f,
                    -0.05f,
                    0.0f);

            collider = planeBoxCollider;
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

    private static Vector2 ToVector2(
        float[] values,
        Vector2 fallback)
    {
        if (values == null || values.Length < 2)
            return fallback;

        return new Vector2(
            values[0],
            values[1]);
    }

    private static Color ToColor(
        float[] values,
        Color fallback)
    {
        if (values == null || values.Length < 4)
            return fallback;

        return new Color(
            values[0],
            values[1],
            values[2],
            values[3]);
    }

    private static float[] FromVector3(
        Vector3 value)
    {
        return new[]
        {
            value.x,
            value.y,
            value.z
        };
    }

    private static float[] FromVector2(
        Vector2 value)
    {
        return new[]
        {
            value.x,
            value.y
        };
    }

    private static float[] FromColor(
        Color value)
    {
        return new[]
        {
            value.r,
            value.g,
            value.b,
            value.a
        };
    }

    private static Color GetMaterialColor(
        Material material)
    {
        if (material.HasProperty("_BaseColor"))
            return material.GetColor("_BaseColor");

        if (material.HasProperty("_Color"))
            return material.GetColor("_Color");

        return Color.white;
    }

    private static void SetMaterialTexture(
        Material material,
        Texture texture)
    {
        if (texture == null)
            return;

        if (material.HasProperty("_BaseMap"))
        {
            material.SetTexture(
                "_BaseMap",
                texture);
        }

        if (material.HasProperty("_MainTex"))
        {
            material.SetTexture(
                "_MainTex",
                texture);
        }

        material.mainTexture = texture;
    }

    private static void SetMaterialTextureTransform(
        Material material,
        Vector2 tiling,
        Vector2 offset)
    {
        if (material.HasProperty("_BaseMap"))
        {
            material.SetTextureScale(
                "_BaseMap",
                tiling);

            material.SetTextureOffset(
                "_BaseMap",
                offset);
        }

        if (material.HasProperty("_MainTex"))
        {
            material.SetTextureScale(
                "_MainTex",
                tiling);

            material.SetTextureOffset(
                "_MainTex",
                offset);
        }

        material.mainTextureScale = tiling;
        material.mainTextureOffset = offset;
    }

    private static void SetMaterialColor(
        Material material,
        Color color)
    {
        if (material.HasProperty("_BaseColor"))
        {
            material.SetColor(
                "_BaseColor",
                color);
        }

        if (material.HasProperty("_Color"))
        {
            material.SetColor(
                "_Color",
                color);
        }
    }

    private static bool IsWhite(
        Color color)
    {
        return Mathf.Approximately(color.r, 1.0f) &&
            Mathf.Approximately(color.g, 1.0f) &&
            Mathf.Approximately(color.b, 1.0f) &&
            Mathf.Approximately(color.a, 1.0f);
    }

    private static bool Approximately(
        Vector3 lhs,
        Vector3 rhs)
    {
        return Mathf.Approximately(lhs.x, rhs.x) &&
            Mathf.Approximately(lhs.y, rhs.y) &&
            Mathf.Approximately(lhs.z, rhs.z);
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

    private static float GetColliderRestitution(
        Collider collider)
    {
        var material = collider != null
            ? collider.sharedMaterial
            : null;

        return material != null
            ? material.bounciness
            : 0.45f;
    }

    private static float GetColliderFriction(
        Collider collider)
    {
        var material = collider != null
            ? collider.sharedMaterial
            : null;

        return material != null
            ? Mathf.Max(
                material.dynamicFriction,
                material.staticFriction)
            : 0.20f;
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
        public string format = "DX3D_LEVEL";
        public int version = 1;
        public LevelObject[] objects;
    }

    [Serializable]
    public sealed class LevelObject
    {
        public string name;
        public string type;
        public TransformData transform;
        public LightData light;
        public MaterialData material;
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
    public sealed class MaterialData
    {
        public string texture = string.Empty;
        public float[] uvTiling = { 1.0f, 1.0f };
        public float[] uvOffset = { 0.0f, 0.0f };
        public float[] color = { 1.0f, 1.0f, 1.0f, 1.0f };
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
