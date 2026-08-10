#include "Graphics/MaterialAsset.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace enignE::Graphics
{
	namespace
	{
		nlohmann::json TextureReference(std::uint64_t handle, const std::string& path)
		{
			return {{"handle", handle}, {"path", path}};
		}
	}

	bool MaterialAsset::Save(const MaterialResource& material, const std::filesystem::path& path)
	{
		const nlohmann::json document = {
			{"version", 1},
			{"albedo", {material.Albedo.x, material.Albedo.y, material.Albedo.z, material.Albedo.w}},
			{"emissive", {material.Emissive.x, material.Emissive.y, material.Emissive.z}},
			{"metallic", material.Metallic},
			{"roughness", material.Roughness},
			{"mode", static_cast<std::uint32_t>(material.Mode)},
			{"albedoTexture", TextureReference(material.AlbedoTextureHandle, material.AlbedoTexturePath)},
			{"normalTexture", TextureReference(material.NormalTextureHandle, material.NormalTexturePath)},
			{"metallicRoughnessTexture", TextureReference(
				material.MetallicRoughnessTextureHandle,
				material.MetallicRoughnessTexturePath)}
		};
		std::error_code error;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		return output && static_cast<bool>(output << document.dump(2));
	}

	bool MaterialAsset::Load(
		const std::filesystem::path& path,
		MaterialResource& material,
		TextureResolver textureResolver)
	{
		try
		{
			std::ifstream input(path, std::ios::binary);
			const nlohmann::json document = nlohmann::json::parse(input);
			if (document.at("version").get<int>() != 1)
				return false;
			const auto& albedo = document.at("albedo");
			const auto& emissive = document.at("emissive");
			MaterialResource loaded;
			loaded.Albedo = {albedo.at(0), albedo.at(1), albedo.at(2), albedo.at(3)};
			loaded.Emissive = {emissive.at(0), emissive.at(1), emissive.at(2)};
			loaded.Metallic = std::clamp(document.value("metallic", 0.0f), 0.0f, 1.0f);
			loaded.Roughness = std::clamp(document.value("roughness", 1.0f), 0.04f, 1.0f);
			loaded.Mode = static_cast<MaterialMode>(document.value("mode", 0u));
			const auto readTexture = [&](const char* key, std::uint64_t& handle,
				std::string& assetPath, std::shared_ptr<Texture2D>& texture)
			{
				if (!document.contains(key)) return;
				const auto& reference = document.at(key);
				handle = reference.value("handle", std::uint64_t{0});
				assetPath = reference.value("path", std::string{});
				if (textureResolver && (handle != 0 || !assetPath.empty()))
					texture = textureResolver(handle, assetPath);
			};
			readTexture("albedoTexture", loaded.AlbedoTextureHandle,
				loaded.AlbedoTexturePath, loaded.AlbedoTexture);
			readTexture("normalTexture", loaded.NormalTextureHandle,
				loaded.NormalTexturePath, loaded.NormalTexture);
			readTexture("metallicRoughnessTexture", loaded.MetallicRoughnessTextureHandle,
				loaded.MetallicRoughnessTexturePath, loaded.MetallicRoughnessTexture);
			material = std::move(loaded);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
}
