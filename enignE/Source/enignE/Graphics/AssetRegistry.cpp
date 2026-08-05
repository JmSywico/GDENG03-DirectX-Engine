#include "Graphics/AssetRegistry.h"

#include "Graphics/ModelLoader.h"
#include "Graphics/Model.h"
#include "Graphics/Texture2D.h"
#include "Core/JobSystem.h"

#include <system_error>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <chrono>
#include <random>
#include <nlohmann/json.hpp>

namespace enignE::Graphics
{
	AssetRegistry::AssetRegistry(std::filesystem::path projectRoot)
	{
		SetProjectRoot(std::move(projectRoot));
	}

	void AssetRegistry::SetProjectRoot(std::filesystem::path projectRoot)
	{
		std::error_code error;
		if (projectRoot.empty())
			projectRoot = std::filesystem::current_path(error);
		m_projectRoot = std::filesystem::weakly_canonical(projectRoot, error);
		if (error)
			m_projectRoot = std::filesystem::absolute(projectRoot);
	}

	std::filesystem::path AssetRegistry::ResolvePath(const std::filesystem::path& path) const
	{
		if (path.empty())
			return {};
		return path.is_absolute() ? path.lexically_normal() : (m_projectRoot / path).lexically_normal();
	}

	std::string AssetRegistry::MakeProjectRelative(const std::filesystem::path& path) const
	{
		if (path.empty())
			return {};
		const std::filesystem::path absolute = ResolvePath(path);
		std::error_code error;
		const std::filesystem::path relative = std::filesystem::relative(absolute, m_projectRoot, error);
		if (!error && !relative.empty() && *relative.begin() != "..")
			return relative.generic_string();
		return absolute.generic_string();
	}

	AssetHandle AssetRegistry::HashPath(const std::string& normalizedPath)
	{
		constexpr AssetHandle offset = 14695981039346656037ull;
		constexpr AssetHandle prime = 1099511628211ull;
		AssetHandle result = offset;
		for (const unsigned char character : normalizedPath)
		{
			result ^= static_cast<unsigned char>(std::tolower(character));
			result *= prime;
		}
		return result == InvalidAssetHandle ? 1 : result;
	}

	AssetHandle AssetRegistry::RegisterPath(const std::filesystem::path& path)
	{
		return Register(path, AssetType::Unknown).Handle;
	}

	AssetReference AssetRegistry::RegisterReference(const AssetReference& reference)
	{
		if (reference.Path.empty())
			return GetReference(reference.Handle);
		AssetReference registered = Register(reference.Path, reference.Type);
		if (reference.Handle != InvalidAssetHandle && registered.Handle != reference.Handle)
			m_diagnostics.push_back({
				AssetDiagnostic::Severity::Warning,
				"Migrated legacy path-based asset handle to metadata identity",
				registered.Path});
		return registered;
	}

	bool AssetRegistry::RegisterWithHandle(
		AssetHandle handle,
		const std::string& path,
		AssetType type)
	{
		if (handle == InvalidAssetHandle || path.empty())
			return false;
		if (const auto existing = m_paths.find(handle);
			existing != m_paths.end() && existing->second != path)
		{
			if (std::filesystem::exists(ResolvePath(existing->second))
				&& std::filesystem::exists(ResolvePath(path)))
			{
				m_diagnostics.push_back({
					AssetDiagnostic::Severity::Error,
					"Duplicate asset identity; another live path already owns this handle",
					path});
				return false;
			}
			m_diagnostics.push_back({
				AssetDiagnostic::Severity::Warning,
				"Updated a moved asset path from its persistent metadata",
				path});
		}
		for (auto iterator = m_paths.begin(); iterator != m_paths.end();)
		{
			if (iterator->first != handle && iterator->second == path)
			{
				m_types.erase(iterator->first);
				m_models.erase(iterator->first);
				m_textures.erase(iterator->first);
				iterator = m_paths.erase(iterator);
			}
			else
				++iterator;
		}
		m_paths.insert_or_assign(handle, path);
		if (type != AssetType::Unknown)
			m_types.insert_or_assign(handle, type);
		return true;
	}

