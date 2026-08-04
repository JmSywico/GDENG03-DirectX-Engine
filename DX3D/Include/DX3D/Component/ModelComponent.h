#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Graphics/MeshData.h>

#include <string>

namespace dx3d
{
	class ModelComponent final : public Component
	{
		dx3d_typeid(ModelComponent)

	public:
		explicit ModelComponent(
			const ComponentDesc& data
		);

		void setMeshData(
			const MeshData& meshData
		);

		const MeshData& getMeshData() const noexcept;

		bool hasMeshData() const noexcept;

		void setModelPath(
			const std::string& modelPath
		);

		const std::string&
			getModelPath() const noexcept;

		void setTexturePath(
			const std::string& texturePath
		);

		const std::string&
			getTexturePath() const noexcept;

		bool hasTexture() const noexcept;

		void clearTexture();

	private:
		MeshData m_meshData{};

		std::string m_modelPath{};
		std::string m_texturePath{};
	};
}