#pragma once

#include "Editor/Commands/EntitySnapshot.h"
#include "Scene/Serialization/SceneSerializer.h"

#include <filesystem>
#include <string>

namespace enignE::Editor
{
	class PrefabAsset
	{
	public:
		static bool SaveFromScene(
			Scene::Scene& scene,
			entt::entity root,
			const std::filesystem::path& path,
			std::string* error = nullptr,
			const Scene::SceneSerializer::PathNormalizer& pathNormalizer = {});
		static bool Load(
			const std::filesystem::path& path,
			PrefabAsset& result,
			const Scene::SceneSerializer::PrimitiveResolver& primitiveResolver = {},
			const Scene::SceneSerializer::ModelResolver& modelResolver = {},
			const Scene::SceneSerializer::TextureResolver& textureResolver = {},
			std::string* error = nullptr);

		EntitySnapshot Instantiate(
			Scene::Scene& scene,
			std::uint64_t assetHandle,
			std::string assetPath,
			std::uint64_t parentID = 0) const;
		bool Empty() const { return m_snapshot.Empty(); }

	private:
		EntitySnapshot m_snapshot;
	};
}
