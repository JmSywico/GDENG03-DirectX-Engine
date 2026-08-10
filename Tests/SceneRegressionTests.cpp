#include "ECS/Systems/HierarchySystem.h"
#include "ECS/Systems/TransformPropagationSystem.h"
#include "Graphics/Instancing/ChunkManager.h"
#include "Graphics/Instancing/InstanceData.h"
#include "Graphics/ShadowRenderer.h"
#include "Graphics/AssetRegistry.h"
#include "Graphics/MaterialAsset.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ModelLoader.h"
#include "Project/ProjectConfig.h"
#include "Scene/Components/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneFactory.h"
#include "Scene/Serialization/SceneSerializer.h"
#include "Scene/Serialization/SceneMigration.h"
#include "Editor/Commands/CommandStack.h"
#include "Editor/Commands/EditorCommand.h"
#include "Editor/PrefabAsset.h"
#include "Editor/ViewportRenderPlan.h"
#include "Core/SimulationClock.h"
#include "Core/JobSystem.h"
#include "ECS/Systems/RotatorSystem.h"
#include "ECS/Systems/FlyControllerSystem.h"
#include "Graphics/Instancing/InstanceSubmissionPlanner.h"
#include "Input/InputActions.h"
#include "Physics/PhysicsWorld.h"

#include <DirectXMath.h>

