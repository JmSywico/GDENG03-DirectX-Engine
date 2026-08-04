#pragma once

#include <DX3D/Graphics/MeshData.h>

#include <string>

namespace dx3d
{
	bool loadObjMesh(
		const std::string& filePath,
		MeshData& outputMesh
	);
}