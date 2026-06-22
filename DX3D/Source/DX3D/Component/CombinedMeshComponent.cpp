#include <DX3D/Component/CombinedMeshComponent.h>

dx3d::CombinedMeshComponent::CombinedMeshComponent(
	const ComponentDesc& data
)
	: Component(data)
{}

void dx3d::CombinedMeshComponent::setMeshData(
	const MeshData& meshData
)
{
	m_meshData = meshData;
	++m_meshRevision;
}

const dx3d::MeshData&
dx3d::CombinedMeshComponent::getMeshData() const noexcept
{
	return m_meshData;
}

bool dx3d::CombinedMeshComponent::hasMeshData() const noexcept
{
	return !m_meshData.empty();
}

dx3d::ui32
dx3d::CombinedMeshComponent::getMeshRevision() const noexcept
{
	return m_meshRevision;
}