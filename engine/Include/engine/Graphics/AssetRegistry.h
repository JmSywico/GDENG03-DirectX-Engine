#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Model;
class Texture2D;
namespace enignE::Core { class JobSystem; }

namespace enignE::Graphics
{
	using AssetHandle = std::uint64_t;
	inline constexpr AssetHandle InvalidAssetHandle = 0;

	enum class AssetType : std::uint8_t
	{
		Unknown,
		Model,
		Texture,
		Material,
		Scene,
		Prefab
	};

	struct AssetDiagnostic
	{
		enum class Severity : std::uint8_t { Warning, Error };
		Severity Level = Severity::Warning;
		std::string Message;
		std::string Path;
	};

	struct AssetReference
	{
		AssetHandle Handle = InvalidAssetHandle;
		AssetType Type = AssetType::Unknown;
		std::string Path;

		bool IsValid() const { return Handle != InvalidAssetHandle; }
	};

	class AssetRegistry
	{
	public:
		explicit AssetRegistry(std::filesystem::path projectRoot = {});
		void SetJobSystem(Core::JobSystem* jobs) { m_jobs = jobs; }

		void SetProjectRoot(std::filesystem::path projectRoot);
		const std::filesystem::path& GetProjectRoot() const { return m_projectRoot; }
		std::filesystem::path ResolvePath(const std::filesystem::path& path) const;
		std::string MakeProjectRelative(const std::filesystem::path& path) const;

		AssetReference Register(const std::filesystem::path& path, AssetType type);
		AssetReference RegisterReference(const AssetReference& reference);
		AssetHandle RegisterPath(const std::filesystem::path& path);
		std::string GetPath(AssetHandle handle) const;
		AssetReference GetReference(AssetHandle handle) const;
		bool Unregister(AssetHandle handle);

		std::shared_ptr<Model> LoadModel(const std::filesystem::path& path);
		std::shared_ptr<Model> LoadModel(AssetHandle handle);
		std::shared_ptr<Texture2D> LoadTexture(const std::filesystem::path& path);
		std::shared_ptr<Texture2D> LoadTexture(AssetHandle handle);
		void ClearRuntimeCache();
		bool SaveManifest(const std::filesystem::path& path) const;
		bool LoadManifest(const std::filesystem::path& path);
		bool RebuildFromMetadata(const std::filesystem::path& assetDirectory);
		bool DiscoverAssets(const std::filesystem::path& assetDirectory);
		std::vector<AssetReference> GetReferences() const;
		const std::vector<AssetDiagnostic>& GetDiagnostics() const { return m_diagnostics; }
		void ClearDiagnostics() { m_diagnostics.clear(); }

	private:
		static AssetHandle HashPath(const std::string& normalizedPath);
		AssetHandle ReadOrCreateMetadata(const std::filesystem::path& absolutePath, AssetType type);
		bool RegisterWithHandle(AssetHandle handle, const std::string& path, AssetType type);

		std::filesystem::path m_projectRoot;
		std::unordered_map<AssetHandle, std::string> m_paths;
		std::unordered_map<AssetHandle, AssetType> m_types;
		std::unordered_map<AssetHandle, std::weak_ptr<Model>> m_models;
		std::unordered_map<AssetHandle, std::weak_ptr<Texture2D>> m_textures;
		std::vector<AssetDiagnostic> m_diagnostics;
		Core::JobSystem* m_jobs = nullptr;
	};
}
