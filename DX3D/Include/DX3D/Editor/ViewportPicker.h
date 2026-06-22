#pragma once

#include <DX3D/Core/Core.h>
#include <DX3D/Math/Vec3.h>

namespace dx3d
{
	class CameraComponent;
	class Mat4x4;
	struct MeshData;

	struct PickingViewportArea
	{
		f32 x{};
		f32 y{};
		f32 width{};
		f32 height{};
	};

	struct PickingRay
	{
		Vec3 origin{};
		Vec3 direction{};
	};

	bool createPickingRay(
		f32 mouseX,
		f32 mouseY,
		const PickingViewportArea& viewportArea,
		CameraComponent& camera,
		PickingRay& outputRay
	) noexcept;

	bool intersectRayWithMesh(
		const PickingRay& ray,
		const MeshData& meshData,
		const Mat4x4& worldMatrix,
		f32& outputDistance
	) noexcept;
}