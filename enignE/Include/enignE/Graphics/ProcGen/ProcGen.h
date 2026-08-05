#pragma once

#include <vector>
#include <memory>

#include "../Mesh.h"
#include "../Model.h"

struct SimpleVertex;
class Model;
class Mesh;

struct MeshData
{
	std::vector<SimpleVertex> vertices;
	std::vector<uint32_t> indices;
};

class ProcGen
{
public:
	static MeshData CreateCube(float size);
	static MeshData CreatePyramid(float size);
	static MeshData CreateRectangle(float width, float height, float depth);
	static MeshData CreatePlane(float width, float depth);
	static MeshData CreateSphere(float radius, int slices, int stacks);

	static std::shared_ptr<Model> BuildModel(const MeshData& data);
};
