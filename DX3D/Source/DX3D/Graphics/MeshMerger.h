#pragma once
#pragma once

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Mat4x4.h>

#include <vector>

namespace dx3d
{
	struct MeshMergeSource
	{
		const MeshData* meshData{};
		Mat4x4 transform{};
	};

	MeshData mergeMeshes(
		const std::vector<MeshMergeSource>& sources
	);
}