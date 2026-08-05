#pragma once

#include "../SceneFactory.h"

#include <filesystem>
#include <functional>
#include <memory>

class Model;
class Texture2D;

namespace enignE::Scene
{
	class Scene;

	/**
	 * @brief Saves and reconstructs persistent scene data as JSON.
	 * @ingroup scene
	 *
	 * Stable entity IDs and parent IDs are serialized. EnTT handles, shared
	 * model pointers, DX11 handles, and renderer caches are never persisted.
	 */
	class SceneSerializer
	{
public:
		/** @brief Reconstructs procedural geometry described by a PrimitiveDesc. */
		using PrimitiveResolver = std::function<std::shared_ptr<Model>(const PrimitiveDesc&)>;
		/** @brief Resolves an imported model from its serialized asset path. */
		using ModelResolver =
			std::function<std::shared_ptr<Model>(std::uint64_t, const std::string&)>;
		using TextureResolver =
			std::function<std::shared_ptr<Texture2D>(std::uint64_t, const std::string&)>;
		using PathNormalizer = std::function<std::string(const std::string&)>;

		/** @return true when the complete JSON document is written successfully. */
		static bool Save(
			Scene& scene,
			const std::filesystem::path& path,
			const PathNormalizer& pathNormalizer = {});
		/**
		 * @brief Loads scene data without model reconstruction callbacks.
		 * @note Render sources that need a resolver may remain without a runtime model.
		 */
		static bool Load(Scene& scene, const std::filesystem::path& path);
		/**
		 * @brief Loads a scene and reconstructs procedural and imported models.
		 *
		 * Entities are created before parent relationships are resolved. Scene
		 * transform and render state is invalidated after reconstruction.
		 */
		static bool Load(
			Scene& scene,
			const std::filesystem::path& path,
			const PrimitiveResolver& primitiveResolver,
			const ModelResolver& modelResolver,
			const TextureResolver& textureResolver = {});
	};
}
