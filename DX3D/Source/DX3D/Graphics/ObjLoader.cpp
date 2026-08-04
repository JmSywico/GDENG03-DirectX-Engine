#include <DX3D/Graphics/ObjLoader.h>

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	struct ObjVertexReference
	{
		int positionIndex{};
		int texCoordIndex{};
		int normalIndex{};

		bool hasTexCoord{};
		bool hasNormal{};
	};

	bool parseFaceVertex(
		const std::string& token,
		ObjVertexReference& outputReference
	)
	{
		if (token.empty())
			return false;

		try
		{
			const size_t firstSlash =
				token.find('/');

			if (firstSlash == std::string::npos)
			{
				outputReference.positionIndex =
					std::stoi(token);

				return true;
			}

			const std::string positionText =
				token.substr(
					0,
					firstSlash
				);

			if (positionText.empty())
				return false;

			outputReference.positionIndex =
				std::stoi(positionText);

			const size_t secondSlash =
				token.find(
					'/',
					firstSlash + 1
				);

			if (secondSlash == std::string::npos)
			{
				const std::string texCoordText =
					token.substr(
						firstSlash + 1
					);

				if (!texCoordText.empty())
				{
					outputReference.texCoordIndex =
						std::stoi(texCoordText);

					outputReference.hasTexCoord =
						true;
				}

				return true;
			}

			const std::string texCoordText =
				token.substr(
					firstSlash + 1,
					secondSlash - firstSlash - 1
				);

			if (!texCoordText.empty())
			{
				outputReference.texCoordIndex =
					std::stoi(texCoordText);

				outputReference.hasTexCoord =
					true;
			}

			const std::string normalText =
				token.substr(
					secondSlash + 1
				);

			if (!normalText.empty())
			{
				outputReference.normalIndex =
					std::stoi(normalText);

				outputReference.hasNormal =
					true;
			}
		}
		catch (...)
		{
			return false;
		}

		return true;
	}

	bool resolveObjIndex(
		int objIndex,
		size_t elementCount,
		size_t& outputIndex
	)
	{
		if (objIndex == 0 ||
			elementCount == 0)
		{
			return false;
		}

		if (objIndex > 0)
		{
			const size_t resolvedIndex =
				static_cast<size_t>(
					objIndex - 1
					);

			if (resolvedIndex >= elementCount)
				return false;

			outputIndex =
				resolvedIndex;

			return true;
		}

		const long long resolvedIndex =
			static_cast<long long>(
				elementCount
				) +
			static_cast<long long>(
				objIndex
				);

		if (resolvedIndex < 0 ||
			resolvedIndex >=
			static_cast<long long>(
				elementCount
				))
		{
			return false;
		}

		outputIndex =
			static_cast<size_t>(
				resolvedIndex
				);

		return true;
	}

	dx3d::Vec3 normalizeVector(
		const dx3d::Vec3& vector,
		const dx3d::Vec3& fallback
	)
	{
		const dx3d::f32 lengthSquared =
			vector.x * vector.x +
			vector.y * vector.y +
			vector.z * vector.z;

		if (lengthSquared <= 0.000001f)
			return fallback;

		const dx3d::f32 inverseLength =
			1.0f /
			std::sqrt(lengthSquared);

		return
		{
			vector.x * inverseLength,
			vector.y * inverseLength,
			vector.z * inverseLength
		};
	}

	dx3d::Vec3 calculateFaceNormal(
		const dx3d::Vec3& first,
		const dx3d::Vec3& second,
		const dx3d::Vec3& third
	)
	{
		const dx3d::Vec3 firstEdge
		{
			second.x - first.x,
			second.y - first.y,
			second.z - first.z
		};

		const dx3d::Vec3 secondEdge
		{
			third.x - first.x,
			third.y - first.y,
			third.z - first.z
		};

		const dx3d::Vec3 normal
		{
			firstEdge.y * secondEdge.z -
				firstEdge.z * secondEdge.y,

			firstEdge.z * secondEdge.x -
				firstEdge.x * secondEdge.z,

			firstEdge.x * secondEdge.y -
				firstEdge.y * secondEdge.x
		};

		return normalizeVector(
			normal,
			{ 0.0f, 1.0f, 0.0f }
		);
	}
}

