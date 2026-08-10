#include "Graphics/ModelLoader.h"

#include "Graphics/Mesh.h"
#include "Graphics/Model.h"
#include "Graphics/Material.h"
#include "Graphics/Texture2D.h"
#include "Core/JobSystem.h"
#include "Logging/Logging.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstdint>
#include <algorithm>
#include <limits>
#include <filesystem>
#include <vector>
#include <unordered_set>

namespace
{
	constexpr unsigned int ImportFlags =
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_ImproveCacheLocality |
		aiProcess_SortByPType |
		aiProcess_PreTransformVertices |
		aiProcess_ValidateDataStructure |
		aiProcess_ConvertToLeftHanded;

	bool FitsGpuBuffer(size_t elementCount, size_t elementSize)
	{
		return elementCount <= std::numeric_limits<uint32_t>::max() / elementSize;
	}

	bool BuildMeshData(const aiMesh& source, MeshData& result)
	{
		if (!source.HasPositions() || source.mNumVertices == 0)
			return false;

		result.vertices.resize(source.mNumVertices);
		for (unsigned int i = 0; i < source.mNumVertices; ++i)
		{
			const aiVector3D& position = source.mVertices[i];
			const aiVector3D normal = source.HasNormals()
				? source.mNormals[i]
				: aiVector3D(0.0f, 1.0f, 0.0f);
			const aiVector3D uv = source.HasTextureCoords(0)
				? source.mTextureCoords[0][i]
				: aiVector3D();
			const aiVector3D tangent = source.HasTangentsAndBitangents()
				? source.mTangents[i]
				: aiVector3D(1.0f, 0.0f, 0.0f);
			float handedness = 1.0f;
			if (source.HasTangentsAndBitangents())
			{
				const aiVector3D bitangent = source.mBitangents[i];
				handedness = (source.mNormals[i] ^ tangent) * bitangent < 0.0f ? -1.0f : 1.0f;
			}

			result.vertices[i] = {
				{position.x, position.y, position.z},
				{normal.x, normal.y, normal.z},
				{uv.x, uv.y},
				{tangent.x, tangent.y, tangent.z, handedness}
			};
		}

		result.indices.reserve(static_cast<size_t>(source.mNumFaces) * 3);
		for (unsigned int faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex)
		{
			const aiFace& face = source.mFaces[faceIndex];
			if (face.mNumIndices != 3)
				continue;

			result.indices.push_back(face.mIndices[0]);
			result.indices.push_back(face.mIndices[1]);
			result.indices.push_back(face.mIndices[2]);
		}

		return !result.indices.empty()
			&& FitsGpuBuffer(result.vertices.size(), sizeof(SimpleVertex))
			&& FitsGpuBuffer(result.indices.size(), sizeof(uint32_t));
	}

	std::string ReadTexturePath(
		const aiMaterial& source,
		aiTextureType type,
		const std::filesystem::path& modelDirectory)
	{
		aiString texturePath;
		if (source.GetTextureCount(type) == 0
			|| source.GetTexture(type, 0, &texturePath) != AI_SUCCESS
			|| texturePath.length == 0
			|| texturePath.C_Str()[0] == '*')
			return {};
		return (modelDirectory / std::filesystem::path(texturePath.C_Str())).lexically_normal().string();
	}

	std::shared_ptr<MaterialResource> BuildMaterial(
		const aiMaterial& source,
		const std::filesystem::path& modelDirectory)
	{
		auto material = std::make_shared<MaterialResource>();
		aiColor4D baseColor;
		if (aiGetMaterialColor(&source, AI_MATKEY_BASE_COLOR, &baseColor) == AI_SUCCESS
			|| aiGetMaterialColor(&source, AI_MATKEY_COLOR_DIFFUSE, &baseColor) == AI_SUCCESS)
			material->Albedo = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};

		aiColor3D emissive;
		if (source.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
			material->Emissive = {emissive.r, emissive.g, emissive.b};
		source.Get(AI_MATKEY_METALLIC_FACTOR, material->Metallic);
		source.Get(AI_MATKEY_ROUGHNESS_FACTOR, material->Roughness);

		material->AlbedoTexturePath = ReadTexturePath(
			source,
			source.GetTextureCount(aiTextureType_BASE_COLOR) > 0
				? aiTextureType_BASE_COLOR
				: aiTextureType_DIFFUSE,
			modelDirectory);
		material->NormalTexturePath = ReadTexturePath(
			source,
			source.GetTextureCount(aiTextureType_NORMAL_CAMERA) > 0
				? aiTextureType_NORMAL_CAMERA
				: aiTextureType_NORMALS,
			modelDirectory);
		material->MetallicRoughnessTexturePath =
			ReadTexturePath(source, aiTextureType_METALNESS, modelDirectory);
		if (material->MetallicRoughnessTexturePath.empty())
			material->MetallicRoughnessTexturePath =
				ReadTexturePath(source, aiTextureType_DIFFUSE_ROUGHNESS, modelDirectory);

		return material;
	}
}

