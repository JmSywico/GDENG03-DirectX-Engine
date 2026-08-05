#include "Scene/SceneFactory.h"
#include "Graphics/ProcGen/ProcGen.h"

#include <memory>
#include <cstring>
#include <unordered_map>

namespace enignE::Scene
{
	namespace
	{
		struct PrimitiveMeshKey
		{
			PrimitiveType Type = PrimitiveType::Cube;
			float Size = 1.0f;
			float Width = 1.0f;
			float Height = 1.0f;
			float Depth = 1.0f;
			float Radius = 0.5f;
			int Slices = 16;
			int Stacks = 16;

			bool operator==(const PrimitiveMeshKey& other) const
			{
				return Type == other.Type
					&& Size == other.Size
					&& Width == other.Width
					&& Height == other.Height
					&& Depth == other.Depth
					&& Radius == other.Radius
					&& Slices == other.Slices
					&& Stacks == other.Stacks;
			}
		};

		struct PrimitiveMeshKeyHash
		{
			size_t operator()(const PrimitiveMeshKey& key) const
			{
				size_t seed = 0;
				const auto combine = [&seed](auto value)
				{
					using ValueType = decltype(value);
					seed ^= std::hash<ValueType>{}(value) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
				};

				combine(static_cast<int>(key.Type));
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

		PrimitiveMeshKey MakePrimitiveMeshKey(const PrimitiveDesc& desc)
		{
			return {
				desc.Type,
				desc.Size,
				desc.Width,
				desc.Height,
				desc.Depth,
				desc.Radius,
				desc.Slices,
				desc.Stacks
			};
		}

		std::uint64_t PrimitiveAssetHandle(const PrimitiveDesc& desc)
		{
			std::uint64_t hash = 14695981039346656037ull;
			const auto append = [&hash](const auto& value)
			{
				const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
				for (std::size_t index = 0; index < sizeof(value); ++index)
				{
					hash ^= bytes[index];
					hash *= 1099511628211ull;
				}
			};
			const PrimitiveMeshKey key = MakePrimitiveMeshKey(desc);
			append(key.Type);
			append(key.Size);
			append(key.Width);
			append(key.Height);
			append(key.Depth);
			append(key.Radius);
			append(key.Slices);
			append(key.Stacks);
			return hash == 0 ? 1 : hash;
		}

		MeshData CreatePrimitiveMesh(const PrimitiveDesc& desc)
		{
			switch (desc.Type)
			{
			case PrimitiveType::Cube:
				return ProcGen::CreateCube(desc.Size);
			case PrimitiveType::Rectangle:
				return ProcGen::CreateRectangle(desc.Width, desc.Height, desc.Depth);
			case PrimitiveType::Pyramid:
				return ProcGen::CreatePyramid(desc.Size);
			case PrimitiveType::Plane:
				return ProcGen::CreatePlane(desc.Width, desc.Depth);
			case PrimitiveType::Sphere:
				return ProcGen::CreateSphere(desc.Radius, desc.Slices, desc.Stacks);
			default:
				return ProcGen::CreateCube(1.0f);
			}
		}

		std::shared_ptr<Model> GetOrCreatePrimitiveModel(
			const PrimitiveDesc& desc,
			const PrimitiveModelFactory& modelFactory)
		{
			static std::unordered_map<PrimitiveMeshKey, std::weak_ptr<Model>, PrimitiveMeshKeyHash> modelCache;

			const PrimitiveMeshKey key = MakePrimitiveMeshKey(desc);
			if (auto it = modelCache.find(key); it != modelCache.end())
			{
				if (auto model = it->second.lock())
					return model;
			}

			auto model = modelFactory(CreatePrimitiveMesh(desc));
			modelCache[key] = model;
			return model;
		}
	}

	entt::entity CreatePrimitive(
		Scene& scene,
		const PrimitiveDesc& desc,
		const PrimitiveModelFactory& modelFactory)
	{
		auto model = GetOrCreatePrimitiveModel(desc, modelFactory);

		auto entity = scene.CreateEntity(desc.Name);

		auto& transform = scene.AddComponent<TransformComponent>(entity);
		transform.SetLocalTransform(desc.Position, desc.Rotation, desc.Scale);

		scene.AddComponent<HierarchyComponent>(entity);

		auto& renderer = scene.AddComponent<MeshRendererComponent>(entity);
		renderer.ModelPtr = model;
		renderer.Albedo = desc.Albedo;
		renderer.Material = desc.Material;
		renderer.MaterialResourcePtr = desc.MaterialResourcePtr;
		renderer.bVisible = desc.Visible;
		scene.AddComponent<RenderSourceComponent>(entity, {
			.Type = RenderSourceType::Primitive,
			.Primitive = desc.Type,
			.Size = desc.Size,
			.Width = desc.Width,
			.Height = desc.Height,
			.Depth = desc.Depth,
			.Radius = desc.Radius,
			.Slices = desc.Slices,
			.Stacks = desc.Stacks,
			.AssetHandle = PrimitiveAssetHandle(desc)
		});

		return entity;
	}

	entt::entity CreateCamera(Scene& scene, const CameraDesc& desc)
	{
		const entt::entity entity = scene.CreateEntity(desc.Name);
		if (entity == entt::null)
			return entt::null;

		TransformComponent transform;
		transform.SetLocalPosition(desc.Position);
		transform.SetLocalRotation(desc.Rotation);
		scene.AddComponent<TransformComponent>(entity, transform);
		scene.AddComponent<HierarchyComponent>(entity);

		CameraComponent camera;
		camera.FOVDegrees = desc.FOVDegrees;
		camera.NearPlane = desc.NearPlane;
		camera.FarPlane = desc.FarPlane;
		scene.AddComponent<CameraComponent>(entity, camera);
		if (desc.SetActive)
			scene.SetActiveCameraEntityID(scene.GetEntityID(entity));
		return entity;
	}
}
