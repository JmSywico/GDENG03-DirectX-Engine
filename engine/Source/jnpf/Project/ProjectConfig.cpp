#include "Project/ProjectConfig.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace jnpf::Project
{
	std::filesystem::path ProjectConfig::GetRoot() const
	{
		return ProjectFile.empty() ? std::filesystem::current_path() : ProjectFile.parent_path();
	}

	std::filesystem::path ProjectConfig::Resolve(const std::filesystem::path& path) const
	{
		return path.is_absolute() ? path.lexically_normal() : (GetRoot() / path).lexically_normal();
	}

	std::filesystem::path ProjectConfig::ResolveStartupScene() const
	{
		return Resolve(StartupScene);
	}

	bool ProjectConfig::Save(const std::filesystem::path& path) const
	{
		const nlohmann::json document = {
			{"version", CurrentVersion},
			{"name", Name},
			{"assets", AssetDirectory.generic_string()},
			{"startupScene", StartupScene.generic_string()},
			{"inputActions", InputActions.generic_string()},
			{"cache", CacheDirectory.generic_string()},
			{"output", OutputDirectory.generic_string()}
		};
		std::error_code error;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		return output && static_cast<bool>(output << document.dump(2));
	}

	bool ProjectConfig::Load(
		const std::filesystem::path& path,
		ProjectConfig& result,
		std::string* error)
	{
		try
		{
			std::ifstream input(path, std::ios::binary);
			if (!input)
				throw std::runtime_error("project file could not be opened");
			const nlohmann::json document = nlohmann::json::parse(input);
			if (document.at("version").get<int>() != CurrentVersion)
				throw std::runtime_error("unsupported project version");
			ProjectConfig loaded;
			loaded.ProjectFile = std::filesystem::absolute(path).lexically_normal();
			loaded.Name = document.value("name", loaded.Name);
			loaded.AssetDirectory = document.value("assets", loaded.AssetDirectory.generic_string());
			loaded.StartupScene = document.value("startupScene", loaded.StartupScene.generic_string());
			loaded.InputActions = document.value("inputActions", loaded.InputActions.generic_string());
			loaded.CacheDirectory = document.value("cache", loaded.CacheDirectory.generic_string());
			loaded.OutputDirectory = document.value("output", loaded.OutputDirectory.generic_string());
			result = std::move(loaded);
			return true;
		}
		catch (const std::exception& exception)
		{
			if (error)
				*error = exception.what();
			return false;
		}
	}

	ProjectConfig ProjectConfig::Discover(const std::filesystem::path& start)
	{
		std::error_code error;
		std::filesystem::path directory = std::filesystem::is_directory(start, error)
			? start : start.parent_path();
		directory = std::filesystem::absolute(directory, error);
		while (!directory.empty())
		{
			for (std::filesystem::directory_iterator iterator(directory, error), end;
				!error && iterator != end; iterator.increment(error))
			{
				if (iterator->is_regular_file() && iterator->path().extension() == ".jnpfproject")
				{
					ProjectConfig project;
					if (Load(iterator->path(), project))
						return project;
				}
			}
			const std::filesystem::path parent = directory.parent_path();
			if (parent == directory)
				break;
			directory = parent;
			error.clear();
		}
		ProjectConfig fallback;
		fallback.ProjectFile = std::filesystem::absolute(start, error) / "jnpf.jnpfproject";
		return fallback;
	}
}