std::optional<PreparedModel> ModelLoader::Prepare(
	const std::string& filepath,
	jnpf::Core::JobSystem* jobs)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filepath, ImportFlags);
	if (!scene || !scene->HasMeshes())
	{
		LOG_ERRORF("Assimp failed to load model '{}': {}", filepath, importer.GetErrorString());
		return std::nullopt;
	}

	PreparedModel prepared;
	prepared.SourcePath = filepath;
	std::vector<std::shared_ptr<MaterialResource>> materials(scene->mNumMaterials);
	const std::filesystem::path modelDirectory = std::filesystem::path(filepath).parent_path();
	for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		if (scene->mMaterials[materialIndex])
			materials[materialIndex] =
				BuildMaterial(*scene->mMaterials[materialIndex], modelDirectory);
	}
	prepared.Meshes.resize(scene->mNumMeshes);
	const auto prepareMesh = [&prepared, &materials, scene, &filepath](std::size_t meshIndex)
	{
		const aiMesh* source = scene->mMeshes[meshIndex];
		if (!source) return;
		PreparedModelMesh& destination = prepared.Meshes[meshIndex];
		if (BuildMeshData(*source, destination.Data))
		{
			destination.Material =
				source->mMaterialIndex < materials.size()
					? materials[source->mMaterialIndex]
					: nullptr;
		}
		else
			LOG_WARNF("Assimp skipped unsupported or invalid mesh {} in '{}'", meshIndex, filepath);
	};
	if (jobs && scene->mNumMeshes >= 4)
		jobs->ParallelFor(scene->mNumMeshes, 1, prepareMesh);
	else
		for (std::size_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) prepareMesh(meshIndex);
	prepared.Meshes.erase(
		std::remove_if(prepared.Meshes.begin(), prepared.Meshes.end(),
			[](const PreparedModelMesh& mesh) { return mesh.Data.vertices.empty() || mesh.Data.indices.empty(); }),
		prepared.Meshes.end());

	if (prepared.Meshes.empty())
	{
		LOG_ERRORF("Assimp loaded no renderable triangle meshes from '{}'", filepath);
		return std::nullopt;
	}
	return prepared;
}

std::shared_ptr<Model> ModelLoader::Finalize(
	PreparedModel prepared,
	const TextureLoader& textureLoader)
{
	auto model = std::make_shared<Model>();
	std::unordered_set<MaterialResource*> resolvedMaterials;
	for (PreparedModelMesh& preparedMesh : prepared.Meshes)
	{
		if (preparedMesh.Material && textureLoader
			&& resolvedMaterials.insert(preparedMesh.Material.get()).second)
		{
			auto& material = *preparedMesh.Material;
			if (!material.AlbedoTexturePath.empty())
				material.AlbedoTexture = textureLoader(material.AlbedoTexturePath);
			if (!material.NormalTexturePath.empty())
				material.NormalTexture = textureLoader(material.NormalTexturePath);
			if (!material.MetallicRoughnessTexturePath.empty())
				material.MetallicRoughnessTexture = textureLoader(material.MetallicRoughnessTexturePath);
		}
		auto mesh = std::make_shared<Mesh>();
		if (mesh->InitializeIndexed(
			preparedMesh.Data.vertices.data(),
			static_cast<uint32_t>(preparedMesh.Data.vertices.size() * sizeof(SimpleVertex)),
			sizeof(SimpleVertex),
			preparedMesh.Data.indices.data(),
			static_cast<uint32_t>(preparedMesh.Data.indices.size() * sizeof(uint32_t))))
			model->AddMesh(std::move(mesh), preparedMesh.Material);
	}
	if (model->GetMeshes().empty()) return nullptr;

	LOG_INFOF("Loaded model '{}' with {} mesh(es)", prepared.SourcePath, model->GetMeshes().size());
	return model;
}

std::shared_ptr<Model> ModelLoader::Load(
	const std::string& filepath,
	const TextureLoader& textureLoader)
{
	auto prepared = Prepare(filepath);
	return prepared ? Finalize(std::move(*prepared), textureLoader) : nullptr;
}
