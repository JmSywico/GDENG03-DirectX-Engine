#pragma once

#include "../../Scene/Scene.h"

#include <optional>
#include <string>
#include <vector>

namespace jnpf::Editor
{
	/**
	 * @brief Serializable-in-memory state for one entity in an editor snapshot.
	 * @ingroup editor
	 *
	 * Runtime model shared pointers may be retained for undo/redo, but snapshots
	 * are not the on-disk scene format.
	 */
	struct EntitySnapshotNode
	{
		std::uint64_t ID = 0;
		std::uint64_t ParentID = 0;
		std::string Name;
		bool HasHierarchy = false;
		std::optional<Scene::TransformComponent> Transform;
		std::optional<Scene::MeshRendererComponent> MeshRenderer;
		std::optional<Scene::RenderSourceComponent> RenderSource;
		std::optional<Scene::CameraComponent> Camera;
		std::optional<Scene::LightComponent> Light;
		std::optional<Scene::RotatorComponent> Rotator;
		std::optional<Scene::FlyControllerComponent> FlyController;
		std::optional<Scene::RigidBodyComponent> RigidBody;
		std::optional<Scene::ColliderComponent> Collider;
		std::optional<Scene::PrefabInstanceComponent> PrefabInstance;
		bool WasActiveCamera = false;
		bool WasActiveLight = false;
	};

	/**
	 * @brief Captures and restores a fixed set of components for an entity subtree.
	 * @ingroup editor
	 *
	 * Restoration creates all entities by stable ID before rebuilding hierarchy
	 * links. Add new component types here when they must survive delete/undo.
	 */
	class EntitySnapshot
	{
	public:
		static EntitySnapshot CaptureSubtree(const Scene::Scene& scene, entt::entity root);
		static EntitySnapshot DuplicateSubtree(Scene::Scene& scene, entt::entity root);
		EntitySnapshot Duplicate(Scene::Scene& scene) const;
		EntitySnapshot Instantiate(
			Scene::Scene& scene,
			std::uint64_t parentID = 0,
			bool appendCopySuffix = false) const;

		entt::entity Restore(Scene::Scene& scene) const;
		std::uint64_t GetRootID() const;
		const std::vector<EntitySnapshotNode>& GetNodes() const { return m_nodes; }
		bool Empty() const { return m_nodes.empty(); }

	private:
		std::vector<EntitySnapshotNode> m_nodes;
	};
}
