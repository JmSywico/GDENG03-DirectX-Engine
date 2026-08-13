#pragma once

#include <DX3D/Core/Core.h>

namespace dx3d
{
	class GameObject;
	class CameraComponent;
	class Vec3;
	class Mat4x4;

	class TransformGizmo final
	{
	public:
		enum class Operation
		{
			Translate,
			Rotate,
			Scale
		};

		enum class Axis
		{
			None,
			X,
			Y,
			Z,
			Center
		};

		struct ViewportArea
		{
			f32 x{};
			f32 y{};
			f32 width{};
			f32 height{};
		};

	public:
		TransformGizmo() = default;

		void setOperation(
			Operation operation
		) noexcept;

		Operation getOperation() const noexcept;

		Axis getHoveredAxis() const noexcept;
		Axis getActiveAxis() const noexcept;

		bool isUsing() const noexcept;

		void draw(
			GameObject* selectedObject,
			CameraComponent* camera,
			const ViewportArea& renderViewportArea,
			const ViewportArea& interactionViewportArea
		);

	private:
		struct ScreenPoint
		{
			f32 x{};
			f32 y{};
		};

		bool projectWorldToScreen(
			const Vec3& worldPosition,
			const Mat4x4& viewProjectionMatrix,
			const ViewportArea& viewportArea,
			ScreenPoint& screenPosition
		) const noexcept;

		f32 distanceToLineSegment(
			const ScreenPoint& point,
			const ScreenPoint& start,
			const ScreenPoint& end
		) const noexcept;

		f32 distanceBetweenScreenPoints(
			const ScreenPoint& lhs,
			const ScreenPoint& rhs
		) const noexcept;

		f32 distanceToLineSegmentWithParameter(
			const ScreenPoint& point,
			const ScreenPoint& start,
			const ScreenPoint& end,
			f32& segmentParameter
		) const noexcept;

		void drawRotationGizmo(
			CameraComponent* camera,
			GameObject* selectedObject,
			const Vec3& gizmoOrigin,
			f32 ringRadius,
			f32 centerHandleRadius,
			const Mat4x4& viewProjectionMatrix,
			const ViewportArea& renderViewportArea,
			const ViewportArea& interactionViewportArea
		);

	private:
		Operation m_operation{
			Operation::Translate
		};

		Axis m_hoveredAxis{
			Axis::None
		};

		Axis m_activeAxis{
			Axis::None
		};

		bool m_isUsing{};

		ScreenPoint m_previousMousePosition{};

		f32 m_previousRotationAngle{};
	};
}
