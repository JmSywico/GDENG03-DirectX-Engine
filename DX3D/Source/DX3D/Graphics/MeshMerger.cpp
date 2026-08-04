#include <DX3D/Graphics/MeshMerger.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

dx3d::MeshData dx3d::mergeMeshes(
	const std::vector<MeshMergeSource>& sources
)
{
	MeshData combinedMesh{};

	size_t totalVertexCount = 0;
	size_t totalIndexCount = 0;

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

		const Mat4x4 inverseTransform =
			Mat4x4::inverse(source.transform);
            
		const Vec4 inverseRow0 =
			inverseTransform.row(0);

		const Vec4 inverseRow1 =
			inverseTransform.row(1);

		const Vec4 inverseRow2 =
			inverseTransform.row(2);

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

			const Vec3 transformedNormal =
				Vec3::normalize(
					{
						sourceVertex.normal.x *
							inverseRow0.x +
						sourceVertex.normal.y *
							inverseRow0.y +
						sourceVertex.normal.z *
							inverseRow0.z,

						sourceVertex.normal.x *
							inverseRow1.x +
						sourceVertex.normal.y *
							inverseRow1.y +
						sourceVertex.normal.z *
							inverseRow1.z,

						sourceVertex.normal.x *
							inverseRow2.x +
						sourceVertex.normal.y *
							inverseRow2.y +
						sourceVertex.normal.z *
							inverseRow2.z
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

			combinedVertex.normal =
				transformedNormal;

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