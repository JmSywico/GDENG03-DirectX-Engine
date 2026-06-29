#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

#include <vector>

namespace dx3d
{
	struct MeshVertex
	{
		Vec3 position{};
		Vec4 color{};
		Vec3 normal{};
	};

	struct MeshData
	{
		std::vector<MeshVertex> vertices{};
		std::vector<ui32> indices{};

		bool empty() const noexcept
		{
			return vertices.empty() ||
				indices.empty();
		}

		void clear()
		{
			vertices.clear();
			indices.clear();
		}
	};
}