	AssetHandle AssetRegistry::ReadOrCreateMetadata(
		const std::filesystem::path& absolutePath,
		AssetType type)
	{
		std::error_code error;
		if (!std::filesystem::is_regular_file(absolutePath, error))
			return InvalidAssetHandle;
		const std::filesystem::path metadataPath = absolutePath.string() + ".meta";
		if (std::filesystem::is_regular_file(metadataPath, error))
		{
			try
			{
				std::ifstream input(metadataPath, std::ios::binary);
				const nlohmann::json metadata = nlohmann::json::parse(input);
				const AssetHandle handle = metadata.at("id").get<AssetHandle>();
				if (handle != InvalidAssetHandle)
					return handle;
			}
			catch (...)
			{
				m_diagnostics.push_back({
					AssetDiagnostic::Severity::Error,
					"Asset metadata is malformed",
					MakeProjectRelative(metadataPath)});
				return InvalidAssetHandle;
			}
		}

		std::random_device random;
		const auto ticks = static_cast<AssetHandle>(
			std::chrono::high_resolution_clock::now().time_since_epoch().count());
		AssetHandle handle = ticks ^ (static_cast<AssetHandle>(random()) << 32) ^ random();
		if (handle == InvalidAssetHandle)
			handle = 1;
		while (m_paths.contains(handle))
			handle = handle * 1099511628211ull + 1;
		try
		{
			nlohmann::json metadata = {
				{"version", 1},
				{"id", handle},
				{"type", static_cast<int>(type)}
			};
			std::ofstream output(metadataPath, std::ios::binary | std::ios::trunc);
			if (!output || !(output << metadata.dump(2)))
				throw std::runtime_error("metadata write failed");
		}
		catch (...)
		{
			m_diagnostics.push_back({
				AssetDiagnostic::Severity::Warning,
				"Could not create asset metadata; using a path-based compatibility handle",
				MakeProjectRelative(absolutePath)});
			return InvalidAssetHandle;
		}
		return handle;
	}

	AssetReference AssetRegistry::Register(const std::filesystem::path& path, AssetType type)
	{
		const std::string normalized = MakeProjectRelative(path);
		if (normalized.empty())
			return {};
		AssetHandle handle = ReadOrCreateMetadata(ResolvePath(path), type);
		if (handle == InvalidAssetHandle)
			handle = HashPath(normalized);
		if (!RegisterWithHandle(handle, normalized, type))
			return {};
		return {handle, type, normalized};
	}

	AssetReference AssetRegistry::GetReference(AssetHandle handle) const
	{
		const auto path = m_paths.find(handle);
		if (path == m_paths.end())
			return {};
		const auto type = m_types.find(handle);
		return {
			handle,
			type == m_types.end() ? AssetType::Unknown : type->second,
			path->second
		};
	}

	std::string AssetRegistry::GetPath(AssetHandle handle) const
	{
		const auto found = m_paths.find(handle);
		return found == m_paths.end() ? std::string{} : found->second;
	}

	std::shared_ptr<Model> AssetRegistry::LoadModel(const std::filesystem::path& path)
	{
		const AssetHandle handle = Register(path, AssetType::Model).Handle;
		if (handle == InvalidAssetHandle)
			return nullptr;
		if (const auto found = m_models.find(handle); found != m_models.end())
		{
			if (auto model = found->second.lock())
				return model;
		}
		const std::filesystem::path modelPath = ResolvePath(GetPath(handle));
		auto prepared = ModelLoader::Prepare(modelPath.string(), m_jobs);
		auto model = prepared ? ModelLoader::Finalize(
			std::move(*prepared),
			[this](const std::string& texturePath)
			{
				return LoadTexture(texturePath);
			}) : nullptr;
		if (model)
		{
			for (std::size_t meshIndex = 0; meshIndex < model->GetMeshes().size(); ++meshIndex)
			{
				const auto material = model->GetMaterial(meshIndex);
				if (!material)
					continue;
				const auto registerTexture = [this](
					AssetHandle& textureHandle,
					std::string& texturePath)
				{
					if (texturePath.empty())
						return;
					const AssetReference reference = Register(texturePath, AssetType::Texture);
					textureHandle = reference.Handle;
					texturePath = reference.Path;
				};
				registerTexture(material->AlbedoTextureHandle, material->AlbedoTexturePath);
				registerTexture(material->NormalTextureHandle, material->NormalTexturePath);
				registerTexture(
					material->MetallicRoughnessTextureHandle,
					material->MetallicRoughnessTexturePath);
			}
			m_models.insert_or_assign(handle, model);
		}
		return model;
	}

	std::shared_ptr<Model> AssetRegistry::LoadModel(AssetHandle handle)
	{
		const AssetReference reference = GetReference(handle);
		return reference.IsValid() ? LoadModel(reference.Path) : nullptr;
	}

	std::shared_ptr<Texture2D> AssetRegistry::LoadTexture(const std::filesystem::path& path)
	{
		const AssetHandle handle = Register(path, AssetType::Texture).Handle;
		if (handle == InvalidAssetHandle)
			return nullptr;
		if (const auto found = m_textures.find(handle); found != m_textures.end())
		{
			if (auto texture = found->second.lock())
				return texture;
		}
		const std::string resolvedPath = ResolvePath(GetPath(handle)).string();
		std::optional<PreparedTexture2D> prepared;
		if (m_jobs)
			prepared = m_jobs->Submit([resolvedPath]
			{
				return Texture2D::PrepareFromFile(resolvedPath);
			}).get();
		else
			prepared = Texture2D::PrepareFromFile(resolvedPath);
		auto texture = prepared ? Texture2D::Finalize(std::move(*prepared)) : nullptr;
		if (texture)
			m_textures.insert_or_assign(handle, texture);
		return texture;
	}

