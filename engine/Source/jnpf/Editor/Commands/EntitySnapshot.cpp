#include "Editor/Commands/EntitySnapshot.h"

#include <unordered_map>

namespace jnpf::Editor
{
	EntitySnapshot EntitySnapshot::CaptureSubtree(const Scene::Scene& scene, entt::entity root)
	{
		EntitySnapshot snapshot;
		if (!scene.IsEntityValid(root))
			return snapshot;

		const auto capture = [&scene, &snapshot](entt::entity entity)
		{
			EntitySnapshotNode node;
			node.ID = scene.GetEntityID(entity);
			node.ParentID = scene.GetEntityID(scene.GetParent(entity));
			if (const auto* tag = scene.GetComponent<Scene::TagComponent>(entity))
				node.Name = tag->Tag;
			node.HasHierarchy = scene.HasComponent<Scene::HierarchyComponent>(entity);
			if (const auto* component = scene.GetComponent<Scene::TransformComponent>(entity))
				node.Transform = *component;
			if (const auto* component = scene.GetComponent<Scene::MeshRendererComponent>(entity))
				node.MeshRenderer = *component;
			if (const auto* component = scene.GetComponent<Scene::RenderSourceComponent>(entity))
				node.RenderSource = *component;
			if (const auto* component = scene.GetComponent<Scene::CameraComponent>(entity))
				node.Camera = *component;
			if (const auto* component = scene.GetComponent<Scene::LightComponent>(entity))
				node.Light = *component;
			if (const auto* component = scene.GetComponent<Scene::RotatorComponent>(entity))
				node.Rotator = *component;
			if (const auto* component = scene.GetComponent<Scene::FlyControllerComponent>(entity))
				node.FlyController = *component;
			if (const auto* component = scene.GetComponent<Scene::RigidBodyComponent>(entity))
				node.RigidBody = *component;
			if (const auto* component = scene.GetComponent<Scene::ColliderComponent>(entity))
				node.Collider = *component;
			if (const auto* component = scene.GetComponent<Scene::PrefabInstanceComponent>(entity))
				node.PrefabInstance = *component;
			node.WasActiveCamera = scene.GetSettings().ActiveCameraEntityID == node.ID;
			node.WasActiveLight = scene.GetSettings().ActiveLightEntityID == node.ID;
			snapshot.m_nodes.push_back(std::move(node));
		};

		capture(root);
		const auto captureDescendants = [&scene, &capture](auto&& self, entt::entity parent) -> void
		{
			for (const entt::entity child : scene.GetChildren(parent))
			{
				capture(child);
				self(self, child);
			}
		};
		captureDescendants(captureDescendants, root);
		return snapshot;
	}

	EntitySnapshot EntitySnapshot::DuplicateSubtree(Scene::Scene& scene, entt::entity root)
	{
		const EntitySnapshot source = CaptureSubtree(scene, root);
		return source.Duplicate(scene);
	}

	EntitySnapshot EntitySnapshot::Duplicate(Scene::Scene& scene) const
	{
		return Instantiate(scene, m_nodes.empty() ? 0 : m_nodes.front().ParentID, true);
	}

