#pragma once

#include <vector>
#include <memory>
#include "Mesh.h"
#include "Material.h"

class Model
{
public:
	Model() = default;
	~Model() = default;

	void AddMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<MaterialResource> material = {})
	{
		m_meshes.push_back(std::move(mesh));
		m_materials.push_back(std::move(material));
	}
	const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return m_meshes; }
	std::shared_ptr<MaterialResource> GetMaterial(std::size_t meshIndex) const
	{
		return meshIndex < m_materials.size() ? m_materials[meshIndex] : nullptr;
	}

private:
	std::vector<std::shared_ptr<Mesh>> m_meshes;
	std::vector<std::shared_ptr<MaterialResource>> m_materials;
};
