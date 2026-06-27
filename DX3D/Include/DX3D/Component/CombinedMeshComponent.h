#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Graphics/MeshData.h>

namespace dx3d
{
	class CombinedMeshComponent final : public Component
	{
		dx3d_typeid(CombinedMeshComponent)

	public:
		explicit CombinedMeshComponent(
			const ComponentDesc& data
		);

		void setMeshData(
			const MeshData& meshData
		);

		const MeshData& getMeshData() const noexcept;

		bool hasMeshData() const noexcept;

		ui32 getMeshRevision() const noexcept;

	private:
		MeshData m_meshData{};
		ui32 m_meshRevision{};
	};
}