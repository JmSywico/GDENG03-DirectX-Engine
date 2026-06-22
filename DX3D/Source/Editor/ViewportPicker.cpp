#include <DX3D/Editor/ViewportPicker.h>

#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Mat4x4.h>
#include <DX3D/Math/Vec4.h>

#include <cmath>
#include <limits>

namespace
{
	constexpr dx3d::f32 intersectionEpsilon =
		0.000001f;

	dx3d::Vec3 subtract(
		const dx3d::Vec3& left,
		const dx3d::Vec3& right
	) noexcept
	{
		return
		{
			left.x - right.x,
			left.y - right.y,
			left.z - right.z
		};
	}

	dx3d::f32 dot(
		const dx3d::Vec3& left,
		const dx3d::Vec3& right
	) noexcept
	{
		return
			left.x * right.x +
			left.y * right.y +
			left.z * right.z;
	}

	dx3d::Vec3 cross(
		const dx3d::Vec3& left,
		const dx3d::Vec3& right
	) noexcept
	{
		return
		{
			left.y * right.z -
			left.z * right.y,

			left.z * right.x -
			left.x * right.z,

			left.x * right.y -
			left.y * right.x
		};
	}

	dx3d::Vec3 normalize(
		const dx3d::Vec3& vector
	) noexcept
	{
		const dx3d::f32 lengthSquared =
			dot(vector, vector);

		if (lengthSquared <=
			intersectionEpsilon)
		{
			return {};
		}

		const dx3d::f32 inverseLength =
			1.0f / std::sqrt(lengthSquared);

		return
		{
			vector.x * inverseLength,
			vector.y * inverseLength,
			vector.z * inverseLength
		};
	}

	bool transformHomogeneousPoint(
		const dx3d::Vec4& homogeneousPoint,
		const dx3d::Mat4x4& matrix,
		dx3d::Vec3& outputPoint
	) noexcept
	{
		const dx3d::Vec4 transformed =
			matrix.transform(homogeneousPoint);

		if (std::fabs(transformed.w) <=
			intersectionEpsilon)
		{
			return false;
		}

		const dx3d::f32 inverseW =
			1.0f / transformed.w;

		outputPoint =
		{
			transformed.x * inverseW,
			transformed.y * inverseW,
			transformed.z * inverseW
		};

		return true;
	}

	dx3d::Vec3 transformMeshPosition(
		const dx3d::Vec3& position,
		const dx3d::Mat4x4& worldMatrix
	) noexcept
	{
		const dx3d::Vec4 transformed =
			worldMatrix.transform(
				{
					position.x,
					position.y,
					position.z,
					1.0f
				}
			);

		return
		{
			transformed.x,
			transformed.y,
			transformed.z
		};
	}
}

bool dx3d::createPickingRay(
	f32 mouseX,
	f32 mouseY,
	const PickingViewportArea& viewportArea,
	CameraComponent& camera,
	PickingRay& outputRay
) noexcept
{
	if (viewportArea.width <= 0.0f ||
		viewportArea.height <= 0.0f)
	{
		return false;
	}

	const bool mouseInsideViewport =
		mouseX >= viewportArea.x &&
		mouseX <=
		viewportArea.x + viewportArea.width &&
		mouseY >= viewportArea.y &&
		mouseY <=
		viewportArea.y + viewportArea.height;

	if (!mouseInsideViewport)
		return false;

	const f32 localMouseX =
		mouseX - viewportArea.x;

	const f32 localMouseY =
		mouseY - viewportArea.y;

	// Convert screen coordinates to Direct3D
	// normalized device coordinates.
	const f32 normalizedX =
		(localMouseX / viewportArea.width) *
		2.0f - 1.0f;

	const f32 normalizedY =
		1.0f -
		(localMouseY / viewportArea.height) *
		2.0f;

	const Mat4x4 viewMatrix =
		camera.getViewMatrix();

	const Mat4x4 projectionMatrix =
		camera.getProjectionMatrix();

	const Mat4x4 inverseViewProjection =
		Mat4x4::inverse(
			viewMatrix * projectionMatrix
		);

	// Direct3D uses a normalized depth range of 0 to 1.
	const Vec4 nearClipPoint
	{
		normalizedX,
		normalizedY,
		0.0f,
		1.0f
	};

	const Vec4 farClipPoint
	{
		normalizedX,
		normalizedY,
		1.0f,
		1.0f
	};

	Vec3 nearWorldPoint{};
	Vec3 farWorldPoint{};

	if (!transformHomogeneousPoint(
		nearClipPoint,
		inverseViewProjection,
		nearWorldPoint
	))
	{
		return false;
	}

	if (!transformHomogeneousPoint(
		farClipPoint,
		inverseViewProjection,
		farWorldPoint
	))
	{
		return false;
	}

	const Vec3 rayDirection =
		normalize(
			subtract(
				farWorldPoint,
				nearWorldPoint
			)
		);

	const f32 directionLengthSquared =
		dot(
			rayDirection,
			rayDirection
		);

	if (directionLengthSquared <=
		intersectionEpsilon)
	{
		return false;
	}

	outputRay.origin =
		nearWorldPoint;

	outputRay.direction =
		rayDirection;

	return true;
}

