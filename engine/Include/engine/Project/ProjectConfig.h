#pragma once

#include <filesystem>
#include <string>

namespace enignE::Project
{
	struct ProjectConfig
	{
		static constexpr int CurrentVersion = 1;

		std::string Name = "enignE Project";
		std::filesystem::path ProjectFile;
		std::filesystem::path AssetDirectory = "assets";
		std::filesystem::path StartupScene = "scenes/test.escene";
		std::filesystem::path InputActions = "config/input-actions.json";
		std::filesystem::path CacheDirectory = ".enigne/cache";
		std::filesystem::path OutputDirectory = "build";

		std::filesystem::path GetRoot() const;
		std::filesystem::path Resolve(const std::filesystem::path& path) const;
		std::filesystem::path ResolveStartupScene() const;
		bool Save(const std::filesystem::path& path) const;
		static bool Load(const std::filesystem::path& path, ProjectConfig& result, std::string* error = nullptr);
		static ProjectConfig Discover(const std::filesystem::path& start);
	};
}