	std::shared_ptr<Texture2D> AssetRegistry::LoadTexture(AssetHandle handle)
	{
		const AssetReference reference = GetReference(handle);
		return reference.IsValid() ? LoadTexture(reference.Path) : nullptr;
	}

	void AssetRegistry::ClearRuntimeCache()
	{
		m_models.clear();
		m_textures.clear();
	}

	bool AssetRegistry::SaveManifest(const std::filesystem::path& path) const
	{
		nlohmann::json root;
		root["version"] = 2;
		root["assets"] = nlohmann::json::array();
		for (const auto& [handle, assetPath] : m_paths)
		{
			const auto type = m_types.find(handle);
			root["assets"].push_back({
				{"handle", handle},
				{"type", static_cast<int>(
					type == m_types.end() ? AssetType::Unknown : type->second)},
				{"path", assetPath}
			});
		}
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path());
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		return output && static_cast<bool>(output << root.dump(2));
	}

	bool AssetRegistry::LoadManifest(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
			return false;
		try
		{
			const nlohmann::json root = nlohmann::json::parse(input);
			const int version = root.at("version").get<int>();
			if ((version != 1 && version != 2) || !root.at("assets").is_array())
				return false;
			for (const auto& asset : root["assets"])
			{
				const AssetHandle handle = asset.at("handle").get<AssetHandle>();
				const AssetType type = static_cast<AssetType>(asset.value("type", 0));
				const std::string assetPath = asset.at("path").get<std::string>();
				if (!RegisterWithHandle(handle, assetPath, type))
					return false;
				if (!std::filesystem::exists(ResolvePath(assetPath)))
					m_diagnostics.push_back({
						AssetDiagnostic::Severity::Warning,
						"Manifest references a missing asset",
						assetPath});
			}
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool AssetRegistry::Unregister(AssetHandle handle)
	{
		if (handle == InvalidAssetHandle || m_paths.erase(handle) == 0) return false;
		m_types.erase(handle);
		m_models.erase(handle);
		m_textures.erase(handle);
		return true;
	}

	bool AssetRegistry::RebuildFromMetadata(const std::filesystem::path& assetDirectory)
	{
		const std::filesystem::path root = ResolvePath(assetDirectory);
		std::error_code error;
		if (!std::filesystem::is_directory(root, error))
			return false;
		bool valid = true;
		for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
			!error && iterator != end; iterator.increment(error))
		{
			if (!iterator->is_regular_file() || iterator->path().extension() == ".meta")
				continue;
			const std::filesystem::path metadataPath = iterator->path().string() + ".meta";
			if (!std::filesystem::exists(metadataPath))
				continue;
			const std::string relative = MakeProjectRelative(iterator->path());
			const AssetHandle handle = ReadOrCreateMetadata(iterator->path(), AssetType::Unknown);
			valid = RegisterWithHandle(handle, relative, AssetType::Unknown) && valid;
		}
		return valid && !error;
	}

	bool AssetRegistry::DiscoverAssets(const std::filesystem::path& assetDirectory)
	{
		const std::filesystem::path root = ResolvePath(assetDirectory);
		std::error_code error;
		if (!std::filesystem::is_directory(root, error))
			return false;
		for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
			!error && iterator != end; iterator.increment(error))
		{
			if (!iterator->is_regular_file() || iterator->path().extension() == ".meta")
				continue;
			std::string extension = iterator->path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
			AssetType type = AssetType::Unknown;
			if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
				|| extension == ".tga" || extension == ".dds" || extension == ".ktx")
				type = AssetType::Texture;
			else if (extension == ".fbx" || extension == ".obj" || extension == ".gltf"
				|| extension == ".glb" || extension == ".dae" || extension == ".3ds")
				type = AssetType::Model;
			else if (extension == ".ematerial") type = AssetType::Material;
			else if (extension == ".escene") type = AssetType::Scene;
			else if (extension == ".eprefab") type = AssetType::Prefab;
			if (type != AssetType::Unknown)
				Register(iterator->path(), type);
		}
		return !error;
	}

	std::vector<AssetReference> AssetRegistry::GetReferences() const
	{
		std::vector<AssetReference> references;
		references.reserve(m_paths.size());
		for (const auto& [handle, path] : m_paths)
			references.push_back(GetReference(handle));
		std::sort(references.begin(), references.end(),
			[](const AssetReference& left, const AssetReference& right)
			{ return left.Path < right.Path; });
		return references;
	}
}