bool dx3d::loadObjMesh(
	const std::string& filePath,
	MeshData& outputMesh
)
{
	outputMesh.clear();

	std::ifstream file(
		filePath
	);

	if (!file)
		return false;

	std::vector<Vec3> positions{};
	std::vector<Vec2> texCoords{};
	std::vector<Vec3> normals{};

	std::string line{};

	while (std::getline(file, line))
	{
		std::istringstream lineStream(
			line
		);

		std::string lineType{};

		lineStream >>
			lineType;

		if (lineType.empty() ||
			lineType[0] == '#')
		{
			continue;
		}

		if (lineType == "v")
		{
			Vec3 position{};

			if (lineStream >>
				position.x >>
				position.y >>
				position.z)
			{
				position.z =
					-position.z;

				positions.push_back(
					position
				);
			}

			continue;
		}

		if (lineType == "vt")
		{
			Vec2 texCoord{};

			if (lineStream >>
				texCoord.x >>
				texCoord.y)
			{
				texCoords.push_back(
					texCoord
				);
			}

			continue;
		}

		if (lineType == "vn")
		{
			Vec3 normal{};

			if (lineStream >>
				normal.x >>
				normal.y >>
				normal.z)
			{
				normal.z =
					-normal.z;

				normals.push_back(
					normalizeVector(
						normal,
						{ 0.0f, 1.0f, 0.0f }
					)
				);
			}

			continue;
		}

		if (lineType != "f")
			continue;

		std::vector<ObjVertexReference>
			faceReferences{};

		std::string faceToken{};

		while (lineStream >> faceToken)
		{
			ObjVertexReference reference{};

			if (!parseFaceVertex(
				faceToken,
				reference
			))
			{
				outputMesh.clear();
				return false;
			}

			faceReferences.push_back(
				reference
			);
		}

		if (faceReferences.size() < 3)
			continue;

		for (
			size_t triangleIndex = 1;
			triangleIndex + 1 <
			faceReferences.size();
			++triangleIndex
			)
		{
			const std::array<
				ObjVertexReference,
				3
			> triangleReferences
			{
				faceReferences[0],
				faceReferences[triangleIndex + 1],
				faceReferences[triangleIndex]
			};

			std::array<size_t, 3>
				positionIndices{};

			for (size_t corner = 0;
				corner < 3;
				++corner)
			{
				if (!resolveObjIndex(
					triangleReferences[corner].
					positionIndex,
					positions.size(),
					positionIndices[corner]
				))
				{
					outputMesh.clear();
					return false;
				}
			}

			const Vec3 faceNormal =
				calculateFaceNormal(
					positions[positionIndices[0]],
					positions[positionIndices[1]],
					positions[positionIndices[2]]
				);

			for (size_t corner = 0;
				corner < 3;
				++corner)
			{
				const ObjVertexReference& reference =
					triangleReferences[corner];

				MeshVertex vertex{};

				vertex.position =
					positions[
						positionIndices[corner]
					];

				vertex.color =
				{
					1.0f,
					1.0f,
					1.0f,
					1.0f
				};

				vertex.normal =
					faceNormal;

				if (reference.hasNormal)
				{
					size_t normalIndex{};

					if (!resolveObjIndex(
						reference.normalIndex,
						normals.size(),
						normalIndex
					))
					{
						outputMesh.clear();
						return false;
					}

					vertex.normal =
						normals[normalIndex];
				}

				if (reference.hasTexCoord)
				{
					size_t texCoordIndex{};

					if (!resolveObjIndex(
						reference.texCoordIndex,
						texCoords.size(),
						texCoordIndex
					))
					{
						outputMesh.clear();
						return false;
					}

					const Vec2 sourceTexCoord =
						texCoords[texCoordIndex];

					vertex.texCoord =
					{
						sourceTexCoord.x,
						1.0f - sourceTexCoord.y
					};
				}

				outputMesh.vertices.push_back(
					vertex
				);

				outputMesh.indices.push_back(
					static_cast<ui32>(
						outputMesh.vertices.size() - 1
						)
				);
			}
		}
	}

	if (outputMesh.empty())
	{
		outputMesh.clear();
		return false;
	}

	return true;
}