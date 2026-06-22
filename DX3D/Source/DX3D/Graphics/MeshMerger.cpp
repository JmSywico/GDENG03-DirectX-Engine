#include <DX3D/Graphics/MeshMerger.h>
#include <DX3D/Math/Vec4.h>

dx3d::MeshData dx3d::mergeMeshes(
	const std::vector<MeshMergeSource>& sources
)
{
	MeshData combinedMesh{};

	size_t totalVertexCount = 0;
	size_t totalIndexCount = 0;

	// Determine the final sizes first so the vectors
	// do not repeatedly reallocate while merging.
	for (const auto& source : sources)
	{
		if (!source.meshData ||
			source.meshData->empty())
		{
			continue;
		}

		totalVertexCount +=
			source.meshData->vertices.size();

		totalIndexCount +=
			source.meshData->indices.size();
	}

	combinedMesh.vertices.reserve(
		totalVertexCount
	);

	combinedMesh.indices.reserve(
		totalIndexCount
	);

	for (const auto& source : sources)
	{
		if (!source.meshData ||
			source.meshData->empty())
		{
			continue;
		}

		const MeshData& sourceMesh =
			*source.meshData;

		// The indices of this mesh must be moved forward
		// by the number of vertices already added.
		const ui32 vertexOffset =
			static_cast<ui32>(
				combinedMesh.vertices.size()
				);

		for (const auto& sourceVertex :
			sourceMesh.vertices)
		{
			const Vec4 transformedPosition =
				source.transform.transform(
					{
						sourceVertex.position.x,
						sourceVertex.position.y,
						sourceVertex.position.z,
						1.0f
					}
				);

			MeshVertex combinedVertex =
				sourceVertex;

			combinedVertex.position =
			{
				transformedPosition.x,
				transformedPosition.y,
				transformedPosition.z
			};

			combinedMesh.vertices.push_back(
				combinedVertex
			);
		}

		for (const ui32 sourceIndex :
		sourceMesh.indices)
		{
			combinedMesh.indices.push_back(
				sourceIndex + vertexOffset
			);
		}
	}

	return combinedMesh;
}