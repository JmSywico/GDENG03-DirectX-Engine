#pragma once

#include "Graphics/Material.h"
#include "Graphics/ProcGen/ProcGen.h"

#include <memory>
#include <optional>
#include <string>
#include <functional>
#include <vector>

class Model;
class Texture2D;
namespace enignE::Core { class JobSystem; }

struct PreparedModelMesh
{
	MeshData Data;
	std::shared_ptr<MaterialResource> Material;
};

struct PreparedModel
{
	std::string SourcePath;
	std::vector<PreparedModelMesh> Meshes;
};

class ModelLoader
{
public:
	using TextureLoader = std::function<std::shared_ptr<Texture2D>(const std::string&)>;
	static std::optional<PreparedModel> Prepare(
		const std::string& filepath,
		enignE::Core::JobSystem* jobs = nullptr);
	static std::shared_ptr<Model> Finalize(
		PreparedModel prepared,
		const TextureLoader& textureLoader = {});
	static std::shared_ptr<Model> Load(
		const std::string& filepath,
		const TextureLoader& textureLoader = {});
};
