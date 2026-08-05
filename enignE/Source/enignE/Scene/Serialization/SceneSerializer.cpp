#include "Scene/Serialization/SceneSerializer.h"
#include "Scene/Serialization/SceneMigration.h"

#include "Logging/Logging.h"
#include "Graphics/Texture2D.h"
#include "Scene/Scene.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_set>
#include <unordered_map>

namespace
{
	using json = nlohmann::json;
	using namespace enignE::Scene;

	struct PrimitiveCacheKey
	{
		PrimitiveType Type = PrimitiveType::Cube;
		float Size = 1.0f;
		float Width = 1.0f;
		float Height = 1.0f;
		float Depth = 1.0f;
		float Radius = 0.5f;
		int Slices = 16;
		int Stacks = 16;

		bool operator==(const PrimitiveCacheKey&) const = default;
	};

	struct PrimitiveCacheKeyHash
	{
		std::size_t operator()(const PrimitiveCacheKey& key) const
		{
			std::size_t seed = 0;
			const auto combine = [&seed](const auto& value)
			{
				seed ^= std::hash<std::decay_t<decltype(value)>>{}(value)
					+ 0x9e3779b9u + (seed << 6) + (seed >> 2);
			};
			combine(key.Type);
			combine(key.Size);
			combine(key.Width);
			combine(key.Height);
			combine(key.Depth);
			combine(key.Radius);
			combine(key.Slices);
			combine(key.Stacks);
			return seed;
		}
	};

	json Vec3(const DirectX::XMFLOAT3& value) { return {value.x, value.y, value.z}; }
	json Vec4(const DirectX::XMFLOAT4& value) { return {value.x, value.y, value.z, value.w}; }

	DirectX::XMFLOAT3 ReadVec3(const json& value, const DirectX::XMFLOAT3&)
	{
		if (!value.is_array() || value.size() != 3)
			throw std::runtime_error("expected a three-number array");
		return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
	}

	DirectX::XMFLOAT4 ReadVec4(const json& value, const DirectX::XMFLOAT4&)
	{
		if (!value.is_array() || value.size() != 4)
			throw std::runtime_error("expected a four-number array");
		return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
	}

	const char* PrimitiveName(PrimitiveType type)
	{
		switch (type)
		{
		case PrimitiveType::Cube: return "Cube";
		case PrimitiveType::Rectangle: return "Rectangle";
		case PrimitiveType::Pyramid: return "Pyramid";
		case PrimitiveType::Plane: return "Plane";
		case PrimitiveType::Sphere: return "Sphere";
		}
		return "Cube";
	}

	PrimitiveType ReadPrimitive(const std::string& name)
	{
		if (name == "Rectangle") return PrimitiveType::Rectangle;
		if (name == "Pyramid") return PrimitiveType::Pyramid;
		if (name == "Plane") return PrimitiveType::Plane;
		if (name == "Sphere") return PrimitiveType::Sphere;
		if (name == "Cube") return PrimitiveType::Cube;
		throw std::runtime_error("unknown primitive type");
	}

	json AssetJson(std::uint64_t handle, const std::string& path)
	{
		return {{"handle", handle}, {"path", path}};
	}
}