#include <cmath>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace
{
	constexpr float Epsilon = 0.0001f;

	struct TestContext
	{
		int Failures = 0;

		void Expect(bool condition, const std::string& message)
		{
			if (condition)
				return;

			++Failures;
			std::cerr << "FAILED: " << message << '\n';
		}
	};

	bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) <= Epsilon;
	}

	DirectX::XMFLOAT3 WorldPosition(
		const entt::registry& registry,
		entt::entity entity)
	{
		DirectX::XMFLOAT4X4 world{};
		DirectX::XMStoreFloat4x4(
			&world,
			registry.get<enignE::Scene::TransformComponent>(entity).GetWorldMatrix());
		return {world._41, world._42, world._43};
	}

	void ExpectPosition(
		TestContext& context,
		const entt::registry& registry,
		entt::entity entity,
		const DirectX::XMFLOAT3& expected,
		const std::string& message)
	{
		const DirectX::XMFLOAT3 actual = WorldPosition(registry, entity);
		context.Expect(
			NearlyEqual(actual.x, expected.x)
				&& NearlyEqual(actual.y, expected.y)
				&& NearlyEqual(actual.z, expected.z),
			message);
	}

	entt::entity CreateSpatialEntity(
		enignE::Scene::Scene& scene,
		const char* name,
		const DirectX::XMFLOAT3& position = {0.0f, 0.0f, 0.0f})
	{
		const entt::entity entity = scene.CreateEntity(name);
		auto& transform = scene.AddComponent<enignE::Scene::TransformComponent>(entity);
		transform.SetLocalPosition(position);
		scene.AddComponent<enignE::Scene::HierarchyComponent>(entity);
		return entity;
	}

	void TestHierarchy(TestContext& context)
	{
		enignE::Scene::Scene scene("Hierarchy");
		auto& registry = scene.GetRegistry();
		const entt::entity parentA = CreateSpatialEntity(scene, "ParentA");
		const entt::entity parentB = CreateSpatialEntity(scene, "ParentB");
		const entt::entity childA = CreateSpatialEntity(scene, "ChildA");
		const entt::entity childB = CreateSpatialEntity(scene, "ChildB");

		enignE::ECS::HierarchySystem::SetParent(registry, childA, parentA);
		enignE::ECS::HierarchySystem::SetParent(registry, childB, parentA);

		const auto children = enignE::ECS::HierarchySystem::GetChildren(registry, parentA);
		context.Expect(children.size() == 2, "parent tracks both direct children");
		context.Expect(
			enignE::ECS::HierarchySystem::GetParent(registry, childA) == parentA,
			"child reports its parent");
		context.Expect(
			registry.get<enignE::Scene::HierarchyComponent>(parentA).ChildCount == 2,
			"parent child count matches sibling list");

		enignE::ECS::HierarchySystem::SetParent(registry, childA, parentB);
		context.Expect(
			enignE::ECS::HierarchySystem::GetChildren(registry, parentA).size() == 1,
			"reparent detaches from the previous parent");
		context.Expect(
			enignE::ECS::HierarchySystem::GetChildren(registry, parentB) == std::vector{childA},
			"reparent attaches to the new parent");

		enignE::ECS::HierarchySystem::SetParent(registry, parentB, childA);
		context.Expect(
			enignE::ECS::HierarchySystem::GetParent(registry, parentB) == entt::null,
			"cycle-producing reparent is rejected");

		scene.SetLocalPosition(parentA, {10.0f, 0.0f, 0.0f});
		scene.SetLocalPosition(parentB, {-5.0f, 0.0f, 0.0f});
		scene.SetLocalPosition(childA, {2.0f, 0.0f, 0.0f});
		enignE::ECS::TransformPropagationSystem transformSystem;
		transformSystem.Update(registry, 0.0f);
		const DirectX::XMFLOAT3 worldBefore = WorldPosition(registry, childA);

		enignE::Editor::CommandStack commands;
		commands.Execute(std::make_unique<enignE::Editor::SetParentCommand>(
			scene,
			scene.GetEntityID(childA),
			scene.GetEntityID(parentB),
			scene.GetEntityID(parentA)));
		transformSystem.Update(registry, 0.0f);
		ExpectPosition(
			context,
			registry,
			childA,
			worldBefore,
			"hierarchy command preserves world position while reparenting");

		commands.Undo();
		transformSystem.Update(registry, 0.0f);
		context.Expect(
			scene.GetParent(childA) == parentB
				&& NearlyEqual(
					scene.GetComponent<enignE::Scene::TransformComponent>(childA)
						->GetLocalPosition().x,
					2.0f),
			"hierarchy command undo restores the original parent and local transform");

		commands.Redo();
		transformSystem.Update(registry, 0.0f);
		ExpectPosition(
			context,
			registry,
			childA,
			worldBefore,
			"hierarchy command redo restores the world-preserving local transform");
	}

	void TestTransforms(TestContext& context)
	{
		enignE::Scene::Scene scene("Transforms");
		auto& registry = scene.GetRegistry();
		const entt::entity parent = CreateSpatialEntity(scene, "Parent", {5.0f, 0.0f, 0.0f});
		const entt::entity child = CreateSpatialEntity(scene, "Child", {1.0f, 2.0f, 0.0f});
		const entt::entity grandchild = CreateSpatialEntity(scene, "Grandchild", {0.0f, 0.0f, 3.0f});

		enignE::ECS::HierarchySystem::SetParent(registry, child, parent);
		enignE::ECS::HierarchySystem::SetParent(registry, grandchild, child);

		enignE::ECS::TransformPropagationSystem system;
		system.Update(registry, 0.0f);
		ExpectPosition(context, registry, grandchild, {6.0f, 2.0f, 3.0f}, "initial world transform propagates");

		registry.get<enignE::Scene::TransformComponent>(parent)
			.SetLocalPosition({10.0f, 0.0f, 0.0f});
		system.Update(registry, 0.0f);
		ExpectPosition(
			context,
			registry,
			grandchild,
			{11.0f, 2.0f, 3.0f},
			"dirty parent updates clean descendants");

		const entt::entity rotated = CreateSpatialEntity(scene, "QuaternionRotation");
		const DirectX::XMVECTOR locked =
			DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XM_PIDIV2, 0.0f);
		const DirectX::XMVECTOR xRotation = DirectX::XMQuaternionMultiply(
			DirectX::XMQuaternionRotationAxis(
				DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
				0.25f),
			locked);
		const DirectX::XMVECTOR zRotation = DirectX::XMQuaternionMultiply(
			DirectX::XMQuaternionRotationAxis(
				DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
				0.25f),
			locked);
		DirectX::XMFLOAT4 xQuaternion{};
		DirectX::XMFLOAT4 zQuaternion{};
		DirectX::XMStoreFloat4(&xQuaternion, xRotation);
		DirectX::XMStoreFloat4(&zQuaternion, zRotation);
		scene.SetLocalRotationQuaternion(rotated, xQuaternion, {0.25f, DirectX::XM_PIDIV2, 0.0f});
		const DirectX::XMMATRIX xMatrix =
			scene.GetComponent<enignE::Scene::TransformComponent>(rotated)->GetLocalMatrix();
		scene.SetLocalRotationQuaternion(rotated, zQuaternion, {0.0f, DirectX::XM_PIDIV2, 0.25f});
		const DirectX::XMMATRIX zMatrix =
			scene.GetComponent<enignE::Scene::TransformComponent>(rotated)->GetLocalMatrix();
		DirectX::XMFLOAT4X4 xStored{};
		DirectX::XMFLOAT4X4 zStored{};
		DirectX::XMStoreFloat4x4(&xStored, xMatrix);
		DirectX::XMStoreFloat4x4(&zStored, zMatrix);
		context.Expect(
			!NearlyEqual(xStored._12, zStored._12)
				|| !NearlyEqual(xStored._13, zStored._13)
				|| !NearlyEqual(xStored._21, zStored._21),
			"quaternion rotations keep local X and Z independent at the Euler singularity");

		const DirectX::XMFLOAT4 quaternionBeforePositionEdit =
			scene.GetComponent<enignE::Scene::TransformComponent>(rotated)
				->GetLocalRotationQuaternion();
		scene.SetLocalPosition(rotated, {2.0f, 3.0f, 4.0f});
		const DirectX::XMFLOAT4 quaternionAfterPositionEdit =
			scene.GetComponent<enignE::Scene::TransformComponent>(rotated)
				->GetLocalRotationQuaternion();
		context.Expect(
			NearlyEqual(quaternionBeforePositionEdit.x, quaternionAfterPositionEdit.x)
				&& NearlyEqual(quaternionBeforePositionEdit.y, quaternionAfterPositionEdit.y)
				&& NearlyEqual(quaternionBeforePositionEdit.z, quaternionAfterPositionEdit.z)
				&& NearlyEqual(quaternionBeforePositionEdit.w, quaternionAfterPositionEdit.w),
			"position edits preserve quaternion rotation");
	}

	void TestDestruction(TestContext& context)
	{
		{
			enignE::Scene::Scene scene("RecursiveDestruction");
			auto& registry = scene.GetRegistry();
			const entt::entity parent = CreateSpatialEntity(scene, "Parent");
			const entt::entity child = CreateSpatialEntity(scene, "Child");
			const entt::entity grandchild = CreateSpatialEntity(scene, "Grandchild");
			enignE::ECS::HierarchySystem::SetParent(registry, child, parent);
			enignE::ECS::HierarchySystem::SetParent(registry, grandchild, child);

			context.Expect(scene.DestroyEntity(parent), "valid entity can be queued for destruction");
			context.Expect(registry.valid(parent), "destruction remains deferred until flush");
			scene.FlushDestroyQueue();
			context.Expect(scene.GetEntityCount() == 0, "recursive destruction removes the full subtree");
		}

		{
			enignE::Scene::Scene scene("NonRecursiveDestruction");
			auto& registry = scene.GetRegistry();
			const entt::entity parent = CreateSpatialEntity(scene, "Parent");
			const entt::entity child = CreateSpatialEntity(scene, "Child");
			enignE::ECS::HierarchySystem::SetParent(registry, child, parent);

			scene.DestroyEntityRecursive(parent, false);
			scene.FlushDestroyQueue();
			context.Expect(!registry.valid(parent), "non-recursive destruction removes the parent");
			context.Expect(registry.valid(child), "non-recursive destruction preserves children");
			context.Expect(
				enignE::ECS::HierarchySystem::GetParent(registry, child) == entt::null,
				"preserved children become roots");
		}
	}

	void TestPrimitiveCaching(TestContext& context)
	{
		enignE::Scene::Scene scene("PrimitiveCaching");
		int createCount = 0;
		const enignE::Scene::PrimitiveModelFactory modelFactory =
			[&createCount](const MeshData&)
			{
				++createCount;
				return std::make_shared<Model>();
			};

		enignE::Scene::PrimitiveDesc cube;
		cube.Name = "CubeA";
		const entt::entity first = enignE::Scene::CreatePrimitive(scene, cube, modelFactory);
		cube.Name = "CubeB";
		const entt::entity second = enignE::Scene::CreatePrimitive(scene, cube, modelFactory);

		const auto firstModel =
			scene.GetRegistry().get<enignE::Scene::MeshRendererComponent>(first).ModelPtr;
		const auto secondModel =
			scene.GetRegistry().get<enignE::Scene::MeshRendererComponent>(second).ModelPtr;
		context.Expect(createCount == 1, "identical primitive geometry is created once");
		context.Expect(firstModel == secondModel, "identical primitives share the cached model");

		cube.Name = "CubeC";
		cube.Size = 2.0f;
		const entt::entity third = enignE::Scene::CreatePrimitive(scene, cube, modelFactory);
		const auto thirdModel =
			scene.GetRegistry().get<enignE::Scene::MeshRendererComponent>(third).ModelPtr;
		context.Expect(createCount == 2, "geometry-affecting descriptor changes miss the cache");
		context.Expect(thirdModel != firstModel, "different primitive geometry gets a different model");

		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "enigne_primitive_cache.escene";
		context.Expect(enignE::Scene::SceneSerializer::Save(scene, path),
			"primitive cache scene saves");
		enignE::Scene::Scene loaded;
		int resolveCount = 0;
		context.Expect(enignE::Scene::SceneSerializer::Load(
			loaded,
			path,
			[&resolveCount](const enignE::Scene::PrimitiveDesc&)
			{
				++resolveCount;
				return std::make_shared<Model>();
			},
			{},
			{}), "primitive cache scene loads");
		context.Expect(resolveCount == 2,
			"scene loading resolves each unique primitive geometry once");
		const auto loadedFirst = loaded.GetComponent<enignE::Scene::MeshRendererComponent>(
			loaded.FindEntityByID(scene.GetEntityID(first)));
		const auto loadedSecond = loaded.GetComponent<enignE::Scene::MeshRendererComponent>(
			loaded.FindEntityByID(scene.GetEntityID(second)));
		context.Expect(loadedFirst && loadedSecond
			&& loadedFirst->ModelPtr == loadedSecond->ModelPtr,
			"scene loading shares identical primitive models");
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	void TestDirtyTransformPropagation(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("DirtyTransforms");
		const entt::entity parent = CreateSpatialEntity(scene, "Parent", {1.0f, 0.0f, 0.0f});
		const entt::entity child = CreateSpatialEntity(scene, "Child", {2.0f, 0.0f, 0.0f});
		const entt::entity grandchild = CreateSpatialEntity(scene, "Grandchild", {3.0f, 0.0f, 0.0f});
		scene.SetParent(child, parent);
		scene.SetParent(grandchild, child);
		for (int index = 0; index < 128; ++index)
			CreateSpatialEntity(scene, "Unrelated");

		ECS::TransformPropagationSystem system;
		const Scene::DirtyTransformSet initial = scene.TakeDirtyTransforms();
		context.Expect(initial.All, "new scenes request one full transform propagation");
		system.Update(scene.GetRegistry(), 0.0f);

		scene.SetLocalPosition(child, {4.0f, 0.0f, 0.0f});
		const Scene::DirtyTransformSet childDirty = scene.TakeDirtyTransforms();
		context.Expect(!childDirty.All && childDirty.Roots == std::vector{child},
			"transform mutation queues only the affected subtree root");
		system.UpdateDirty(scene.GetRegistry(), childDirty.Roots, 0.0f);
		context.Expect(system.GetVisitedTransformCount() == 2,
			"dirty propagation visits the changed child and its descendant only");
		ExpectPosition(context, scene.GetRegistry(), grandchild, {8.0f, 0.0f, 0.0f},
			"dirty child propagation uses the cached parent world transform");

		scene.SetLocalPosition(parent, {10.0f, 0.0f, 0.0f});
		const Scene::DirtyTransformSet parentDirty = scene.TakeDirtyTransforms();
		system.UpdateDirty(scene.GetRegistry(), parentDirty.Roots, 0.0f);
		context.Expect(system.GetVisitedTransformCount() == 3,
			"dirty parent propagation visits its subtree without scanning unrelated roots");
		ExpectPosition(context, scene.GetRegistry(), grandchild, {17.0f, 0.0f, 0.0f},
			"dirty parent propagation refreshes descendant world transforms");
	}

	void TestRenderInvalidation(TestContext& context)
	{
		enignE::Scene::Scene scene("RenderInvalidation");
		auto& registry = scene.GetRegistry();
		const entt::entity entity = scene.CreateEntity("Renderable");
		const std::uint64_t initialVersion = scene.GetRenderVersion();

		scene.AddComponent<enignE::Scene::TransformComponent>(entity);
		const std::uint64_t transformVersion = scene.GetRenderVersion();
		context.Expect(transformVersion > initialVersion, "adding a transform invalidates render data");

		scene.AddComponent<enignE::Scene::MeshRendererComponent>(entity);
		const std::uint64_t rendererVersion = scene.GetRenderVersion();
		context.Expect(rendererVersion > transformVersion, "adding a renderer invalidates render data");

		registry.patch<enignE::Scene::MeshRendererComponent>(
			entity,
			[](enignE::Scene::MeshRendererComponent& renderer)
			{
				renderer.bVisible = false;
			});
		const std::uint64_t patchedVersion = scene.GetRenderVersion();
		context.Expect(patchedVersion > rendererVersion, "updating a renderer invalidates render data");

		registry.get<enignE::Scene::MeshRendererComponent>(entity).bVisible = true;
		auto& transform = registry.get<enignE::Scene::TransformComponent>(entity);
		transform.SetLocalPosition({1.0f, 0.0f, 0.0f});
		enignE::ECS::TransformPropagationSystem transformSystem;
		transformSystem.Update(registry, 0.0f);
		context.Expect(
			transformSystem.DidUpdateRenderableTransform(),
			"renderable transform updates are reported");
		if (transformSystem.DidUpdateRenderableTransform())
			scene.MarkRenderDataChanged();
		const std::uint64_t movedVersion = scene.GetRenderVersion();
		context.Expect(movedVersion > patchedVersion, "reported transform updates invalidate render data");

		transformSystem.Update(registry, 0.0f);
		context.Expect(
			!transformSystem.DidUpdateRenderableTransform(),
			"clean transforms do not trigger render invalidation");

		scene.DestroyEntity(entity);
		scene.FlushDestroyQueue();
		context.Expect(scene.GetRenderVersion() > movedVersion, "destroying render components invalidates render data");
	}

	void TestPerChunkDirtyTracking(TestContext& context)
	{
		enignE::Scene::Scene scene("ChunkDirtyTracking");
		auto& registry = scene.GetRegistry();
		auto model = std::make_shared<Model>();

		const auto createRenderable = [&](const DirectX::XMFLOAT3& position)
		{
			const entt::entity entity = scene.CreateEntity();
			auto& transform = scene.AddComponent<enignE::Scene::TransformComponent>(entity);
			transform.SetLocalPosition(position);
			auto& renderer = scene.AddComponent<enignE::Scene::MeshRendererComponent>(entity);
			renderer.ModelPtr = model;
			return entity;
		};

		const entt::entity first = createRenderable({1.0f, 0.0f, 0.0f});
		createRenderable({65.0f, 0.0f, 0.0f});
		enignE::ECS::TransformPropagationSystem transformSystem;
		transformSystem.Update(registry, 0.0f);

		enignE::Graphics::ChunkManager chunks;
		context.Expect(chunks.UpdateFromRegistry(registry).size() == 2, "initial chunk sync dirties populated chunks");

		registry.patch<enignE::Scene::MeshRendererComponent>(
			first,
			[](enignE::Scene::MeshRendererComponent& renderer)
			{
				renderer.Albedo.x = 0.5f;
			});
		context.Expect(chunks.UpdateFromRegistry(registry).size() == 1, "material edit dirties only its chunk");

		registry.get<enignE::Scene::TransformComponent>(first).SetLocalPosition({33.0f, 0.0f, 0.0f});
		transformSystem.Update(registry, 0.0f);
		context.Expect(chunks.UpdateFromRegistry(registry).size() == 2, "cross-chunk move dirties old and new chunks");
		context.Expect(chunks.GetChunks().size() == 2, "empty old chunk is removed after a cross-chunk move");

		scene.SetLocalPosition(first, {34.0f, 0.0f, 0.0f});
		transformSystem.Update(registry, 0.0f);
		context.Expect(
			chunks.UpdateFromRegistry(registry).size() == 1,
			"same-chunk movement rebuilds only the containing chunk");
		const auto chunk = chunks.GetChunks().find({1, 0, 0});
		context.Expect(
			chunk != chunks.GetChunks().end()
				&& chunk->second.GetEntities() == std::vector{first}
				&& chunk->second.GetBoundsMin().x >= 34.0f,
			"same-chunk rebuild retains the entity and refreshes its bounds");

		auto& renderer = registry.get<enignE::Scene::MeshRendererComponent>(first);
		renderer.MaterialResourcePtr = std::make_shared<MaterialResource>();
		context.Expect(chunks.UpdateFromRegistry(registry).size() == 1,
			"assigning a material resource dirties its chunk");
		renderer.MaterialResourcePtr->Albedo.x = 0.25f;
		context.Expect(chunks.UpdateFromRegistry(registry).size() == 1,
			"in-place material resource edits dirty the containing chunk");
	}

	void TestRenderChunkBoundsAndCulling(TestContext& context)
	{
		using enignE::Graphics::ChunkCoord;
		using enignE::Graphics::Frustum;
		using enignE::Graphics::RenderChunk;

		RenderChunk chunk(ChunkCoord{0, 0, 0}, 32.0f);
		chunk.AddEntity(
			entt::entity{1},
			DirectX::XMFLOAT3{-2.0f, -1.0f, -3.0f},
			DirectX::XMFLOAT3{2.0f, 3.0f, 1.0f});
		chunk.AddEntity(
			entt::entity{2},
			DirectX::XMFLOAT3{10.0f, 0.0f, -1.0f},
			DirectX::XMFLOAT3{12.0f, 2.0f, 4.0f});

		context.Expect(
			chunk.GetBoundsMin().x == -2.0f
				&& chunk.GetBoundsMin().y == -1.0f
				&& chunk.GetBoundsMin().z == -3.0f,
			"chunk bounds include the minimum of all entities");
		context.Expect(
			chunk.GetBoundsMax().x == 12.0f
				&& chunk.GetBoundsMax().y == 3.0f
				&& chunk.GetBoundsMax().z == 4.0f,
			"chunk bounds include the maximum of all entities");
		context.Expect(
			chunk.IsVisibleByDistance({5.0f, 1.0f, 0.0f}, 1.0f),
			"camera inside chunk bounds passes distance culling");
		context.Expect(
			!chunk.IsVisibleByDistance({100.0f, 100.0f, 100.0f}, 1.0f),
			"distant chunk fails distance culling");

		Frustum containingFrustum{};
		containingFrustum.Planes[0] = {1.0f, 0.0f, 0.0f, 20.0f};
		containingFrustum.Planes[1] = {-1.0f, 0.0f, 0.0f, 20.0f};
		containingFrustum.Planes[2] = {0.0f, 1.0f, 0.0f, 20.0f};
		containingFrustum.Planes[3] = {0.0f, -1.0f, 0.0f, 20.0f};
		containingFrustum.Planes[4] = {0.0f, 0.0f, 1.0f, 20.0f};
		containingFrustum.Planes[5] = {0.0f, 0.0f, -1.0f, 20.0f};
		context.Expect(chunk.IntersectsFrustum(containingFrustum), "chunk inside frustum is accepted");

		Frustum rejectingFrustum = containingFrustum;
		rejectingFrustum.Planes[0] = {1.0f, 0.0f, 0.0f, -50.0f};
		context.Expect(!chunk.IntersectsFrustum(rejectingFrustum), "chunk outside one frustum plane is rejected");
	}

	void TestStableIDsAndVersions(TestContext& context)
	{
		enignE::Scene::Scene scene("IDs");
		const auto structure = scene.GetStructureVersion();
		const entt::entity first = scene.CreateEntity("First");
		const entt::entity loaded = scene.CreateEntityWithID(1001, "Loaded");
		context.Expect(first != entt::null && loaded != entt::null, "entities are created with stable IDs");
		context.Expect(scene.GetEntityID(first) != 0, "new entity receives a non-zero ID");
		context.Expect(scene.GetEntityID(loaded) == 1001, "explicit ID is preserved");
		context.Expect(scene.FindEntityByID(1001) == loaded, "entity lookup resolves stable ID");
		context.Expect(
			scene.CreateEntityWithID(1001, "Duplicate") == entt::null,
			"stable ID index rejects duplicate IDs");
		context.Expect(scene.GetStructureVersion() > structure, "entity creation increments structure version");
		scene.DestroyEntity(loaded);
		scene.FlushDestroyQueue();
		context.Expect(scene.FindEntityByID(1001) == entt::null,
			"destroyed entities are removed from the stable ID index");
		context.Expect(scene.CreateEntityWithID(1001, "Reused") != entt::null,
			"an ID can be restored after its previous entity is destroyed");

		scene.AddComponent<enignE::Scene::TransformComponent>(first);
		const auto transformVersion = scene.GetTransformVersion();
		const auto renderVersion = scene.GetRenderVersion();
		scene.SetLocalPosition(first, {1.0f, 2.0f, 3.0f});
		context.Expect(scene.GetTransformVersion() > transformVersion, "scene transform API increments transform version");
		context.Expect(scene.GetRenderVersion() == renderVersion,
			"non-renderable transform mutation does not invalidate render batches");
	}

	void TestSerializationAndCommands(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene source("Serialized");
		const entt::entity parent = source.CreateEntityWithID(10, "Parent");
		const entt::entity child = source.CreateEntityWithID(20, "Child");
		source.AddComponent<Scene::TransformComponent>(parent);
		source.AddComponent<Scene::HierarchyComponent>(parent);
		source.AddComponent<Scene::TransformComponent>(child);
		source.AddComponent<Scene::HierarchyComponent>(child);
		source.SetLocalPosition(child, {4.0f, 5.0f, 6.0f});
		source.SetParent(child, parent);
		const entt::entity cameraEntity = source.CreateEntityWithID(30, "Main Camera");
		source.AddComponent<Scene::TransformComponent>(cameraEntity);
		source.AddComponent<Scene::HierarchyComponent>(cameraEntity);
		Scene::CameraComponent camera;
		camera.FOVDegrees = 60.0f;
		camera.NearPlane = 0.25f;
		camera.FarPlane = 2500.0f;
		source.AddComponent<Scene::CameraComponent>(cameraEntity, camera);
		source.SetActiveCameraEntityID(30);
		const entt::entity lightEntity = source.CreateEntityWithID(40, "Sun");
		source.AddComponent<Scene::TransformComponent>(lightEntity);
		source.AddComponent<Scene::HierarchyComponent>(lightEntity);
		Scene::LightComponent sourceLight;
		sourceLight.LightType = Scene::LightComponent::Type::Spot;
		sourceLight.bEnabled = false;
		sourceLight.Intensity = 3.0f;
		sourceLight.Range = 20.0f;
		sourceLight.SpotAngle = 35.0f;
		sourceLight.ShadowStrength = 0.7f;
		sourceLight.ShadowBias = 0.002f;
		sourceLight.ShadowNormalBias = 0.04f;
		sourceLight.ShadowDistance = 75.0f;
		source.AddComponent<Scene::LightComponent>(lightEntity, sourceLight);
		source.SetActiveLightEntityID(40);
		const entt::entity rendered = source.CreateEntityWithID(50, "Textured");
		auto& renderer = source.AddComponent<Scene::MeshRendererComponent>(rendered);
		renderer.bCastShadows = false;
		renderer.bReceiveShadows = false;
		renderer.MaterialResourcePtr = std::make_shared<MaterialResource>();
		renderer.MaterialResourcePtr->Metallic = 0.75f;
		renderer.MaterialResourcePtr->Roughness = 0.25f;
		renderer.MaterialResourcePtr->Emissive = {0.1f, 0.2f, 0.3f};
		renderer.MaterialResourcePtr->AlbedoTextureHandle = 101;
		renderer.MaterialResourcePtr->AlbedoTexturePath = "textures/checker.png";
		renderer.MaterialResourcePtr->NormalTextureHandle = 102;
		renderer.MaterialResourcePtr->NormalTexturePath = "textures/checker_n.png";
		auto& renderSource = source.AddComponent<Scene::RenderSourceComponent>(rendered);
		renderSource.Type = Scene::RenderSourceType::ImportedModel;
		renderSource.AssetHandle = 100;
		renderSource.AssetPath = "models/test.obj";

		Editor::CommandStack commands;
		commands.Execute(std::make_unique<Editor::RenameEntityCommand>(
			source, 20, "Child", "Renamed"));
		context.Expect(
			std::string(source.GetComponent<Scene::TagComponent>(child)->Tag) == "Renamed",
			"command executes against stable ID");
		commands.Undo();
		context.Expect(
			std::string(source.GetComponent<Scene::TagComponent>(child)->Tag) == "Child",
			"undo restores previous value");
		commands.Redo();
		context.Expect(
			std::string(source.GetComponent<Scene::TagComponent>(child)->Tag) == "Renamed",
			"redo reapplies command");

		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "enigne_scene_regression.escene";
		context.Expect(Scene::SceneSerializer::Save(source, path), "scene saves to JSON");
		Scene::Scene loaded;
		std::vector<std::string> resolvedTextures;
		std::uint64_t resolvedModelHandle = 0;
		context.Expect(
			Scene::SceneSerializer::Load(
				loaded,
				path,
				{},
				[&resolvedModelHandle](std::uint64_t handle, const std::string&)
				{
					resolvedModelHandle = handle;
					return std::shared_ptr<Model>{};
				},
				[&resolvedTextures](std::uint64_t, const std::string& texturePath)
				{
					resolvedTextures.push_back(texturePath);
					return std::shared_ptr<Texture2D>{};
				}),
			"scene loads from JSON");
		const entt::entity loadedChild = loaded.FindEntityByID(20);
		context.Expect(loadedChild != entt::null, "serialized stable ID is restored");
		context.Expect(
			loaded.GetEntityID(loaded.GetParent(loadedChild)) == 10,
			"hierarchy parent is restored by stable ID");
		const auto* transform = loaded.GetComponent<Scene::TransformComponent>(loadedChild);
		context.Expect(
			transform && NearlyEqual(transform->GetLocalPosition().x, 4.0f),
			"transform data round-trips");
		const entt::entity loadedCamera = loaded.FindEntityByID(30);
		const auto* loadedCameraComponent = loaded.GetComponent<Scene::CameraComponent>(loadedCamera);
		context.Expect(
			loaded.GetSettings().ActiveCameraEntityID == 30
				&& loadedCameraComponent
				&& NearlyEqual(loadedCameraComponent->FOVDegrees, 60.0f)
				&& NearlyEqual(loadedCameraComponent->NearPlane, 0.25f)
				&& NearlyEqual(loadedCameraComponent->FarPlane, 2500.0f),
			"active camera and projection settings round-trip");
		context.Expect(
			loaded.GetSettings().ActiveLightEntityID == 40
				&& loaded.GetComponent<Scene::LightComponent>(
					loaded.FindEntityByID(40))->LightType == Scene::LightComponent::Type::Spot
				&& !loaded.GetComponent<Scene::LightComponent>(
					loaded.FindEntityByID(40))->bEnabled
				&& NearlyEqual(loaded.GetComponent<Scene::LightComponent>(
					loaded.FindEntityByID(40))->ShadowStrength, 0.7f)
				&& NearlyEqual(loaded.GetComponent<Scene::LightComponent>(
					loaded.FindEntityByID(40))->ShadowNormalBias, 0.04f),
			"active light and type round-trip");
		const auto* loadedRenderer =
			loaded.GetComponent<Scene::MeshRendererComponent>(loaded.FindEntityByID(50));
		context.Expect(
			loadedRenderer
				&& !loadedRenderer->bCastShadows
				&& !loadedRenderer->bReceiveShadows
				&& loadedRenderer->MaterialResourcePtr
				&& loadedRenderer->MaterialResourcePtr->AlbedoTexturePath == "textures/checker.png"
				&& NearlyEqual(loadedRenderer->MaterialResourcePtr->Metallic, 0.75f)
				&& NearlyEqual(loadedRenderer->MaterialResourcePtr->Roughness, 0.25f)
				&& std::find(
					resolvedTextures.begin(),
					resolvedTextures.end(),
					"textures/checker.png") != resolvedTextures.end()
				&& std::find(
					resolvedTextures.begin(),
					resolvedTextures.end(),
					"textures/checker_n.png") != resolvedTextures.end(),
			"material texture reference round-trips through the texture resolver");
		const auto* loadedSource =
			loaded.GetComponent<Scene::RenderSourceComponent>(loaded.FindEntityByID(50));
		context.Expect(
			loadedSource
				&& loadedSource->AssetHandle == 100
				&& loadedSource->AssetPath == "models/test.obj"
				&& resolvedModelHandle == 100,
			"model asset handles round-trip and drive resolution");
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	void TestSceneMigrations(TestContext& context)
	{
		using namespace enignE;
		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "enigne_v1_scene.escene";
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << R"({
				"scene":{"name":"Migrated","version":1,"activeDirectionalLight":2},
				"entities":[{
					"id":1,"name":"Legacy","parent":null,
					"components":{"MeshRendererComponent":{
						"visible":true,"materialMode":0,
						"albedo":[0.2,0.3,0.4,1.0],
						"assetPath":"models/legacy.obj"
					}}
				},{
					"id":2,"name":"Legacy Sun","parent":null,
					"components":{"LightComponent":{
						"type":0,"color":[1.0,1.0,1.0],
						"intensity":2.0,"range":100.0,"spotAngle":45.0
					}}
				}]
			})";
		}
		Scene::Scene scene;
		std::uint64_t modelHandle = 1;
		std::string modelPath;
		context.Expect(
			Scene::SceneSerializer::Load(
				scene,
				path,
				{},
				[&](std::uint64_t handle, const std::string& assetPath)
				{
					modelHandle = handle;
					modelPath = assetPath;
					return std::shared_ptr<Model>{};
				}),
			"version 1 scene migrates through every registered step");
		const auto* renderer =
			scene.GetComponent<Scene::MeshRendererComponent>(scene.FindEntityByID(1));
		context.Expect(
			modelHandle == 0
				&& modelPath == "models/legacy.obj"
				&& renderer
				&& renderer->MaterialResourcePtr
				&& NearlyEqual(renderer->MaterialResourcePtr->Roughness, 1.0f)
				&& scene.GetSettings().ActiveLightEntityID == 2,
			"migration preserves legacy model/material data and adds PBR defaults");

		context.Expect(Scene::SceneSerializer::Save(scene, path), "migrated scene saves");
		std::ifstream input(path);
		const nlohmann::json saved = nlohmann::json::parse(input);
		const auto savedRenderer = std::find_if(
			saved["entities"].begin(),
			saved["entities"].end(),
			[](const nlohmann::json& entity) { return entity.value("id", std::uint64_t{0}) == 1; });
		context.Expect(
			saved["scene"]["version"].get<int>() == Scene::SceneMigration::CurrentVersion
				&& saved["scene"].contains("activeLight")
				&& savedRenderer != saved["entities"].end()
				&& (*savedRenderer)["components"]["MeshRendererComponent"].contains("modelAsset")
				&& (*savedRenderer)["components"]["MeshRendererComponent"].value("castShadows", false)
				&& scene.GetComponent<Scene::LightComponent>(scene.FindEntityByID(2))->bCastShadows,
			"migrated scene is rewritten in the current format");
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	void TestMalformedSceneLoad(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("KeepMe");
		const entt::entity entity = scene.CreateEntityWithID(77, "Existing");
		scene.AddComponent<Scene::TransformComponent>(entity);
		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "enigne_malformed_scene.escene";

		const auto expectRejected = [&](const std::string& document, const std::string& message)
		{
			{
				std::ofstream output(path, std::ios::binary | std::ios::trunc);
				output << document;
			}
			context.Expect(!Scene::SceneSerializer::Load(scene, path), message);
			context.Expect(
				scene.GetName() == "KeepMe"
					&& scene.FindEntityByID(77) != entt::null
					&& scene.GetEntityCount() == 1,
				message + " without replacing the live scene");
		};

		expectRejected("{", "truncated JSON is rejected");
		expectRejected(
			R"({"scene":{"name":"Future","version":999},"entities":[]})",
			"future scene versions are rejected");
		expectRejected(
			R"({"scene":{"name":"BadParent","version":2},"entities":[{"id":1,"name":"A","parent":2,"components":{}}]})",
			"dangling parent references are rejected");
		expectRejected(
			R"({"scene":{"name":"BadActive","version":2,"activeCamera":1},"entities":[{"id":1,"name":"A","parent":null,"components":{}}]})",
			"active camera references must target camera entities");

		std::error_code error;
		std::filesystem::remove(path, error);
	}

	void TestCameraLightCommandsAndEdges(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("Settings");
		const entt::entity cameraEntity = CreateSpatialEntity(scene, "Camera");
		const entt::entity lightEntity = CreateSpatialEntity(scene, "Light");
		const entt::entity plainEntity = CreateSpatialEntity(scene, "Plain");
		scene.AddComponent<Scene::CameraComponent>(cameraEntity);
		scene.AddComponent<Scene::LightComponent>(lightEntity);
		const std::uint64_t cameraID = scene.GetEntityID(cameraEntity);
		const std::uint64_t lightID = scene.GetEntityID(lightEntity);

		context.Expect(!scene.SetActiveCameraEntityID(scene.GetEntityID(plainEntity)),
			"non-camera entity cannot become active camera");
		context.Expect(!scene.SetActiveLightEntityID(scene.GetEntityID(plainEntity)),
			"non-light entity cannot become active light");

		Editor::CommandStack commands;
		commands.Execute(std::make_unique<Editor::SetActiveCameraCommand>(scene, 0, cameraID));
		commands.Execute(std::make_unique<Editor::SetActiveLightCommand>(scene, 0, lightID));
		context.Expect(
			scene.GetSettings().ActiveCameraEntityID == cameraID
				&& scene.GetSettings().ActiveLightEntityID == lightID,
			"active camera and light commands apply");
		commands.Undo();
		commands.Undo();
		context.Expect(
			scene.GetSettings().ActiveCameraEntityID == 0
				&& scene.GetSettings().ActiveLightEntityID == 0,
			"active camera and light commands undo");

		Scene::CameraComponent first = *scene.GetComponent<Scene::CameraComponent>(cameraEntity);
		Scene::CameraComponent second = first;
		second.FOVDegrees = 60.0f;
		Scene::CameraComponent third = second;
		third.FOVDegrees = 75.0f;
		third.AspectRatio = 2.0f;
		third.bPrimary = true;
		commands.ExecuteCoalesced(std::make_unique<Editor::SetCameraCommand>(
			scene, cameraID, first, second));
		commands.ExecuteCoalesced(std::make_unique<Editor::SetCameraCommand>(
			scene, cameraID, second, third));
		commands.EndCoalescing();
		context.Expect(
			NearlyEqual(scene.GetComponent<Scene::CameraComponent>(cameraEntity)->AspectRatio, 2.0f)
				&& scene.GetComponent<Scene::CameraComponent>(cameraEntity)->bPrimary,
			"camera command covers aspect ratio and primary state");
		commands.Undo();
		context.Expect(
			NearlyEqual(scene.GetComponent<Scene::CameraComponent>(cameraEntity)->FOVDegrees, 45.0f),
			"coalesced camera edits undo to the interaction start");

		Scene::LightComponent beforeLight =
			*scene.GetComponent<Scene::LightComponent>(lightEntity);
		Scene::LightComponent afterLight = beforeLight;
		afterLight.LightType = Scene::LightComponent::Type::Spot;
		afterLight.Color = {0.2f, 0.4f, 0.8f};
		afterLight.Intensity = 4.0f;
		afterLight.Range = 25.0f;
		afterLight.SpotAngle = 30.0f;
		commands.Execute(std::make_unique<Editor::SetLightCommand>(
			scene, lightID, beforeLight, afterLight));
		commands.Execute(std::make_unique<Editor::SetActiveLightCommand>(scene, 0, lightID));
		const auto* editedLight = scene.GetComponent<Scene::LightComponent>(lightEntity);
		context.Expect(
			editedLight
				&& editedLight->LightType == Scene::LightComponent::Type::Spot
				&& NearlyEqual(editedLight->Color.z, 0.8f)
				&& NearlyEqual(editedLight->Intensity, 4.0f)
				&& NearlyEqual(editedLight->Range, 25.0f)
				&& NearlyEqual(editedLight->SpotAngle, 30.0f)
				&& scene.GetSettings().ActiveLightEntityID == lightID,
			"point and spot lights can remain active with every editable field");
		commands.Undo();
		commands.Undo();

		scene.SetActiveCameraEntityID(cameraID);
		Editor::CommandStack deletion;
		deletion.Execute(std::make_unique<Editor::DeleteEntityCommand>(scene, cameraEntity));
		scene.FlushDestroyQueue();
		context.Expect(scene.GetSettings().ActiveCameraEntityID == 0,
			"deleting the active camera clears the scene setting");
		deletion.Undo();
		context.Expect(scene.GetSettings().ActiveCameraEntityID == cameraID,
			"undoing active camera deletion restores the scene setting");
	}

	void TestAssetPathsAndTextureFailures(TestContext& context)
	{
		using namespace enignE;
		Graphics::AssetRegistry assets(std::filesystem::current_path());
		const Graphics::AssetHandle first = assets.RegisterPath("textures/checker.png");
		const Graphics::AssetHandle second =
			assets.RegisterPath(std::filesystem::current_path() / "textures/checker.png");
		context.Expect(first != Graphics::InvalidAssetHandle && first == second,
			"project-relative and absolute asset paths share a stable handle");
		context.Expect(assets.GetPath(first) == "textures/checker.png",
			"asset registry stores project-relative paths");
		const std::filesystem::path manifest =
			std::filesystem::temp_directory_path() / "enigne_asset_manifest.json";
		context.Expect(assets.SaveManifest(manifest), "asset manifest saves");
		Graphics::AssetRegistry restored(std::filesystem::current_path());
		context.Expect(
			restored.LoadManifest(manifest)
				&& restored.GetReference(first).Path == "textures/checker.png",
			"asset handles survive registry reconstruction through the manifest");
		std::error_code error;
		std::filesystem::remove(manifest, error);

		context.Expect(
			!Texture2D::LoadFromFile("missing-texture.png"),
			"texture loading fails cleanly before renderer initialization");
	}

	void TestSnapshotEntityCommands(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("SnapshotCommands");
		const entt::entity parent = CreateSpatialEntity(scene, "Parent");
		const entt::entity child = CreateSpatialEntity(scene, "Child", {2.0f, 0.0f, 0.0f});
		const std::uint64_t parentID = scene.GetEntityID(parent);
		const std::uint64_t childID = scene.GetEntityID(child);
		scene.SetParent(child, parent);
		auto model = std::make_shared<Model>();
		auto& renderer = scene.AddComponent<Scene::MeshRendererComponent>(child);
		renderer.ModelPtr = model;
		renderer.Albedo = {0.2f, 0.3f, 0.4f, 1.0f};

		Editor::CommandStack commands;
		commands.Execute(std::make_unique<Editor::DeleteEntityCommand>(scene, parent));
		context.Expect(scene.IsEntityPendingDestroy(parent), "delete command queues the subtree");
		commands.Undo();
		scene.FlushDestroyQueue();
		context.Expect(
			scene.FindEntityByID(parentID) != entt::null && scene.FindEntityByID(childID) != entt::null,
			"undo before flush cancels pending subtree destruction");

		commands.Redo();
		scene.FlushDestroyQueue();
		context.Expect(
			scene.FindEntityByID(parentID) == entt::null && scene.FindEntityByID(childID) == entt::null,
			"redo destroys the captured subtree");
		commands.Undo();
		const entt::entity restoredParent = scene.FindEntityByID(parentID);
		const entt::entity restoredChild = scene.FindEntityByID(childID);
		context.Expect(
			restoredParent != entt::null && restoredChild != entt::null,
			"undo after flush recreates the subtree with stable IDs");
		context.Expect(scene.GetParent(restoredChild) == restoredParent, "snapshot restores hierarchy links");
		const auto* restoredRenderer = scene.GetComponent<Scene::MeshRendererComponent>(restoredChild);
		context.Expect(
			restoredRenderer && restoredRenderer->ModelPtr == model,
			"snapshot restores runtime model ownership");

		auto duplicate = std::make_unique<Editor::DuplicateEntityCommand>(scene, restoredParent);
		Editor::DuplicateEntityCommand* duplicateCommand = duplicate.get();
		commands.Execute(std::move(duplicate));
		const std::uint64_t duplicateID = duplicateCommand->GetEntityID();
		const entt::entity duplicatedParent = scene.FindEntityByID(duplicateID);
		const auto duplicatedChildren = scene.GetChildren(duplicatedParent);
		context.Expect(
			duplicatedParent != entt::null && duplicateID != parentID,
			"duplicate command creates a root with a fresh stable ID");
		context.Expect(
			duplicatedChildren.size() == 1
				&& scene.GetEntityID(duplicatedChildren.front()) != childID,
			"duplicate command recreates descendants with fresh stable IDs");
		const auto* duplicatedRenderer = duplicatedChildren.empty()
			? nullptr
			: scene.GetComponent<Scene::MeshRendererComponent>(duplicatedChildren.front());
		context.Expect(
			duplicatedRenderer && duplicatedRenderer->ModelPtr == model,
			"duplicate command preserves subtree component state");
		commands.Undo();
		scene.FlushDestroyQueue();
		context.Expect(
			scene.FindEntityByID(duplicateID) == entt::null,
			"undo removes a duplicated subtree");
		commands.Redo();
		context.Expect(
			scene.FindEntityByID(duplicateID) != entt::null,
			"redo restores a duplicate with the same generated ID");

		auto create = std::make_unique<Editor::CreateEntityCommand>(scene, "Created");
		Editor::CreateEntityCommand* createCommand = create.get();
		commands.Execute(std::move(create));
		const std::uint64_t createdID = createCommand->GetEntityID();
		context.Expect(scene.FindEntityByID(createdID) != entt::null, "create command creates an entity");
		commands.Undo();
		scene.FlushDestroyQueue();
		context.Expect(scene.FindEntityByID(createdID) == entt::null, "undo removes a created entity");
		commands.Redo();
		context.Expect(scene.FindEntityByID(createdID) != entt::null, "redo restores the same stable ID");

		Scene::CameraComponent camera;
		camera.FOVDegrees = 70.0f;
		auto createCamera = std::make_unique<Editor::CreateEntityCommand>(scene, "Camera", camera);
		Editor::CreateEntityCommand* createCameraCommand = createCamera.get();
		commands.Execute(std::move(createCamera));
		const std::uint64_t cameraID = createCameraCommand->GetEntityID();
		const entt::entity createdCamera = scene.FindEntityByID(cameraID);
		context.Expect(
			createdCamera != entt::null
				&& scene.HasComponent<Scene::CameraComponent>(createdCamera),
			"camera create command adds a camera component");
		commands.Undo();
		scene.FlushDestroyQueue();
		context.Expect(scene.FindEntityByID(cameraID) == entt::null, "undo removes the camera game object");
		commands.Redo();
		const entt::entity restoredCamera = scene.FindEntityByID(cameraID);
		const auto* restoredCameraComponent = scene.GetComponent<Scene::CameraComponent>(restoredCamera);
		context.Expect(
			restoredCameraComponent && NearlyEqual(restoredCameraComponent->FOVDegrees, 70.0f),
			"redo restores the same camera component and stable ID");
	}

	void TestRecordedTransformCommand(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("RecordedTransform");
		const entt::entity entity = CreateSpatialEntity(scene, "Dragged");
		const std::uint64_t id = scene.GetEntityID(entity);
		const Editor::TransformValue before{{0, 0, 0}, {0, 0, 0}, {1, 1, 1}};
		const Editor::TransformValue after{{3, 4, 5}, {0, 0.5f, 0}, {2, 2, 2}};
		scene.SetTransform(entity, after.Position, after.Rotation, after.Scale);

		Editor::CommandStack commands;
		commands.RecordExecuted(std::make_unique<Editor::SetTransformCommand>(scene, id, before, after));
		commands.Undo();
		context.Expect(
			NearlyEqual(scene.GetComponent<Scene::TransformComponent>(entity)->GetLocalPosition().x, 0.0f),
			"recorded live transform undoes without re-executing first");
		commands.Redo();
		context.Expect(
			NearlyEqual(scene.GetComponent<Scene::TransformComponent>(entity)->GetLocalPosition().x, 3.0f),
			"recorded live transform redoes the final value");
	}

	void TestJobSystemAndParallelTransforms(TestContext& context)
	{
		enignE::Core::JobSystem jobs(4);
		std::atomic_size_t sum = 0;
		jobs.ParallelFor(1000, 16, [&sum](std::size_t index)
		{
			sum.fetch_add(index + 1, std::memory_order_relaxed);
		});
		context.Expect(sum.load() == 500500, "job system parallel-for completes every index");
		bool completionRan = false;
		jobs.PostMainThread([&completionRan] { completionRan = true; });
		context.Expect(jobs.DrainMainThread() == 1 && completionRan,
			"job system drains main-thread completions explicitly");
		jobs.CancelPending();
		auto cancelled = jobs.SubmitCancelable(
			[](std::stop_token token) { return token.stop_requested(); });
		context.Expect(cancelled.get(), "cancelable jobs observe the pool cancellation token");
		jobs.ResetCancellation();

		enignE::Scene::Scene scene("Parallel Transforms");
		for (std::size_t index = 0; index < 128; ++index)
		{
			const entt::entity entity = scene.CreateEntity("Root");
			auto& transform = scene.AddComponent<enignE::Scene::TransformComponent>(entity);
			transform.SetLocalPosition({static_cast<float>(index), 2.0f, -3.0f});
			scene.AddComponent<enignE::Scene::HierarchyComponent>(entity);
		}
		enignE::ECS::TransformPropagationSystem transforms;
		transforms.Update(scene.GetRegistry(), 0.0f, &jobs);
		context.Expect(transforms.GetVisitedTransformCount() == 128,
			"parallel transform propagation visits each independent root once");
		for (const entt::entity entity : scene.View<enignE::Scene::TransformComponent>())
		{
			const auto* transform = scene.GetComponent<enignE::Scene::TransformComponent>(entity);
			context.Expect(transform && !transform->IsWorldTransformDirty(),
				"parallel transform propagation commits every world matrix");
		}
		const enignE::Core::JobSystemStats stats = jobs.GetStats();
		context.Expect(stats.CompletedJobs > 0 && stats.WorkerCount == 4,
			"job telemetry reports completed work and worker capacity");

		const auto modelPath = std::filesystem::temp_directory_path() / "enigne_job_import.obj";
		{
			std::ofstream output(modelPath);
			output << "o Triangle\n"
				<< "v 0 0 0\n"
				<< "v 1 0 0\n"
				<< "v 0 1 0\n"
				<< "f 1 2 3\n";
		}
		auto preparedModel = ModelLoader::Prepare(modelPath.string(), &jobs);
		context.Expect(preparedModel && preparedModel->Meshes.size() == 1
			&& preparedModel->Meshes.front().Data.vertices.size() == 3,
			"model import CPU preparation does not require GPU resource finalization");
		std::filesystem::remove(modelPath);
	}

	void TestInstanceDataEncoding(TestContext& context)
	{
		const DirectX::XMMATRIX world =
			DirectX::XMMatrixScaling(2.0f, 3.0f, 4.0f)
			* DirectX::XMMatrixTranslation(7.0f, 8.0f, 9.0f);
		const enignE::Graphics::InstanceData data =
			enignE::Graphics::InstanceData::FromWorldMatrix(world);
		context.Expect(
			NearlyEqual(data.WorldRow0.x, 2.0f)
				&& NearlyEqual(data.WorldRow1.y, 3.0f)
				&& NearlyEqual(data.WorldRow2.z, 4.0f)
				&& NearlyEqual(data.WorldRow3.x, 7.0f)
				&& NearlyEqual(data.WorldRow3.y, 8.0f)
				&& NearlyEqual(data.WorldRow3.z, 9.0f)
				&& NearlyEqual(data.WorldRow3.w, 1.0f),
			"instance encoding preserves scale, translation, and homogeneous position");
	}

	void TestProjectAndMetadataIdentity(TestContext& context)
	{
		using namespace enignE;
		const std::filesystem::path root =
			std::filesystem::temp_directory_path() / "enigne_project_metadata_test";
		std::error_code error;
		std::filesystem::remove_all(root, error);
		std::filesystem::create_directories(root / "assets", error);
		Project::ProjectConfig project;
		project.Name = "Metadata Test";
		project.ProjectFile = root / "test.enigneproject";
		project.StartupScene = "scenes/opening.escene";
		project.InputActions = "config/game-input.json";
		project.OutputDirectory = "dist";
		context.Expect(project.Save(project.ProjectFile), "project descriptor saves");
		Project::ProjectConfig loaded;
		context.Expect(
			Project::ProjectConfig::Load(project.ProjectFile, loaded)
				&& loaded.Name == project.Name
				&& loaded.GetRoot() == root
				&& loaded.ResolveStartupScene() == root / "scenes/opening.escene"
				&& loaded.InputActions == project.InputActions
				&& loaded.OutputDirectory == project.OutputDirectory,
			"project descriptor restores runtime and project-relative paths");

		const std::filesystem::path original = root / "assets" / "sample.model";
		{
			std::ofstream output(original);
			output << "test";
		}
		Graphics::AssetRegistry assets(root);
		const Graphics::AssetHandle originalHandle =
			assets.Register(original, Graphics::AssetType::Model).Handle;
		const std::filesystem::path metadata = original.string() + ".meta";
		const std::filesystem::path manifest = root / "asset-manifest.json";
		context.Expect(
			originalHandle != Graphics::InvalidAssetHandle
				&& std::filesystem::exists(metadata)
				&& assets.SaveManifest(manifest),
			"registering a file creates persistent asset metadata");

		const std::filesystem::path moved = root / "assets" / "renamed.model";
		const std::filesystem::path movedMetadata = moved.string() + ".meta";
		std::filesystem::rename(original, moved, error);
		std::filesystem::rename(metadata, movedMetadata, error);
		Graphics::AssetRegistry afterMove(root);
		context.Expect(afterMove.LoadManifest(manifest),
			"the previous manifest remains readable after an external asset move");
		const Graphics::AssetHandle movedHandle =
			afterMove.Register(moved, Graphics::AssetType::Model).Handle;
		context.Expect(
			originalHandle == movedHandle
				&& afterMove.GetPath(originalHandle) == "assets/renamed.model",
			"moving an asset with its metadata preserves identity and reconciles its path");
		const std::filesystem::path texturePath = root / "assets" / "surface.png";
		{ std::ofstream output(texturePath); output << "discovery"; }
		context.Expect(afterMove.DiscoverAssets("assets"), "asset discovery scans project assets");
		const auto discovered = afterMove.GetReferences();
		const auto texture = std::find_if(discovered.begin(), discovered.end(),
			[](const Graphics::AssetReference& asset)
			{ return asset.Path == "assets/surface.png"; });
		context.Expect(
			texture != discovered.end() && texture->Type == Graphics::AssetType::Texture
				&& std::filesystem::exists(texturePath.string() + ".meta"),
			"asset discovery classifies textures and creates persistent metadata");
		std::filesystem::remove_all(root, error);
	}

	void TestMaterialAssetRoundTrip(TestContext& context)
	{
		MaterialResource source;
		source.Albedo = {0.2f, 0.4f, 0.8f, 1.0f};
		source.Emissive = {0.1f, 0.05f, 0.0f};
		source.Metallic = 0.75f;
		source.Roughness = 0.22f;
		source.AlbedoTextureHandle = 42;
		source.AlbedoTexturePath = "assets/albedo.png";
		const auto path = std::filesystem::temp_directory_path() / "enigne_material_test.ematerial";
		MaterialResource loaded;
		context.Expect(
			enignE::Graphics::MaterialAsset::Save(source, path)
				&& enignE::Graphics::MaterialAsset::Load(path, loaded)
				&& NearlyEqual(loaded.Albedo.z, source.Albedo.z)
				&& NearlyEqual(loaded.Metallic, source.Metallic)
				&& NearlyEqual(loaded.Roughness, source.Roughness)
				&& loaded.AlbedoTextureHandle == source.AlbedoTextureHandle,
			"material assets preserve PBR values and stable texture references");
		std::error_code error;
		std::filesystem::remove(path, error);

		enignE::Scene::Scene scene("MaterialUndo");
		const entt::entity entity = scene.CreateEntity("Surface");
		scene.AddComponent<enignE::Scene::MeshRendererComponent>(entity);
		const std::uint64_t id = scene.GetEntityID(entity);
		enignE::Editor::CommandStack commands;
		commands.Execute(std::make_unique<enignE::Editor::SetRendererMaterialResourceCommand>(
			scene, id, std::nullopt, source));
		context.Expect(
			scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)->MaterialResourcePtr
				&& NearlyEqual(
					scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)
						->MaterialResourcePtr->Metallic,
					source.Metallic),
			"material command applies a value snapshot");
		commands.Undo();
		context.Expect(
			!scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)->MaterialResourcePtr,
			"undo restores the absence of a material resource");
		commands.Redo();
		context.Expect(
			scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)->MaterialResourcePtr
				&& NearlyEqual(
					scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)
						->MaterialResourcePtr->Roughness,
					source.Roughness),
			"redo restores the complete material resource");

		auto firstModel = std::make_shared<Model>();
		auto secondModel = std::make_shared<Model>();
		scene.SetRendererModel(entity, firstModel);
		enignE::Scene::RenderSourceComponent sourceBefore;
		sourceBefore.Type = enignE::Scene::RenderSourceType::ImportedModel;
		sourceBefore.AssetPath = "assets/first.obj";
		scene.AddComponent<enignE::Scene::RenderSourceComponent>(entity, sourceBefore);
		enignE::Scene::RenderSourceComponent sourceAfter = sourceBefore;
		sourceAfter.AssetPath = "assets/second.obj";
		commands.Execute(std::make_unique<enignE::Editor::SetRendererModelCommand>(
			scene, id, firstModel, sourceBefore, secondModel, sourceAfter));
		commands.Undo();
		context.Expect(
			scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)->ModelPtr == firstModel
				&& scene.GetComponent<enignE::Scene::RenderSourceComponent>(entity)->AssetPath
					== sourceBefore.AssetPath,
			"undo restores both model data and its serialized asset source");
		commands.Redo();
		context.Expect(
			scene.GetComponent<enignE::Scene::MeshRendererComponent>(entity)->ModelPtr == secondModel
				&& scene.GetComponent<enignE::Scene::RenderSourceComponent>(entity)->AssetPath
					== sourceAfter.AssetPath,
			"redo reapplies model assignment and its serialized asset source");
	}

	void TestCommandStackDirtyRevisions(TestContext& context)
	{
		enignE::Scene::Scene scene("DirtyRevisions");
		const entt::entity entity = scene.CreateEntity("Original");
		const std::uint64_t id = scene.GetEntityID(entity);
		enignE::Editor::CommandStack commands;
		context.Expect(!commands.IsDirty(), "a cleared command stack starts clean");
		commands.Execute(std::make_unique<enignE::Editor::RenameEntityCommand>(
			scene, id, "Original", "Saved"));
		context.Expect(commands.IsDirty(), "executing a command marks the scene dirty");
		commands.MarkSaved();
		context.Expect(!commands.IsDirty(), "marking the current revision saved clears dirty state");
		commands.Execute(std::make_unique<enignE::Editor::RenameEntityCommand>(
			scene, id, "Saved", "Changed"));
		commands.Undo();
		context.Expect(!commands.IsDirty(), "undoing to the saved revision clears dirty state");
		commands.Redo();
		context.Expect(commands.IsDirty(), "redoing past the saved revision restores dirty state");
		commands.Undo();
		commands.Execute(std::make_unique<enignE::Editor::RenameEntityCommand>(
			scene, id, "Saved", "Branched"));
		context.Expect(commands.IsDirty() && !commands.CanRedo(),
			"branching from a saved revision creates a distinct dirty revision");
	}

	void TestClipboardSnapshotPaste(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("Clipboard");
		const entt::entity root = CreateSpatialEntity(scene, "Copied Root");
		const entt::entity child = CreateSpatialEntity(scene, "Copied Child");
		scene.SetParent(child, root);
		const Editor::EntitySnapshot clipboard = Editor::EntitySnapshot::CaptureSubtree(scene, root);
		scene.DestroyEntityRecursive(root);
		scene.FlushDestroyQueue();
		Editor::CommandStack commands;
		auto paste = std::make_unique<Editor::PasteEntityCommand>(scene, clipboard);
		Editor::PasteEntityCommand* pasted = paste.get();
		commands.Execute(std::move(paste));
		const std::uint64_t pastedID = pasted->GetEntityID();
		const entt::entity pastedRoot = scene.FindEntityByID(pastedID);
		context.Expect(
			pastedRoot != entt::null && scene.GetChildren(pastedRoot).size() == 1,
			"clipboard snapshots paste complete subtrees after the source is deleted");
		commands.Undo();
		scene.FlushDestroyQueue();
		context.Expect(scene.FindEntityByID(pastedID) == entt::null,
			"undo removes a pasted subtree");
		commands.Redo();
		context.Expect(scene.FindEntityByID(pastedID) != entt::null,
			"redo restores the pasted subtree with stable command IDs");
	}

	void TestShadowConfiguration(TestContext& context)
	{
		using namespace enignE;
		const auto splits = Graphics::ShadowRenderer::CalculateCascadeSplits(0.1f, 100.0f);
		context.Expect(
			splits[0] > 0.1f
				&& splits[0] < splits[1]
				&& splits[1] < splits[2]
				&& splits[2] < splits[3]
				&& NearlyEqual(splits[3], 100.0f),
			"cascade splits are ordered and terminate at the shadow distance");
		const auto threeSplits = Graphics::ShadowRenderer::CalculateCascadeSplits(
			0.1f, 100.0f, 0.65f, 3);
		context.Expect(
			threeSplits[0] < threeSplits[1]
				&& threeSplits[1] < threeSplits[2]
				&& NearlyEqual(threeSplits[2], 100.0f)
				&& NearlyEqual(threeSplits[3], 100.0f),
			"configurable cascade splits terminate at the active cascade count");
		Graphics::ShadowRenderer shadowRenderer;
		context.Expect(
			shadowRenderer.GetDirectionalCascadeCount() == 3,
			"directional shadows default to three cascades");
		shadowRenderer.SetDirectionalCascadeCount(9);
		context.Expect(
			shadowRenderer.GetDirectionalCascadeCount() == Graphics::ShadowFrameData::MaxCascades,
			"directional cascade configuration is clamped to the supported maximum");

		Scene::Scene scene("ShadowFlags");
		const entt::entity entity = CreateSpatialEntity(scene, "Caster");
		scene.AddComponent<Scene::MeshRendererComponent>(entity);
		const std::uint64_t before = scene.GetRenderVersion();
		context.Expect(
			scene.SetRendererShadowFlags(entity, false, false)
				&& scene.GetRenderVersion() > before,
			"shadow participation changes invalidate cached render data");
		const auto* renderer = scene.GetComponent<Scene::MeshRendererComponent>(entity);
		context.Expect(
			renderer && !renderer->bCastShadows && !renderer->bReceiveShadows,
			"mesh shadow participation is stored independently");
	}

	void TestFixedStepRotator(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene editorScene("RotatorEditor");
		const entt::entity editorEntity = CreateSpatialEntity(editorScene, "Spinner");
		editorScene.AddComponent<Scene::RotatorComponent>(editorEntity,
			Scene::RotatorComponent{{0.0f, 2.0f, 0.0f}, true});
		Scene::Scene playScene("RotatorPlay");
		playScene.CopyFrom(editorScene);
		ECS::RotatorSystem system;
		system.FixedUpdate(playScene, 0.25f);
		const entt::entity playEntity = playScene.FindEntityByID(editorScene.GetEntityID(editorEntity));
		context.Expect(
			std::abs(playScene.GetComponent<Scene::TransformComponent>(playEntity)->GetLocalRotation().y - 0.5f) < 0.0001f,
			"rotator behavior advances deterministically on the fixed timestep");
		context.Expect(
			std::abs(editorScene.GetComponent<Scene::TransformComponent>(editorEntity)->GetLocalRotation().y) < 0.0001f,
			"fixed-step behavior does not mutate the editor scene copy");
		const auto path = std::filesystem::temp_directory_path() / "enigne_rotator.escene";
		context.Expect(Scene::SceneSerializer::Save(editorScene, path),
			"rotator behavior serializes with the scene");
		Scene::Scene loaded("LoadedRotator");
		context.Expect(Scene::SceneSerializer::Load(loaded, path)
			&& loaded.HasComponent<Scene::RotatorComponent>(loaded.FindEntityByID(editorScene.GetEntityID(editorEntity))),
			"rotator behavior survives a scene round trip");
		std::filesystem::remove(path);
	}

	void TestRotatorAuthoringCommands(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("RotatorAuthoring");
		const entt::entity entity = CreateSpatialEntity(scene, "Authored Spinner");
		const std::uint64_t id = scene.GetEntityID(entity);
		Editor::CommandStack commands;
		commands.Execute(std::make_unique<Editor::AddComponentCommand>(
			scene, id, Scene::ComponentKind::Rotator));
		context.Expect(scene.HasComponent<Scene::RotatorComponent>(entity),
			"component catalog adds the rotator through an undoable command");
		const Scene::RotatorComponent before = *scene.GetComponent<Scene::RotatorComponent>(entity);
		Scene::RotatorComponent after{{1.0f, 2.0f, 3.0f}, false};
		commands.Execute(std::make_unique<Editor::SetRotatorCommand>(scene, id, before, after));
		context.Expect(scene.GetComponent<Scene::RotatorComponent>(entity)->AngularVelocity.z == 3.0f,
			"rotator authoring command applies angular velocity");
		commands.Execute(std::make_unique<Editor::RemoveRotatorCommand>(scene, id, after));
		context.Expect(!scene.HasComponent<Scene::RotatorComponent>(entity),
			"rotator removal is command-backed");
		commands.Undo();
		context.Expect(scene.HasComponent<Scene::RotatorComponent>(entity)
			&& scene.GetComponent<Scene::RotatorComponent>(entity)->AngularVelocity.z == 3.0f,
			"undo restores the complete rotator value");
		const auto& descriptor = Scene::ComponentCatalog::Describe(Scene::ComponentKind::Rotator);
		context.Expect(descriptor.Persisted && descriptor.Snapshotted,
			"component catalog records rotator persistence contracts");
	}

	void TestFlyController(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("FlyController");
		const entt::entity camera = CreateSpatialEntity(scene, "Fly Camera");
		scene.AddComponent<Scene::FlyControllerComponent>(camera);
		Input::InputActionMap actions;
		actions.Bind("MoveForward", {Input::BindingType::Key, 'W', 1.0f});
		actions.UpdateFromValues([](const Input::ActionBinding& binding)
			{ return binding.Code == 'W' ? 1.0f : 0.0f; });
		ECS::FlyControllerSystem system;
		system.FixedUpdate(scene, actions, 0.5f, false);
		context.Expect(scene.GetComponent<Scene::TransformComponent>(camera)->GetLocalPosition().z == 0.0f,
			"fly controller ignores input without game authorization");
		system.FixedUpdate(scene, actions, 0.5f, true);
		context.Expect(std::abs(scene.GetComponent<Scene::TransformComponent>(camera)->GetLocalPosition().z - 2.5f) < 0.0001f,
			"fly controller movement is deterministic on fixed steps");
		system.ApplyLook(scene, 100.0f, -50.0f);
		const auto rotation = scene.GetComponent<Scene::TransformComponent>(camera)->GetLocalRotation();
		context.Expect(rotation.y > 0.0f && rotation.x < 0.0f,
			"fly controller consumes frame mouse delta once for look");
		Scene::Scene copy("FlyCopy"); copy.CopyFrom(scene);
		context.Expect(copy.HasComponent<Scene::FlyControllerComponent>(copy.FindEntityByID(scene.GetEntityID(camera))),
			"play-scene copies preserve fly controllers");
		const auto path = std::filesystem::temp_directory_path() / "enigne_fly_controller.escene";
		context.Expect(Scene::SceneSerializer::Save(scene, path), "fly controller serializes");
		Scene::Scene loaded("LoadedFly");
		context.Expect(Scene::SceneSerializer::Load(loaded, path)
			&& loaded.HasComponent<Scene::FlyControllerComponent>(loaded.FindEntityByID(scene.GetEntityID(camera))),
			"fly controller survives a scene round trip");
		std::filesystem::remove(path);
	}

	void TestInstanceSubmissionPlanning(TestContext& context)
	{
		using enignE::Graphics::InstanceSubmissionPlanner;
		const auto split = InstanceSubmissionPlanner::Build(10, 4);
		context.Expect(split == std::vector<std::uint32_t>({4, 4, 2}),
			"instance submission planner preserves all instances across capacity splits");
		context.Expect(InstanceSubmissionPlanner::Build(10, 0).empty(),
			"instance submission planner reports zero-capacity exhaustion without looping");
		context.Expect(InstanceSubmissionPlanner::NextCount(3, 9) == 3,
			"instance submission planner does not overrun the remaining batch");
		context.Expect(InstanceSubmissionPlanner::MaxInstancesPerSubmission == 32768,
			"instance submission ceiling leaves capacity for later render passes");
		const auto stress = InstanceSubmissionPlanner::Build(100000, 4096);
		const std::uint64_t plannedTotal = std::accumulate(
			stress.begin(), stress.end(), std::uint64_t{0});
		context.Expect(plannedTotal == 100000 && stress.size() == 25 && stress.back() == 1696,
			"instance submission planner preserves a 100k-instance stress batch");
	}

	void TestViewportRenderPlanning(TestContext& context)
	{
		using enignE::Editor::ViewportRenderPlan;
		const auto sceneOnly = ViewportRenderPlan::Build(true, false, true, false);
		context.Expect(sceneOnly.RenderScene && !sceneOnly.RenderGame
			&& sceneOnly.SceneOwnsShadows && !sceneOnly.GameOwnsShadows,
			"hidden Game viewport submits no world work");
		const auto gameOnly = ViewportRenderPlan::Build(false, true, true, false);
		context.Expect(!gameOnly.RenderScene && gameOnly.RenderGame
			&& gameOnly.GameOwnsShadows && !gameOnly.SceneOwnsShadows,
			"hidden Scene viewport submits no work and visible Game retains shadows");
		const auto both = ViewportRenderPlan::Build(true, true, true, true);
		context.Expect(both.RenderScene && both.RenderGame
			&& both.GameOwnsShadows && !both.SceneOwnsShadows,
			"two visible viewports render together with one shadow owner");
		const auto missingCamera = ViewportRenderPlan::Build(false, true, false, true);
		context.Expect(missingCamera.ClearGame && !missingCamera.RenderGame
			&& !missingCamera.GameOwnsShadows,
			"visible Game viewport clears safely without an active camera");
	}

	void TestPhaseOneFoundation(TestContext& context)
	{
		using namespace enignE;
		Core::SimulationClock clock(0.02);
		context.Expect(clock.Advance(0.05, true) == 2 && clock.GetTickCount() == 2,
			"fixed simulation clock emits deterministic steps");
		context.Expect(clock.Advance(1.0, true, 3) == 3,
			"fixed simulation clock caps catch-up work");
		clock.Reset();
		clock.RequestStep();
		context.Expect(clock.Advance(0.0, false) == 1,
			"paused simulation supports one explicit step");

		Input::InputActionMap actions;
		actions.Bind("Move", {Input::BindingType::Key, 1, 1.0f});
		actions.Bind("Move", {Input::BindingType::Key, 2, -1.0f});
		actions.UpdateFromValues([](const Input::ActionBinding& binding)
			{ return binding.Code == 1 ? binding.Scale : 0.0f; });
		context.Expect(actions.WasPressed("Move") && actions.GetValue("Move") == 1.0f,
			"input actions combine bindings and expose pressed edges");
		actions.UpdateFromValues([](const Input::ActionBinding&) { return 0.0f; });
		context.Expect(actions.WasReleased("Move"), "input actions expose released edges");

		Scene::Scene editorScene("Editor");
		const entt::entity root = CreateSpatialEntity(editorScene, "Prefab Root");
		const entt::entity child = CreateSpatialEntity(editorScene, "Child");
		editorScene.SetParent(child, root);
		Scene::Scene playScene("Play");
		playScene.CopyFrom(editorScene);
		playScene.RenameEntity(playScene.FindEntityByID(editorScene.GetEntityID(root)), "Runtime Root");
		context.Expect(
			std::string(editorScene.GetComponent<Scene::TagComponent>(root)->Tag) == "Prefab Root",
			"play-scene copies are isolated from editor state");
		const auto sharedMaterial = std::make_shared<MaterialResource>();
		editorScene.AddComponent<Scene::MeshRendererComponent>(root).MaterialResourcePtr = sharedMaterial;
		editorScene.AddComponent<Scene::MeshRendererComponent>(child).MaterialResourcePtr = sharedMaterial;
		playScene.CopyFrom(editorScene);
		const auto* playRootRenderer = playScene.GetComponent<Scene::MeshRendererComponent>(
			playScene.FindEntityByID(editorScene.GetEntityID(root)));
		const auto* playChildRenderer = playScene.GetComponent<Scene::MeshRendererComponent>(
			playScene.FindEntityByID(editorScene.GetEntityID(child)));
		context.Expect(playRootRenderer && playChildRenderer
			&& playRootRenderer->MaterialResourcePtr == playChildRenderer->MaterialResourcePtr
			&& playRootRenderer->MaterialResourcePtr != sharedMaterial,
			"play-scene copies preserve material sharing while isolating editor resources");

		const auto prefabPath = std::filesystem::temp_directory_path() / "enigne_phase1.eprefab";
		std::string error;
		context.Expect(Editor::PrefabAsset::SaveFromScene(editorScene, root, prefabPath, &error),
			"entity subtrees save as prefab assets");
		Editor::PrefabAsset prefab;
		context.Expect(Editor::PrefabAsset::Load(prefabPath, prefab, {}, {}, {}, &error),
			"prefab assets load through versioned scene persistence");
		Editor::CommandStack commands;
		auto instantiate = std::make_unique<Editor::InstantiatePrefabCommand>(
			editorScene, std::move(prefab), 42, "assets/enigne_phase1.eprefab");
		auto* result = instantiate.get();
		commands.Execute(std::move(instantiate));
		const std::uint64_t instanceID = result->GetEntityID();
		const entt::entity instance = editorScene.FindEntityByID(instanceID);
		const auto* metadata = editorScene.GetComponent<Scene::PrefabInstanceComponent>(instance);
		context.Expect(metadata && metadata->IsRoot && metadata->PrefabAssetHandle == 42,
			"prefab instances retain asset identity and override metadata");
		commands.Undo();
		editorScene.FlushDestroyQueue();
		context.Expect(editorScene.FindEntityByID(instanceID) == entt::null,
			"prefab instantiation participates in undo");
		commands.Redo();
		const entt::entity restoredInstance = editorScene.FindEntityByID(instanceID);
		context.Expect(restoredInstance != entt::null,
			"prefab instantiation participates in redo");
		auto unpack = std::make_unique<Editor::UnpackPrefabCommand>(editorScene, restoredInstance);
		commands.Execute(std::move(unpack));
		context.Expect(!editorScene.HasComponent<Scene::PrefabInstanceComponent>(restoredInstance),
			"prefab instances can be unpacked");
		commands.Undo();
		context.Expect(editorScene.HasComponent<Scene::PrefabInstanceComponent>(restoredInstance),
			"unpacking a prefab instance is undoable");
		std::filesystem::remove(prefabPath);

		const auto assetRoot = std::filesystem::temp_directory_path() / "enigne_prefab_delete_test";
		std::filesystem::create_directories(assetRoot / "assets");
		const auto assetPath = assetRoot / "assets" / "delete.eprefab";
		{ std::ofstream output(assetPath); output << "{}"; }
		Graphics::AssetRegistry registry(assetRoot);
		const Graphics::AssetReference reference = registry.Register(
			assetPath, Graphics::AssetType::Prefab);
		Editor::CommandStack assetCommands;
		assetCommands.Execute(std::make_unique<Editor::DeleteAssetCommand>(registry, reference));
		context.Expect(!std::filesystem::exists(assetPath) && !registry.GetReference(reference.Handle).IsValid(),
			"prefab deletion moves the asset out of the registry");
		assetCommands.Undo();
		context.Expect(std::filesystem::exists(assetPath) && registry.GetReference(reference.Handle).IsValid(),
			"prefab deletion restores the asset and stable metadata on undo");
		std::filesystem::remove_all(assetRoot);

		Scene::Scene previewScene("Preview");
		const entt::entity preview = CreateSpatialEntity(previewScene, "Transient Preview");
		previewScene.AddComponent<Scene::TransientEditorComponent>(preview);
		const auto previewPath = std::filesystem::temp_directory_path() / "enigne_preview_filter.escene";
		context.Expect(Scene::SceneSerializer::Save(previewScene, previewPath),
			"scenes containing a drag preview still serialize");
		{
			std::ifstream input(previewPath);
			const nlohmann::json saved = nlohmann::json::parse(input);
			context.Expect(saved["entities"].empty(),
				"transient drag previews are excluded from scene persistence");
		}
		previewScene.RemoveComponent<Scene::TransientEditorComponent>(preview);
		const Editor::EntitySnapshot adopted = Editor::EntitySnapshot::CaptureSubtree(previewScene, preview);
		Editor::CommandStack previewCommands;
		previewCommands.RecordExecuted(std::make_unique<Editor::AdoptEntityCommand>(previewScene, adopted));
		previewCommands.Undo();
		previewScene.FlushDestroyQueue();
		context.Expect(previewScene.FindEntityByID(adopted.GetRootID()) == entt::null,
			"committed drag placement is undoable");
		previewCommands.Redo();
		context.Expect(previewScene.FindEntityByID(adopted.GetRootID()) != entt::null,
			"committed drag placement is redoable");
		std::filesystem::remove(previewPath);
	}

	void TestPhysicsComponentSlice(TestContext& context)
	{
		using namespace enignE;
		Scene::Scene scene("PhysicsComponents");
		const entt::entity entity = CreateSpatialEntity(scene, "Physics Cube", {0.0f, 4.0f, 0.0f});
		const std::uint64_t id = scene.GetEntityID(entity);
		Editor::CommandStack commands;
		commands.Execute(std::make_unique<Editor::AddComponentCommand>(
			scene, id, Scene::ComponentKind::RigidBody));
		commands.Execute(std::make_unique<Editor::AddComponentCommand>(
			scene, id, Scene::ComponentKind::Collider));
		context.Expect(scene.HasComponent<Scene::RigidBodyComponent>(entity)
			&& scene.HasComponent<Scene::ColliderComponent>(entity),
			"physics components are catalog-authored through undoable commands");

		Scene::RigidBodyComponent body = *scene.GetComponent<Scene::RigidBodyComponent>(entity);
		body.Motion = Scene::RigidBodyMotion::Dynamic;
		body.Friction = 0.72f;
		body.Restitution = 0.18f;
		commands.Execute(std::make_unique<Editor::SetRigidBodyCommand>(
			scene, id, *scene.GetComponent<Scene::RigidBodyComponent>(entity), body));
		Scene::ColliderComponent collider = *scene.GetComponent<Scene::ColliderComponent>(entity);
		collider.HalfExtents = {0.6f, 0.4f, 0.8f};
		commands.Execute(std::make_unique<Editor::SetColliderCommand>(
			scene, id, *scene.GetComponent<Scene::ColliderComponent>(entity), collider));

		Scene::Scene playCopy("PhysicsPlayCopy");
		playCopy.CopyFrom(scene);
		const entt::entity copied = playCopy.FindEntityByID(id);
		context.Expect(playCopy.HasComponent<Scene::RigidBodyComponent>(copied)
			&& playCopy.GetComponent<Scene::ColliderComponent>(copied)->HalfExtents.z == 0.8f,
			"Play scene copies preserve complete physics configuration");

		const auto path = std::filesystem::temp_directory_path() / "enigne_physics_components.escene";
		context.Expect(Scene::SceneSerializer::Save(scene, path), "physics components serialize");
		Scene::Scene loaded("LoadedPhysics");
		const bool loadedPhysics = Scene::SceneSerializer::Load(loaded, path);
		const entt::entity loadedEntity = loaded.FindEntityByID(id);
		context.Expect(loadedPhysics && loadedEntity != entt::null
			&& loaded.GetComponent<Scene::RigidBodyComponent>(loadedEntity)->Motion
				== Scene::RigidBodyMotion::Dynamic
			&& NearlyEqual(loaded.GetComponent<Scene::ColliderComponent>(loadedEntity)->HalfExtents.z, 0.8f),
			"physics configuration survives a scene round trip");
		std::filesystem::remove(path);

		const Editor::EntitySnapshot snapshot = Editor::EntitySnapshot::CaptureSubtree(scene, entity);
		scene.DestroyEntity(entity);
		scene.FlushDestroyQueue();
		const entt::entity restored = snapshot.Restore(scene);
		context.Expect(restored != entt::null
			&& scene.HasComponent<Scene::RigidBodyComponent>(restored)
			&& scene.HasComponent<Scene::ColliderComponent>(restored),
			"delete undo snapshots restore physics components");

		commands.Execute(std::make_unique<Editor::RemoveColliderCommand>(
			scene, id, *scene.GetComponent<Scene::ColliderComponent>(restored)));
		context.Expect(!scene.HasComponent<Scene::ColliderComponent>(restored),
			"collider removal is command-backed");
		commands.Undo();
		context.Expect(scene.HasComponent<Scene::ColliderComponent>(restored),
			"undo restores the complete collider value");
		const auto& rigidDescriptor = Scene::ComponentCatalog::Describe(Scene::ComponentKind::RigidBody);
		const auto& colliderDescriptor = Scene::ComponentCatalog::Describe(Scene::ComponentKind::Collider);
		context.Expect(rigidDescriptor.Persisted && rigidDescriptor.Snapshotted
			&& colliderDescriptor.Persisted && colliderDescriptor.Snapshotted,
			"component catalog records physics persistence contracts");

		const auto malformedPath =
			std::filesystem::temp_directory_path() / "enigne_malformed_physics.escene";
		{
			nlohmann::json malformed;
			malformed["scene"] = {{"name", "MalformedPhysics"}, {"version", 8},
				{"activeCamera", 0}, {"activeLight", 0}};
			malformed["entities"] = nlohmann::json::array();
			malformed["entities"].push_back({
				{"id", 1}, {"name", "Bad Body"}, {"parent", nullptr},
				{"components", {
					{"TransformComponent", {{"position", {0, 1, 0}}, {"rotation", {0, 0, 0}}, {"scale", {1, 1, 1}}}},
					{"RigidBodyComponent", {{"motion", 1}, {"enabled", true}}},
					{"ColliderComponent", {{"shape", 0}, {"halfExtents", {0.0, 0.5, 0.5}}, {"radius", 0.5}}}
				}}
			});
			std::ofstream stream(malformedPath);
			stream << malformed.dump(2);
		}
		Scene::Scene rejected("RejectedPhysics");
		context.Expect(!Scene::SceneSerializer::Load(rejected, malformedPath),
			"malformed collider data fails scene loading safely");
		std::filesystem::remove(malformedPath);
	}

	void TestJoltPhysicsWorld(TestContext& context)
	{
		using namespace enignE::Physics;
		const auto simulateDrop = [&context]()
		{
			PhysicsWorld world;
			PhysicsWorldConfig worldConfig;
			worldConfig.MaxBodies = 64;
			worldConfig.MaxBodyPairs = 64;
			worldConfig.MaxContactConstraints = 64;
			worldConfig.WorkerThreads = 1;
			worldConfig.TemporaryAllocatorBytes = 1024u * 1024u;
			context.Expect(world.Initialize(worldConfig), "Jolt physics world initializes");

			PhysicsBodyConfig floor;
			floor.EntityID = 100;
			floor.Motion = MotionType::Static;
			floor.Shape = ShapeType::Box;
			floor.Position = {0.0f, -0.5f, 0.0f};
			floor.HalfExtents = {5.0f, 0.5f, 5.0f};
			context.Expect(world.AddBody(floor), "physics world creates a static box floor");

			PhysicsBodyConfig body;
			body.EntityID = 200;
			body.Motion = MotionType::Dynamic;
			body.Shape = ShapeType::Box;
			body.Position = {0.0f, 3.0f, 0.0f};
			body.HalfExtents = {0.5f, 0.5f, 0.5f};
			context.Expect(world.AddBody(body), "physics world creates a dynamic box");
			context.Expect(!world.AddBody(body), "physics world rejects duplicate stable entity IDs");
			PhysicsRaycastHit raycastHit;
			context.Expect(world.Raycast(
				{0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f, raycastHit)
				&& raycastHit.EntityID == body.EntityID
				&& NearlyEqual(raycastHit.Distance, 1.5f)
				&& raycastHit.Normal.y > 0.99f,
				"physics raycasts return engine-owned stable IDs, distance, and surface normal");
			context.Expect(!world.Raycast(
				{0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 10.0f, raycastHit),
				"physics raycasts reject a zero direction safely");

			bool stepsSucceeded = true;
			bool sawContactBegin = false;
			bool sawContactPersist = false;
			std::size_t peakContactCount = 0;
			for (int step = 0; step < 240; ++step)
			{
				stepsSucceeded = world.Step(1.0f / 60.0f) && stepsSucceeded;
				const PhysicsWorldStats stats = world.GetStats();
				peakContactCount = std::max(peakContactCount, stats.ContactCount);
				context.Expect(stats.ContactEventCount == world.GetContactEvents().size(),
					"physics Stats match the normalized event batch");
				for (const PhysicsContactEvent& event : world.GetContactEvents())
				{
					if (event.Contact.EntityA != floor.EntityID
						|| event.Contact.EntityB != body.EntityID)
						continue;
					sawContactBegin |= event.Type == ContactEventType::Begin;
					sawContactPersist |= event.Type == ContactEventType::Persist;
					if (event.Type != ContactEventType::End)
					{
						context.Expect(event.Contact.PointCount > 0
							&& std::isfinite(event.Contact.Position.x)
							&& std::isfinite(event.Contact.Position.y)
							&& std::isfinite(event.Contact.Position.z)
							&& event.Contact.Position.y > -1.0f
							&& event.Contact.Position.y < 1.0f,
							"contact diagnostics expose a world-space manifold point");
					}
				}
			}
			context.Expect(stepsSucceeded, "Jolt fixed steps complete without errors");
			context.Expect(sawContactBegin && sawContactPersist && peakContactCount > 0,
				"contact delivery reports deterministic begin/persist phases and active counts");

			PhysicsBodyState state;
			context.Expect(world.GetBodyState(body.EntityID, state),
				"physics body state is queryable by stable ID");
			context.Expect(state.Position.y > 0.47f && state.Position.y < 0.51f,
				"dynamic box settles on the static floor (y="
					+ std::to_string(state.Position.y) + ")");
			context.Expect(world.GetStats().BodyCount == 2,
				"physics Stats report the live body count");
			context.Expect(world.RemoveBody(body.EntityID) && !world.ContainsBody(body.EntityID),
				"physics body removal clears the stable-ID mapping");
			world.Shutdown();
			context.Expect(!world.IsInitialized() && world.GetStats().BodyCount == 0,
				"physics world shutdown releases all runtime state");
			return state.Position;
		};

		const DirectX::XMFLOAT3 first = simulateDrop();
		const DirectX::XMFLOAT3 second = simulateDrop();
		context.Expect(NearlyEqual(first.x, second.x) && NearlyEqual(first.y, second.y)
			&& NearlyEqual(first.z, second.z),
			"fresh Jolt worlds reproduce the fixed-step box drop deterministically");

		PhysicsWorld shapeWorld;
		PhysicsWorldConfig shapeConfig;
		shapeConfig.MaxBodies = 16;
		shapeConfig.MaxBodyPairs = 16;
		shapeConfig.MaxContactConstraints = 16;
		shapeConfig.TemporaryAllocatorBytes = 1024u * 1024u;
		context.Expect(shapeWorld.Initialize(shapeConfig),
			"physics world can be re-created after prior teardown");
		PhysicsBodyConfig sphere;
		sphere.EntityID = 300;
		sphere.Motion = MotionType::Dynamic;
		sphere.Shape = ShapeType::Sphere;
		sphere.Radius = 0.5f;
		context.Expect(shapeWorld.AddBody(sphere), "physics adapter supports sphere bodies");
		PhysicsBodyConfig kinematic;
		kinematic.EntityID = 302;
		kinematic.Motion = MotionType::Kinematic;
		kinematic.Shape = ShapeType::Box;
		context.Expect(shapeWorld.AddBody(kinematic)
			&& shapeWorld.SetBodyTransform(
				kinematic.EntityID, {2.0f, 3.0f, 4.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f / 60.0f),
			"kinematic bodies accept ECS-authoritative transforms before a fixed step");
		PhysicsBodyState kinematicState;
		context.Expect(shapeWorld.Step(1.0f / 60.0f)
			&& shapeWorld.GetBodyState(kinematic.EntityID, kinematicState)
			&& NearlyEqual(kinematicState.Position.x, 2.0f)
			&& NearlyEqual(kinematicState.Position.y, 3.0f),
			"kinematic target state remains queryable by stable ID");
		sphere.EntityID = 301;
		sphere.Radius = 0.0f;
		context.Expect(!shapeWorld.AddBody(sphere),
			"physics adapter rejects invalid shape dimensions");
	}

	void TestFallingCubesScene(TestContext& context)
	{
		using namespace enignE;
		const std::filesystem::path scenePath =
			std::filesystem::path(__FILE__).parent_path().parent_path()
			/ "scenes" / "falling-cubes.escene";
		Scene::Scene scene("FallingCubesTest");
		context.Expect(Scene::SceneSerializer::Load(scene, scenePath),
			"falling-cubes sample loads through the production serializer");

		Physics::PhysicsWorld world;
		context.Expect(world.Initialize(), "falling-cubes physics world initializes");
		auto bodies = scene.View<Scene::IDComponent, Scene::TransformComponent,
			Scene::RigidBodyComponent, Scene::ColliderComponent>();
		std::size_t dynamicCount = 0;
		for (const entt::entity entity : bodies)
		{
			const auto& transform = bodies.get<Scene::TransformComponent>(entity);
			const auto& body = bodies.get<Scene::RigidBodyComponent>(entity);
			const auto& collider = bodies.get<Scene::ColliderComponent>(entity);
			Physics::PhysicsBodyConfig config;
			config.EntityID = scene.GetEntityID(entity);
			config.Position = transform.GetLocalPosition();
			config.Rotation = transform.GetLocalRotationQuaternion();
			config.Motion = body.Motion == Scene::RigidBodyMotion::Dynamic
				? Physics::MotionType::Dynamic
				: body.Motion == Scene::RigidBodyMotion::Kinematic
					? Physics::MotionType::Kinematic : Physics::MotionType::Static;
			config.Shape = collider.Shape == Scene::ColliderShape::Sphere
				? Physics::ShapeType::Sphere : Physics::ShapeType::Box;
			config.HalfExtents = collider.HalfExtents;
			config.Radius = collider.Radius;
			config.Friction = body.Friction;
			config.Restitution = body.Restitution;
			config.LinearDamping = body.LinearDamping;
			config.AngularDamping = body.AngularDamping;
			config.GravityFactor = body.GravityFactor;
			context.Expect(world.AddBody(config),
				"falling-cubes sample body enters the physics world");
			if (config.Motion == Physics::MotionType::Dynamic) ++dynamicCount;
		}
		context.Expect(world.GetStats().BodyCount == 10 && dynamicCount == 9,
			"falling-cubes sample contains one ground body and nine dynamic cubes");
		bool stepped = true;
		for (int step = 0; step < 600; ++step)
			stepped = world.Step(1.0f / 60.0f) && stepped;
		context.Expect(stepped, "falling-cubes sample completes ten seconds of fixed simulation");
		for (const entt::entity entity : bodies)
		{
			const auto& body = bodies.get<Scene::RigidBodyComponent>(entity);
			if (body.Motion != Scene::RigidBodyMotion::Dynamic) continue;
			Physics::PhysicsBodyState state;
			context.Expect(world.GetBodyState(scene.GetEntityID(entity), state)
				&& state.Position.y > 0.45f && state.Position.y < 10.0f,
				"sample cube falls and remains supported by the ground/cube pile");
		}
	}

	void TestPhysicsStressScene(TestContext& context)
	{
		using namespace enignE;
		const std::filesystem::path scenePath =
			std::filesystem::path(__FILE__).parent_path().parent_path()
			/ "scenes" / "physics-stress.escene";
		Scene::Scene scene("PhysicsStressTest");
		context.Expect(Scene::SceneSerializer::Load(scene, scenePath),
			"744-body physics stress scene loads through the production serializer");

		Physics::PhysicsWorld world;
		context.Expect(world.Initialize(), "physics stress world initializes");
		auto bodies = scene.View<Scene::IDComponent, Scene::TransformComponent,
			Scene::RigidBodyComponent, Scene::ColliderComponent>();
		std::size_t dynamicCount = 0;
		float trackedInitialHeight = 0.0f;
		for (const entt::entity entity : bodies)
		{
			const auto& transform = bodies.get<Scene::TransformComponent>(entity);
			const auto& body = bodies.get<Scene::RigidBodyComponent>(entity);
			const auto& collider = bodies.get<Scene::ColliderComponent>(entity);
			Physics::PhysicsBodyConfig config;
			config.EntityID = scene.GetEntityID(entity);
			config.Position = transform.GetLocalPosition();
			config.Rotation = transform.GetLocalRotationQuaternion();
			config.Motion = body.Motion == Scene::RigidBodyMotion::Dynamic
				? Physics::MotionType::Dynamic
				: body.Motion == Scene::RigidBodyMotion::Kinematic
					? Physics::MotionType::Kinematic : Physics::MotionType::Static;
			config.Shape = collider.Shape == Scene::ColliderShape::Sphere
				? Physics::ShapeType::Sphere : Physics::ShapeType::Box;
			config.HalfExtents = collider.HalfExtents;
			config.Radius = collider.Radius;
			config.Friction = body.Friction;
			config.Restitution = body.Restitution;
			config.LinearDamping = body.LinearDamping;
			config.AngularDamping = body.AngularDamping;
			config.GravityFactor = body.GravityFactor;
			context.Expect(world.AddBody(config),
				"stress-scene body enters the physics world");
			if (config.Motion == Physics::MotionType::Dynamic) ++dynamicCount;
			if (config.EntityID == 7100) trackedInitialHeight = config.Position.y;
		}
		context.Expect(world.GetStats().BodyCount == 750 && dynamicCount == 744,
			"stress scene supplies 744 dynamic bodies and six static colliders");
		bool stepped = true;
		std::size_t peakContacts = 0;
		std::size_t deliveredEvents = 0;
		for (int step = 0; step < 240; ++step)
		{
			stepped = world.Step(1.0f / 60.0f) && stepped;
			peakContacts = std::max(peakContacts, world.GetStats().ContactCount);
			const auto& events = world.GetContactEvents();
			deliveredEvents += events.size();
			for (std::size_t index = 1; index < events.size(); ++index)
			{
				context.Expect(events[index - 1].Contact.EntityA < events[index].Contact.EntityA
					|| (events[index - 1].Contact.EntityA == events[index].Contact.EntityA
						&& events[index - 1].Contact.EntityB <= events[index].Contact.EntityB),
					"contact event batches are normalized into stable-ID order");
			}
		}
		Physics::PhysicsBodyState trackedState;
		context.Expect(stepped && peakContacts >= 400 && deliveredEvents > dynamicCount,
			"stress scene completes four seconds with sustained observable contacts");
		context.Expect(world.GetBodyState(7100, trackedState)
			&& trackedInitialHeight - trackedState.Position.y > 2.0f,
			"elevated collapse field falls far enough to produce visible motion");
	}
}

int main()
{
	TestContext context;
	TestHierarchy(context);
	TestTransforms(context);
	TestDirtyTransformPropagation(context);
	TestDestruction(context);
	TestPrimitiveCaching(context);
	TestRenderInvalidation(context);
	TestPerChunkDirtyTracking(context);
	TestRenderChunkBoundsAndCulling(context);
	TestInstanceDataEncoding(context);
	TestStableIDsAndVersions(context);
	TestJobSystemAndParallelTransforms(context);
	TestSerializationAndCommands(context);
	TestMalformedSceneLoad(context);
	TestSceneMigrations(context);
	TestCameraLightCommandsAndEdges(context);
	TestAssetPathsAndTextureFailures(context);
	TestProjectAndMetadataIdentity(context);
	TestMaterialAssetRoundTrip(context);
	TestCommandStackDirtyRevisions(context);
	TestClipboardSnapshotPaste(context);
	TestSnapshotEntityCommands(context);
	TestRecordedTransformCommand(context);
	TestShadowConfiguration(context);
	TestFixedStepRotator(context);
	TestRotatorAuthoringCommands(context);
	TestFlyController(context);
	TestPhysicsComponentSlice(context);
	TestInstanceSubmissionPlanning(context);
	TestViewportRenderPlanning(context);
	TestPhaseOneFoundation(context);
	TestJoltPhysicsWorld(context);
	TestFallingCubesScene(context);
	TestPhysicsStressScene(context);

	if (context.Failures != 0)
	{
		std::cerr << context.Failures << " regression test(s) failed\n";
		return 1;
	}

	std::cout << "Scene regression tests passed\n";
	return 0;
}
