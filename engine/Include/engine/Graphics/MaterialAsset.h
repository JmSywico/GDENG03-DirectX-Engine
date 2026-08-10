#pragma once

#include "Material.h"

#include <filesystem>
#include <functional>

namespace jnpf::Graphics
{
	class MaterialAsset
	{
	public:
		using TextureResolver =
			std::function<std::shared_ptr<Texture2D>(std::uint64_t, const std::string&)>;

		static bool Save(const MaterialResource& material, const std::filesystem::path& path);
		static bool Load(
			const std::filesystem::path& path,
			MaterialResource& material,
			TextureResolver textureResolver = {});
	};
}