bool dx3d::intersectRayWithMesh(
	const PickingRay& ray,
	const MeshData& meshData,
	const Mat4x4& worldMatrix,
	f32& outputDistance
) noexcept
{
	if (meshData.vertices.empty() ||
		meshData.indices.size() < 3)
	{
		return false;
	}

	bool foundIntersection = false;

	f32 closestDistance =
		std::numeric_limits<f32>::max();

	// Möller-Trumbore ray-triangle intersection.
	for (
		size_t indexPosition = 0;
		indexPosition + 2 < meshData.indices.size();
		indexPosition += 3
		)
	{
		const ui32 indexA =
			meshData.indices[indexPosition];

		const ui32 indexB =
			meshData.indices[indexPosition + 1];

		const ui32 indexC =
			meshData.indices[indexPosition + 2];

		if (indexA >= meshData.vertices.size() ||
			indexB >= meshData.vertices.size() ||
			indexC >= meshData.vertices.size())
		{
			continue;
		}

		const Vec3 pointA =
			transformMeshPosition(
				meshData.vertices[indexA].position,
				worldMatrix
			);

		const Vec3 pointB =
			transformMeshPosition(
				meshData.vertices[indexB].position,
				worldMatrix
			);

		const Vec3 pointC =
			transformMeshPosition(
				meshData.vertices[indexC].position,
				worldMatrix
			);

		const Vec3 edgeAB =
			subtract(pointB, pointA);

		const Vec3 edgeAC =
			subtract(pointC, pointA);

		const Vec3 directionCrossEdge =
			cross(
				ray.direction,
				edgeAC
			);

		const f32 determinant =
			dot(
				edgeAB,
				directionCrossEdge
			);

		// Do not perform back-face culling. This allows
		// planes and negatively scaled objects to be picked
		// from either side.
		if (std::fabs(determinant) <=
			intersectionEpsilon)
		{
			continue;
		}

		const f32 inverseDeterminant =
			1.0f / determinant;

		const Vec3 originToPoint =
			subtract(
				ray.origin,
				pointA
			);

		const f32 triangleU =
			dot(
				originToPoint,
				directionCrossEdge
			) *
			inverseDeterminant;

		if (triangleU < 0.0f ||
			triangleU > 1.0f)
		{
			continue;
		}

		const Vec3 originCrossEdge =
			cross(
				originToPoint,
				edgeAB
			);

		const f32 triangleV =
			dot(
				ray.direction,
				originCrossEdge
			) *
			inverseDeterminant;

		if (triangleV < 0.0f ||
			triangleU + triangleV > 1.0f)
		{
			continue;
		}

		const f32 intersectionDistance =
			dot(
				edgeAC,
				originCrossEdge
			) *
			inverseDeterminant;

		if (intersectionDistance <=
			intersectionEpsilon)
		{
			continue;
		}

		if (intersectionDistance <
			closestDistance)
		{
			closestDistance =
				intersectionDistance;

			foundIntersection = true;
		}
	}

	if (!foundIntersection)
		return false;

	outputDistance =
		closestDistance;

	return true;
}