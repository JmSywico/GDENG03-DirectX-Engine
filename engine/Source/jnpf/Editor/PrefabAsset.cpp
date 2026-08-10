#include "Editor/PrefabAsset.h"

namespace jnpf::Editor
{
	bool PrefabAsset::SaveFromScene(
		Scene::Scene& scene,
		entt::entity root,
		const std::filesystem::path& path,
		std::string* error,
		const Scene::SceneSerializer::PathNormalizer& pathNormalizer)
	{
		const EntitySnapshot snapshot = EntitySnapshot::CaptureSubtree(scene, root);
		if (snapshot.Empty())
		{
			if (error) *error = "prefab root is invalid";
			return false;
		}
		for (const EntitySnapshotNode& node : snapshot.GetNodes())
		{
			if (node.PrefabInstance)
			{
				if (error) *error = "nested prefab capture is not supported; unpack it first";
				return false;
			}
		}
		Scene::Scene prefabScene("Prefab");
		if (snapshot.Restore(prefabScene) == entt::null
			|| !Scene::SceneSerializer::Save(prefabScene, path, pathNormalizer))
		{
			if (error) *error = "prefab could not be written";
			return false;
		}
		return true;
	}

	bool PrefabAsset::Load(
		const std::filesystem::path& path,
		PrefabAsset& result,
		const Scene::SceneSerializer::PrimitiveResolver& primitiveResolver,
		const Scene::SceneSerializer::ModelResolver& modelResolver,
		const Scene::SceneSerializer::TextureResolver& textureResolver,
		std::string* error)
	{
		Scene::Scene prefabScene("Prefab");
		if (!Scene::SceneSerializer::Load(
			prefabScene, path, primitiveResolver, modelResolver, textureResolver))
		{
			if (error) *error = "prefab could not be loaded";
			return false;
		}
		entt::entity root = entt::null;
		for (const entt::entity entity : prefabScene.GetAllEntities())
		{
			if (prefabScene.GetParent(entity) != entt::null) continue;
			if (root != entt::null)
			{
				if (error) *error = "prefab must contain exactly one root";
				return false;
			}
			root = entity;
		}
		if (root == entt::null)
		{
			if (error) *error = "prefab contains no root";
			return false;
		}
		result.m_snapshot = EntitySnapshot::CaptureSubtree(prefabScene, root);
		return !result.m_snapshot.Empty();
	}

	EntitySnapshot PrefabAsset::Instantiate(
		Scene::Scene& scene,
		std::uint64_t assetHandle,
		std::string assetPath,
		std::uint64_t parentID) const
	{
		EntitySnapshot instance = m_snapshot.Instantiate(scene, parentID);
		const auto& sourceNodes = m_snapshot.GetNodes();
		const auto& instanceNodes = instance.GetNodes();
		if (sourceNodes.size() != instanceNodes.size()) return {};
		for (std::size_t index = 0; index < instanceNodes.size(); ++index)
		{
			const entt::entity entity = scene.FindEntityByID(instanceNodes[index].ID);
			if (entity == entt::null) return {};
			Scene::PrefabInstanceComponent metadata;
			metadata.PrefabAssetHandle = assetHandle;
			metadata.PrefabAssetPath = assetPath;
			metadata.PrefabLocalID = sourceNodes[index].ID;
			metadata.IsRoot = index == 0;
			scene.AddComponent<Scene::PrefabInstanceComponent>(entity, std::move(metadata));
		}
		return EntitySnapshot::CaptureSubtree(
			scene, scene.FindEntityByID(instance.GetRootID()));
	}
}