	EntitySnapshot EntitySnapshot::Instantiate(
		Scene::Scene& scene,
		std::uint64_t parentID,
		bool appendCopySuffix) const
	{
		const EntitySnapshot& source = *this;
		if (m_nodes.empty())
			return {};

		std::unordered_map<std::uint64_t, entt::entity> duplicates;
		for (const EntitySnapshotNode& node : source.m_nodes)
		{
			std::string name = node.Name;
			if (node.ID == source.m_nodes.front().ID && appendCopySuffix)
				name += " Copy";
			const entt::entity entity = scene.CreateEntity(name);
			if (entity == entt::null)
				return {};
			duplicates.emplace(node.ID, entity);
			if (node.Transform)
				scene.AddComponent<Scene::TransformComponent>(entity, *node.Transform);
			if (node.HasHierarchy)
				scene.AddComponent<Scene::HierarchyComponent>(entity);
			if (node.MeshRenderer)
				scene.AddComponent<Scene::MeshRendererComponent>(entity, *node.MeshRenderer);
			if (node.RenderSource)
				scene.AddComponent<Scene::RenderSourceComponent>(entity, *node.RenderSource);
			if (node.Camera)
				scene.AddComponent<Scene::CameraComponent>(entity, *node.Camera);
			if (node.Light)
				scene.AddComponent<Scene::LightComponent>(entity, *node.Light);
			if (node.Rotator)
				scene.AddComponent<Scene::RotatorComponent>(entity, *node.Rotator);
			if (node.FlyController)
				scene.AddComponent<Scene::FlyControllerComponent>(entity, *node.FlyController);
			if (node.RigidBody)
				scene.AddComponent<Scene::RigidBodyComponent>(entity, *node.RigidBody);
			if (node.Collider)
				scene.AddComponent<Scene::ColliderComponent>(entity, *node.Collider);
			if (node.PrefabInstance)
				scene.AddComponent<Scene::PrefabInstanceComponent>(entity, *node.PrefabInstance);
		}

		for (auto nodeIt = source.m_nodes.rbegin(); nodeIt != source.m_nodes.rend(); ++nodeIt)
		{
			const EntitySnapshotNode& node = *nodeIt;
			entt::entity parent = entt::null;
			if (const auto duplicateParent = duplicates.find(node.ParentID);
				duplicateParent != duplicates.end())
			{
				parent = duplicateParent->second;
			}
			else if (node.ID == source.m_nodes.front().ID && parentID != 0)
			{
				parent = scene.FindEntityByID(parentID);
			}
			if (parent != entt::null)
				scene.SetParent(duplicates.at(node.ID), parent);
		}

		return CaptureSubtree(scene, duplicates.at(source.m_nodes.front().ID));
	}

	entt::entity EntitySnapshot::Restore(Scene::Scene& scene) const
	{
		if (m_nodes.empty())
			return entt::null;

		const entt::entity existingRoot = scene.FindEntityByID(m_nodes.front().ID);
		if (existingRoot != entt::null)
		{
			scene.CancelDestroyEntityRecursive(existingRoot);
			return existingRoot;
		}

		for (const EntitySnapshotNode& node : m_nodes)
		{
			const entt::entity entity = scene.CreateEntityWithID(node.ID, node.Name);
			if (entity == entt::null)
				return entt::null;
			if (node.Transform)
				scene.AddComponent<Scene::TransformComponent>(entity, *node.Transform);
			if (node.HasHierarchy)
				scene.AddComponent<Scene::HierarchyComponent>(entity);
			if (node.MeshRenderer)
				scene.AddComponent<Scene::MeshRendererComponent>(entity, *node.MeshRenderer);
			if (node.RenderSource)
				scene.AddComponent<Scene::RenderSourceComponent>(entity, *node.RenderSource);
			if (node.Camera)
				scene.AddComponent<Scene::CameraComponent>(entity, *node.Camera);
			if (node.Light)
				scene.AddComponent<Scene::LightComponent>(entity, *node.Light);
			if (node.Rotator)
				scene.AddComponent<Scene::RotatorComponent>(entity, *node.Rotator);
			if (node.FlyController)
				scene.AddComponent<Scene::FlyControllerComponent>(entity, *node.FlyController);
			if (node.RigidBody)
				scene.AddComponent<Scene::RigidBodyComponent>(entity, *node.RigidBody);
			if (node.Collider)
				scene.AddComponent<Scene::ColliderComponent>(entity, *node.Collider);
			if (node.PrefabInstance)
				scene.AddComponent<Scene::PrefabInstanceComponent>(entity, *node.PrefabInstance);
		}

		// Hierarchy insertion prepends children, so restore in reverse capture order.
		for (auto nodeIt = m_nodes.rbegin(); nodeIt != m_nodes.rend(); ++nodeIt)
		{
			const EntitySnapshotNode& node = *nodeIt;
			if (node.ParentID == 0)
				continue;
			const entt::entity child = scene.FindEntityByID(node.ID);
			const entt::entity parent = scene.FindEntityByID(node.ParentID);
			if (child != entt::null && parent != entt::null)
				scene.SetParent(child, parent);
		}
		for (const EntitySnapshotNode& node : m_nodes)
		{
			if (node.WasActiveCamera)
				scene.SetActiveCameraEntityID(node.ID);
			if (node.WasActiveLight)
				scene.SetActiveLightEntityID(node.ID);
		}
		return scene.FindEntityByID(m_nodes.front().ID);
	}

	std::uint64_t EntitySnapshot::GetRootID() const
	{
		return m_nodes.empty() ? 0 : m_nodes.front().ID;
	}
}
