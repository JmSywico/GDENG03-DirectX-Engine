#include <DX3D/Component/ModelComponent.h>

dx3d::ModelComponent::ModelComponent(
	const ComponentDesc& data
)
	: Component(data)
{}

void dx3d::ModelComponent::setMeshData(
	const MeshData& meshData
)
{
	m_meshData =
		meshData;
}

const dx3d::MeshData&
dx3d::ModelComponent::getMeshData() const noexcept
{
	return m_meshData;
}

bool dx3d::ModelComponent::hasMeshData() const noexcept
{
	return !m_meshData.empty();
}

void dx3d::ModelComponent::setModelPath(
	const std::string& modelPath
)
{
	m_modelPath =
		modelPath;
}

const std::string&
dx3d::ModelComponent::getModelPath() const noexcept
{
	return m_modelPath;
}

void dx3d::ModelComponent::setTexturePath(
	const std::string& texturePath
)
{
	m_texturePath =
		texturePath;
}

const std::string&
dx3d::ModelComponent::getTexturePath() const noexcept
{
	return m_texturePath;
}

bool dx3d::ModelComponent::hasTexture() const noexcept
{
	return !m_texturePath.empty();
}

void dx3d::ModelComponent::clearTexture()
{
	m_texturePath.clear();
}