namespace enignE::Scene
{
	bool SceneSerializer::Save(
		Scene& scene,
		const std::filesystem::path& path,
		const PathNormalizer& pathNormalizer)
	{
		try
		{
			json root;
			root["scene"] = {
				{"name", scene.GetName()},
				{"version", SceneMigration::CurrentVersion},
				{"activeCamera", scene.GetSettings().ActiveCameraEntityID},
				{"activeLight", scene.GetSettings().ActiveLightEntityID}
			};
			root["entities"] = json::array();

			for (const entt::entity entity : scene.GetAllEntities())
			{
				if (scene.HasComponent<TransientEditorComponent>(entity)) continue;
				const std::uint64_t id = scene.GetEntityID(entity);
				if (id == 0)
					continue;
				const auto* tag = scene.GetComponent<TagComponent>(entity);
				const entt::entity parent = scene.GetParent(entity);
				if (const auto* body = scene.GetComponent<RigidBodyComponent>(entity); body && body->Enabled)
				{
					if (!scene.HasComponent<TransformComponent>(entity)
						|| !scene.HasComponent<ColliderComponent>(entity))
						throw std::runtime_error(
							"enabled rigid body requires Transform and Collider components");
					if (body->Motion == RigidBodyMotion::Dynamic && parent != entt::null)
						throw std::runtime_error("dynamic rigid body must be a hierarchy root");
				}
				json item = {
					{"id", id},
					{"name", tag ? tag->Tag : "Entity"},
					{"parent", parent == entt::null ? json(nullptr) : json(scene.GetEntityID(parent))},
					{"components", json::object()}
				};
				json& components = item["components"];

				if (const auto* transform = scene.GetComponent<TransformComponent>(entity))
				{
					components["TransformComponent"] = {
						{"position", Vec3(transform->GetLocalPosition())},
						{"rotation", Vec3(transform->GetLocalRotation())},
						{"rotationQuaternion", Vec4(transform->GetLocalRotationQuaternion())},
						{"scale", Vec3(transform->GetLocalScale())}
					};
				}
				if (const auto* renderer = scene.GetComponent<MeshRendererComponent>(entity))
				{
					json rendererJson = {
						{"visible", renderer->bVisible},
						{"castShadows", renderer->bCastShadows},
						{"receiveShadows", renderer->bReceiveShadows},
						{"materialMode", static_cast<int>(renderer->Material)},
						{"albedo", Vec4(renderer->Albedo)}
					};
					if (renderer->MaterialResourcePtr)
					{
						const MaterialResource& material = *renderer->MaterialResourcePtr;
						rendererJson["material"] = {
							{"mode", static_cast<int>(material.Mode)},
							{"albedo", Vec4(material.Albedo)},
							{"emissive", Vec3(material.Emissive)},
							{"metallic", material.Metallic},
							{"roughness", material.Roughness},
							{"albedoTextureAsset", AssetJson(
								material.AlbedoTextureHandle,
								pathNormalizer && !material.AlbedoTexturePath.empty()
									? pathNormalizer(material.AlbedoTexturePath)
									: material.AlbedoTexturePath)},
							{"normalTextureAsset", AssetJson(
								material.NormalTextureHandle,
								pathNormalizer && !material.NormalTexturePath.empty()
									? pathNormalizer(material.NormalTexturePath)
									: material.NormalTexturePath)},
							{"metallicRoughnessTextureAsset", AssetJson(
								material.MetallicRoughnessTextureHandle,
								pathNormalizer && !material.MetallicRoughnessTexturePath.empty()
									? pathNormalizer(material.MetallicRoughnessTexturePath)
									: material.MetallicRoughnessTexturePath)}
						};
					}
					if (const auto* source = scene.GetComponent<RenderSourceComponent>(entity))
					{
						if (source->Type == RenderSourceType::Primitive)
						{
							rendererJson["primitive"] = {
								{"assetHandle", source->AssetHandle},
								{"type", PrimitiveName(source->Primitive)},
								{"size", source->Size},
								{"width", source->Width},
								{"height", source->Height},
								{"depth", source->Depth},
								{"radius", source->Radius},
								{"slices", source->Slices},
								{"stacks", source->Stacks}
							};
						}
						else if (source->Type == RenderSourceType::ImportedModel)
						{
							rendererJson["modelAsset"] = AssetJson(
								source->AssetHandle,
								pathNormalizer && !source->AssetPath.empty()
									? pathNormalizer(source->AssetPath)
									: source->AssetPath);
						}
					}
					components["MeshRendererComponent"] = std::move(rendererJson);
				}
				if (const auto* camera = scene.GetComponent<CameraComponent>(entity))
				{
					components["CameraComponent"] = {
						{"fovDegrees", camera->FOVDegrees},
						{"nearPlane", camera->NearPlane},
						{"farPlane", camera->FarPlane},
						{"aspectRatio", camera->AspectRatio},
						{"primary", camera->bPrimary}
					};
				}
				if (const auto* light = scene.GetComponent<LightComponent>(entity))
				{
					components["LightComponent"] = {
						{"type", static_cast<int>(light->LightType)},
						{"color", Vec3(light->Color)},
						{"intensity", light->Intensity},
						{"range", light->Range},
						{"spotAngle", light->SpotAngle},
						{"castShadows", light->bCastShadows},
						{"shadowStrength", light->ShadowStrength},
						{"shadowBias", light->ShadowBias},
						{"shadowNormalBias", light->ShadowNormalBias},
						{"shadowDistance", light->ShadowDistance}
					};
				}
				if (const auto* rotator = scene.GetComponent<RotatorComponent>(entity))
				{
					components["RotatorComponent"] = {
						{"angularVelocity", Vec3(rotator->AngularVelocity)},
						{"enabled", rotator->Enabled}
					};
				}
				if (const auto* fly = scene.GetComponent<FlyControllerComponent>(entity))
				{
					components["FlyControllerComponent"] = {
						{"moveSpeed", fly->MoveSpeed}, {"lookSensitivity", fly->LookSensitivity},
						{"boostMultiplier", fly->BoostMultiplier},
						{"pitchLimitDegrees", fly->PitchLimitDegrees}, {"enabled", fly->Enabled}
					};
				}
				if (const auto* body = scene.GetComponent<RigidBodyComponent>(entity))
				{
					components["RigidBodyComponent"] = {
						{"motion", static_cast<int>(body->Motion)},
						{"friction", body->Friction},
						{"restitution", body->Restitution},
						{"linearDamping", body->LinearDamping},
						{"angularDamping", body->AngularDamping},
						{"gravityFactor", body->GravityFactor},
						{"enabled", body->Enabled}
					};
				}
				if (const auto* collider = scene.GetComponent<ColliderComponent>(entity))
				{
					components["ColliderComponent"] = {
						{"shape", static_cast<int>(collider->Shape)},
						{"halfExtents", Vec3(collider->HalfExtents)},
						{"radius", collider->Radius}
					};
				}
				if (const auto* prefab = scene.GetComponent<PrefabInstanceComponent>(entity))
				{
					components["PrefabInstanceComponent"] = {
						{"assetHandle", prefab->PrefabAssetHandle},
						{"assetPath", pathNormalizer && !prefab->PrefabAssetPath.empty()
							? pathNormalizer(prefab->PrefabAssetPath) : prefab->PrefabAssetPath},
						{"localID", prefab->PrefabLocalID},
						{"overrideMask", prefab->OverrideMask},
						{"root", prefab->IsRoot}
					};
				}
				root["entities"].push_back(std::move(item));
			}

			if (path.has_parent_path())
				std::filesystem::create_directories(path.parent_path());
			const std::filesystem::path temporaryPath = path.string() + ".tmp";
			std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
			if (!stream)
				return false;
			stream << root.dump(2);
			stream.flush();
			if (!stream)
				return false;
			stream.close();
#ifdef _WIN32
			if (!MoveFileExW(
					temporaryPath.c_str(),
					path.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				std::filesystem::remove(temporaryPath);
				return false;
			}
#else
			std::filesystem::rename(temporaryPath, path);
#endif
			LOG_INFOF("Saved scene '{}' to '{}'", scene.GetName(), path.string());
			return true;
		}
		catch (const std::exception& exception)
		{
			LOG_ERRORF("Failed to save scene '{}': {}", path.string(), exception.what());
			return false;
		}
	}

	bool SceneSerializer::Load(Scene& scene, const std::filesystem::path& path)
	{
		return Load(scene, path, {}, {}, {});
	}

	bool SceneSerializer::Load(
		Scene& scene,
		const std::filesystem::path& path,
		const PrimitiveResolver& primitiveResolver,
		const ModelResolver& modelResolver,
		const TextureResolver& textureResolver)
	{
		try
		{
			std::ifstream stream(path);
			if (!stream)
			{
				LOG_ERRORF("Scene file '{}' could not be opened", path.string());
				return false;
			}
			json root = json::parse(stream);
			SceneMigration::Migrate(root);
			const json& sceneJson = root.at("scene");
			const json& entities = root.at("entities");

			Scene loadedScene(sceneJson.value("name", "Scene"));
			std::unordered_map<std::uint64_t, entt::entity> loaded;
			std::unordered_set<std::uint64_t> hierarchyEntities;
			std::unordered_map<PrimitiveCacheKey, std::shared_ptr<Model>, PrimitiveCacheKeyHash>
				primitiveModels;
			for (const json& item : entities)
			{
				if (item.contains("parent") && !item["parent"].is_null())
				{
					hierarchyEntities.insert(item.at("id").get<std::uint64_t>());
					hierarchyEntities.insert(item["parent"].get<std::uint64_t>());
				}
			}
			for (const json& item : entities)
			{
				if (!item.is_object() || !item.contains("components") || !item["components"].is_object())
					throw std::runtime_error("entity must be an object with a components object");
				const std::uint64_t id = item.at("id").get<std::uint64_t>();
				const entt::entity entity = loadedScene.CreateEntityWithID(id, item.value("name", "Entity"));
				if (entity == entt::null)
					throw std::runtime_error("duplicate or invalid entity ID");
				if (hierarchyEntities.contains(id))
					loadedScene.AddComponent<HierarchyComponent>(entity);
				loaded.emplace(id, entity);
			}

			for (const json& item : entities)
			{
				const entt::entity entity = loaded.at(item.at("id").get<std::uint64_t>());
				const json& components = item.at("components");
				if (const auto it = components.find("TransformComponent"); it != components.end())
				{
					TransformComponent transform;
					const DirectX::XMFLOAT3 position =
						ReadVec3(it->value("position", json::array()), {});
					const DirectX::XMFLOAT3 rotation =
						ReadVec3(it->value("rotation", json::array()), {});
					const DirectX::XMFLOAT3 scale =
						ReadVec3(it->value("scale", json::array()), {1.0f, 1.0f, 1.0f});
					if (const auto quaternion = it->find("rotationQuaternion");
						quaternion != it->end())
					{
						transform.SetLocalTransformQuaternion(
							position,
							ReadVec4(*quaternion, {0.0f, 0.0f, 0.0f, 1.0f}),
							rotation,
							scale);
					}
					else
					{
						transform.SetLocalTransform(position, rotation, scale);
					}
					loadedScene.AddComponent<TransformComponent>(entity, transform);
					if (!loadedScene.HasComponent<HierarchyComponent>(entity))
						loadedScene.AddComponent<HierarchyComponent>(entity);
				}
				if (const auto it = components.find("MeshRendererComponent"); it != components.end())
				{
					MeshRendererComponent renderer;
					renderer.bVisible = it->value("visible", true);
					renderer.bCastShadows = it->value("castShadows", true);
					renderer.bReceiveShadows = it->value("receiveShadows", true);
					renderer.Material = static_cast<MaterialMode>(it->value("materialMode", 0));
					renderer.Albedo = ReadVec4(it->value("albedo", json::array()), {1, 1, 1, 1});
					if (static_cast<int>(renderer.Material) < static_cast<int>(MaterialMode::LitTint)
						|| static_cast<int>(renderer.Material) > static_cast<int>(MaterialMode::FlatBlue))
						throw std::runtime_error("invalid material mode");
					if (const auto materialJson = it->find("material"); materialJson != it->end())
					{
						if (!materialJson->is_object())
							throw std::runtime_error("material must be an object");
						auto material = std::make_shared<MaterialResource>();
						material->Mode = static_cast<MaterialMode>(materialJson->value("mode", 0));
						if (static_cast<int>(material->Mode) < static_cast<int>(MaterialMode::LitTint)
							|| static_cast<int>(material->Mode) > static_cast<int>(MaterialMode::FlatBlue))
							throw std::runtime_error("invalid material resource mode");
						material->Albedo = ReadVec4(
							materialJson->value("albedo", json::array()),
							{1, 1, 1, 1});
						material->Emissive = ReadVec3(
							materialJson->value("emissive", json::array()),
							{0, 0, 0});
						material->Metallic = materialJson->value("metallic", 0.0f);
						material->Roughness = materialJson->value("roughness", 1.0f);
						const auto loadTextureAsset = [&](const char* key,
							std::uint64_t& handle, std::string& assetPath,
							std::shared_ptr<Texture2D>& texture)
						{
							const json& asset = materialJson->at(key);
							handle = asset.value("handle", std::uint64_t{0});
							assetPath = asset.value("path", "");
							if ((handle != 0 || !assetPath.empty()) && textureResolver)
								texture = textureResolver(handle, assetPath);
						};
						loadTextureAsset(
							"albedoTextureAsset",
							material->AlbedoTextureHandle,
							material->AlbedoTexturePath,
							material->AlbedoTexture);
						loadTextureAsset(
							"normalTextureAsset",
							material->NormalTextureHandle,
							material->NormalTexturePath,
							material->NormalTexture);
						loadTextureAsset(
							"metallicRoughnessTextureAsset",
							material->MetallicRoughnessTextureHandle,
							material->MetallicRoughnessTexturePath,
							material->MetallicRoughnessTexture);
						renderer.MaterialResourcePtr = std::move(material);
					}
					RenderSourceComponent source;
					if (const auto primitive = it->find("primitive"); primitive != it->end())
					{
						source.Type = RenderSourceType::Primitive;
						source.AssetHandle =
							primitive->value("assetHandle", std::uint64_t{0});
						source.Primitive = ReadPrimitive(primitive->value("type", "Cube"));
						source.Size = primitive->value("size", 1.0f);
						source.Width = primitive->value("width", 1.0f);
						source.Height = primitive->value("height", 1.0f);
						source.Depth = primitive->value("depth", 1.0f);
						source.Radius = primitive->value("radius", 0.5f);
						source.Slices = primitive->value("slices", 16);
						source.Stacks = primitive->value("stacks", 16);
						if (primitiveResolver)
						{
							PrimitiveDesc desc;
							desc.Type = source.Primitive;
							desc.Size = source.Size;
							desc.Width = source.Width;
							desc.Height = source.Height;
							desc.Depth = source.Depth;
							desc.Radius = source.Radius;
							desc.Slices = source.Slices;
							desc.Stacks = source.Stacks;
							const PrimitiveCacheKey key{
								desc.Type, desc.Size, desc.Width, desc.Height, desc.Depth,
								desc.Radius, desc.Slices, desc.Stacks
							};
							if (const auto cached = primitiveModels.find(key);
								cached != primitiveModels.end())
							{
								renderer.ModelPtr = cached->second;
							}
							else
							{
								renderer.ModelPtr = primitiveResolver(desc);
								primitiveModels.emplace(key, renderer.ModelPtr);
							}
						}
					}
					else if (const auto asset = it->find("modelAsset"); asset != it->end())
					{
						source.Type = RenderSourceType::ImportedModel;
						source.AssetHandle = asset->value("handle", std::uint64_t{0});
						source.AssetPath = asset->value("path", "");
						if (modelResolver)
							renderer.ModelPtr = modelResolver(source.AssetHandle, source.AssetPath);
					}
					else
					{
						LOG_WARNF("Entity {} has unsupported renderer source; geometry was not restored", scene.GetEntityID(entity));
					}
					loadedScene.AddComponent<MeshRendererComponent>(entity, renderer);
					loadedScene.AddComponent<RenderSourceComponent>(entity, source);
				}
				if (const auto it = components.find("CameraComponent"); it != components.end())
				{
					CameraComponent camera;
					camera.FOVDegrees = it->value("fovDegrees", 45.0f);
					camera.NearPlane = it->value("nearPlane", 0.1f);
					camera.FarPlane = it->value("farPlane", 1000.0f);
					camera.AspectRatio = it->value("aspectRatio", 16.0f / 9.0f);
					camera.bPrimary = it->value("primary", false);
					if (camera.FOVDegrees < 1.0f || camera.FOVDegrees > 179.0f
						|| camera.NearPlane <= 0.0f || camera.FarPlane <= camera.NearPlane
						|| camera.AspectRatio <= 0.0f)
						throw std::runtime_error("invalid camera projection settings");
					loadedScene.AddComponent<CameraComponent>(entity, camera);
				}
				if (const auto it = components.find("LightComponent"); it != components.end())
				{
					LightComponent light;
					light.LightType = static_cast<LightComponent::Type>(it->value("type", 0));
					if (static_cast<int>(light.LightType) < 0 || static_cast<int>(light.LightType) > 2)
						throw std::runtime_error("invalid light type");
					light.Color = ReadVec3(it->value("color", json::array()), {1, 1, 1});
					light.Intensity = it->value("intensity", 1.0f);
					light.Range = it->value("range", 100.0f);
					light.SpotAngle = it->value("spotAngle", 45.0f);
					light.bCastShadows = it->value("castShadows", true);
					light.ShadowStrength = it->value("shadowStrength", 1.0f);
					light.ShadowBias = it->value("shadowBias", 0.0012f);
					light.ShadowNormalBias = it->value("shadowNormalBias", 0.02f);
					light.ShadowDistance = it->value("shadowDistance", 100.0f);
					if (light.Intensity < 0.0f || light.Range < 0.0f
						|| light.SpotAngle <= 0.0f || light.SpotAngle >= 180.0f
						|| light.ShadowStrength < 0.0f || light.ShadowStrength > 1.0f
						|| light.ShadowBias < 0.0f || light.ShadowNormalBias < 0.0f
						|| light.ShadowDistance <= 0.0f)
						throw std::runtime_error("invalid light settings");
					loadedScene.AddComponent<LightComponent>(entity, light);
				}
				if (const auto it = components.find("RotatorComponent"); it != components.end())
				{
					RotatorComponent rotator;
					rotator.AngularVelocity = ReadVec3(
						it->value("angularVelocity", json::array()), {0.0f, 1.0f, 0.0f});
					rotator.Enabled = it->value("enabled", true);
					if (!std::isfinite(rotator.AngularVelocity.x)
						|| !std::isfinite(rotator.AngularVelocity.y)
						|| !std::isfinite(rotator.AngularVelocity.z))
						throw std::runtime_error("invalid rotator angular velocity");
					loadedScene.AddComponent<RotatorComponent>(entity, rotator);
				}
				if (const auto it = components.find("FlyControllerComponent"); it != components.end())
				{
					FlyControllerComponent fly;
					fly.MoveSpeed = it->value("moveSpeed", 5.0f);
					fly.LookSensitivity = it->value("lookSensitivity", 0.0025f);
					fly.BoostMultiplier = it->value("boostMultiplier", 4.0f);
					fly.PitchLimitDegrees = it->value("pitchLimitDegrees", 89.0f);
					fly.Enabled = it->value("enabled", true);
					if (!std::isfinite(fly.MoveSpeed) || !std::isfinite(fly.LookSensitivity)
						|| !std::isfinite(fly.BoostMultiplier) || !std::isfinite(fly.PitchLimitDegrees)
						|| fly.MoveSpeed < 0 || fly.LookSensitivity < 0 || fly.BoostMultiplier < 1
						|| fly.PitchLimitDegrees <= 0 || fly.PitchLimitDegrees >= 90)
						throw std::runtime_error("invalid fly controller settings");
					loadedScene.AddComponent<FlyControllerComponent>(entity, fly);
				}
				if (const auto it = components.find("RigidBodyComponent"); it != components.end())
				{
					RigidBodyComponent body;
					body.Motion = static_cast<RigidBodyMotion>(it->value("motion", 0));
					body.Friction = it->value("friction", 0.5f);
					body.Restitution = it->value("restitution", 0.0f);
					body.LinearDamping = it->value("linearDamping", 0.05f);
					body.AngularDamping = it->value("angularDamping", 0.05f);
					body.GravityFactor = it->value("gravityFactor", 1.0f);
					body.Enabled = it->value("enabled", true);
					if (body.Motion < RigidBodyMotion::Static || body.Motion > RigidBodyMotion::Kinematic
						|| !std::isfinite(body.Friction) || !std::isfinite(body.Restitution)
						|| !std::isfinite(body.LinearDamping) || !std::isfinite(body.AngularDamping)
						|| !std::isfinite(body.GravityFactor) || body.Friction < 0.0f
						|| body.Restitution < 0.0f || body.Restitution > 1.0f
						|| body.LinearDamping < 0.0f || body.AngularDamping < 0.0f)
						throw std::runtime_error("invalid rigid body settings");
					loadedScene.AddComponent<RigidBodyComponent>(entity, body);
				}
				if (const auto it = components.find("ColliderComponent"); it != components.end())
				{
					ColliderComponent collider;
					collider.Shape = static_cast<ColliderShape>(it->value("shape", 0));
					collider.HalfExtents = ReadVec3(
						it->value("halfExtents", json::array()), {0.5f, 0.5f, 0.5f});
					collider.Radius = it->value("radius", 0.5f);
					if (collider.Shape < ColliderShape::Box || collider.Shape > ColliderShape::Sphere
						|| !std::isfinite(collider.HalfExtents.x)
						|| !std::isfinite(collider.HalfExtents.y)
						|| !std::isfinite(collider.HalfExtents.z)
						|| !std::isfinite(collider.Radius) || collider.HalfExtents.x <= 0.0f
						|| collider.HalfExtents.y <= 0.0f || collider.HalfExtents.z <= 0.0f
						|| collider.Radius <= 0.0f)
						throw std::runtime_error("invalid collider settings");
					loadedScene.AddComponent<ColliderComponent>(entity, collider);
				}
				if (const auto it = components.find("PrefabInstanceComponent"); it != components.end())
				{
					PrefabInstanceComponent prefab;
					prefab.PrefabAssetHandle = it->value("assetHandle", std::uint64_t{0});
					prefab.PrefabAssetPath = it->value("assetPath", "");
					prefab.PrefabLocalID = it->value("localID", std::uint64_t{0});
					prefab.OverrideMask = it->value("overrideMask", std::uint32_t{0});
					prefab.IsRoot = it->value("root", false);
					loadedScene.AddComponent<PrefabInstanceComponent>(entity, std::move(prefab));
				}
				if (const auto* body = loadedScene.GetComponent<RigidBodyComponent>(entity);
					body && body->Enabled
					&& (!loadedScene.HasComponent<TransformComponent>(entity)
						|| !loadedScene.HasComponent<ColliderComponent>(entity)))
					throw std::runtime_error("enabled rigid body requires Transform and Collider components");
			}

			for (const json& item : entities)
			{
				if (!item.contains("parent") || item["parent"].is_null())
					continue;
				const auto parent = loaded.find(item["parent"].get<std::uint64_t>());
				const auto child = loaded.find(item["id"].get<std::uint64_t>());
				if (parent == loaded.end() || child == loaded.end()
					|| !loadedScene.SetParent(child->second, parent->second))
					throw std::runtime_error("invalid parent reference or hierarchy cycle");
			}

			const std::uint64_t activeCamera = sceneJson.value("activeCamera", std::uint64_t{0});
			const std::uint64_t activeLight = sceneJson.value("activeLight", std::uint64_t{0});
			if (activeCamera != 0 && !loadedScene.SetActiveCameraEntityID(activeCamera))
				throw std::runtime_error("active camera does not reference a camera entity");
			if (activeLight != 0 && !loadedScene.SetActiveLightEntityID(activeLight))
				throw std::runtime_error("active light does not reference a light entity");
			loadedScene.MarkTransformDirty();
			loadedScene.MarkRenderDirty();
			scene = std::move(loadedScene);
			LOG_INFOF("Loaded scene '{}' from '{}'", scene.GetName(), path.string());
			return true;
		}
		catch (const std::exception& exception)
		{
			LOG_ERRORF("Failed to load scene '{}': {}", path.string(), exception.what());
			return false;
		}
	}
}
