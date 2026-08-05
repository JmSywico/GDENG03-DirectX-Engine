#include <DX3D/Editor/TransformGizmo.h>

#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Math/Mat4x4.h>
#include <DX3D/Math/MathUtils.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Math/Vec4.h>

#include <imgui.h>

#include <cmath>

void dx3d::TransformGizmo::setOperation(
	Operation operation
) noexcept
{
	m_operation = operation;

	m_hoveredAxis = Axis::None;
	m_activeAxis = Axis::None;
	m_isUsing = false;
	m_previousRotationAngle = 0.0f;
}

dx3d::TransformGizmo::Operation
dx3d::TransformGizmo::getOperation() const noexcept
{
	return m_operation;
}

dx3d::TransformGizmo::Axis
dx3d::TransformGizmo::getHoveredAxis() const noexcept
{
	return m_hoveredAxis;
}

dx3d::TransformGizmo::Axis
dx3d::TransformGizmo::getActiveAxis() const noexcept
{
	return m_activeAxis;
}

bool dx3d::TransformGizmo::isUsing() const noexcept
{
	return m_isUsing;
}

bool dx3d::TransformGizmo::projectWorldToScreen(
	const Vec3& worldPosition,
	const Mat4x4& viewProjectionMatrix,
	const ViewportArea& viewportArea,
	ScreenPoint& screenPosition
) const noexcept
{
	const Vec4 worldPoint
	{
		worldPosition.x,
		worldPosition.y,
		worldPosition.z,
		1.0f
	};

	const Vec4 clipPosition =
		viewProjectionMatrix.transform(worldPoint);

	if (clipPosition.w <= 0.001f)
		return false;

	const f32 inverseW =
		1.0f / clipPosition.w;

	const f32 normalizedX =
		clipPosition.x * inverseW;

	const f32 normalizedY =
		clipPosition.y * inverseW;

	const f32 normalizedZ =
		clipPosition.z * inverseW;

	if (normalizedZ < 0.0f ||
		normalizedZ > 1.0f)
	{
		return false;
	}

	screenPosition.x =
		viewportArea.x +
		(normalizedX * 0.5f + 0.5f) *
		viewportArea.width;

	screenPosition.y =
		viewportArea.y +
		(-normalizedY * 0.5f + 0.5f) *
		viewportArea.height;

	return true;
}

dx3d::f32
dx3d::TransformGizmo::distanceToLineSegment(
	const ScreenPoint& point,
	const ScreenPoint& start,
	const ScreenPoint& end
) const noexcept
{
	const f32 lineX =
		end.x - start.x;

	const f32 lineY =
		end.y - start.y;

	const f32 pointX =
		point.x - start.x;

	const f32 pointY =
		point.y - start.y;

	const f32 lineLengthSquared =
		lineX * lineX +
		lineY * lineY;

	if (lineLengthSquared <= 0.0001f)
	{
		const f32 differenceX =
			point.x - start.x;

		const f32 differenceY =
			point.y - start.y;

		return std::sqrt(
			differenceX * differenceX +
			differenceY * differenceY
		);
	}

	f32 projection =
		(pointX * lineX +
			pointY * lineY) /
		lineLengthSquared;

	if (projection < 0.0f)
		projection = 0.0f;

	if (projection > 1.0f)
		projection = 1.0f;

	const f32 closestX =
		start.x +
		projection * lineX;

	const f32 closestY =
		start.y +
		projection * lineY;

	const f32 differenceX =
		point.x - closestX;

	const f32 differenceY =
		point.y - closestY;

	return std::sqrt(
		differenceX * differenceX +
		differenceY * differenceY
	);
}

dx3d::f32
dx3d::TransformGizmo::distanceToLineSegmentWithParameter(
	const ScreenPoint& point,
	const ScreenPoint& start,
	const ScreenPoint& end,
	f32& segmentParameter
) const noexcept
{
	const f32 lineX =
		end.x - start.x;

	const f32 lineY =
		end.y - start.y;

	const f32 lineLengthSquared =
		lineX * lineX +
		lineY * lineY;

	if (lineLengthSquared <= 0.0001f)
	{
		segmentParameter = 0.0f;

		const f32 differenceX =
			point.x - start.x;

		const f32 differenceY =
			point.y - start.y;

		return std::sqrt(
			differenceX * differenceX +
			differenceY * differenceY
		);
	}

	segmentParameter =
		(
			(point.x - start.x) * lineX +
			(point.y - start.y) * lineY
			) /
		lineLengthSquared;

	if (segmentParameter < 0.0f)
		segmentParameter = 0.0f;

	if (segmentParameter > 1.0f)
		segmentParameter = 1.0f;

	const f32 closestX =
		start.x +
		lineX * segmentParameter;

	const f32 closestY =
		start.y +
		lineY * segmentParameter;

	const f32 differenceX =
		point.x - closestX;

	const f32 differenceY =
		point.y - closestY;

	return std::sqrt(
		differenceX * differenceX +
		differenceY * differenceY
	);
}

void dx3d::TransformGizmo::drawRotationGizmo(
	GameObject* selectedObject,
	const Vec3& gizmoOrigin,
	f32 ringRadius,
	const Mat4x4& viewProjectionMatrix,
	const ViewportArea& viewportArea
)
{
	if (!selectedObject)
		return;

	ImDrawList* drawList =
		ImGui::GetForegroundDrawList();

	const ImVec2 clipMinimum
	{
		viewportArea.x,
		viewportArea.y
	};

	const ImVec2 clipMaximum
	{
		viewportArea.x + viewportArea.width,
		viewportArea.y + viewportArea.height
	};

	const ImVec2 mousePositionImGui =
		ImGui::GetMousePos();

	const ScreenPoint mousePosition
	{
		mousePositionImGui.x,
		mousePositionImGui.y
	};

	const bool mouseInsideViewport =
		mousePosition.x >= viewportArea.x &&
		mousePosition.x <=
		viewportArea.x + viewportArea.width &&
		mousePosition.y >= viewportArea.y &&
		mousePosition.y <=
		viewportArea.y + viewportArea.height;

	const bool rightMouseDown =
		ImGui::IsMouseDown(
			ImGuiMouseButton_Right
		);

	const bool canHoverGizmo =
		mouseInsideViewport &&
		!rightMouseDown;

	constexpr ui32 segmentCount = 64;
	constexpr f32 hoverDistance = 10.0f;
	constexpr f32 ringThickness = 3.0f;

	const f32 fullRotation =
		2.0f * MathUtils::PI;

	auto getRingWorldPoint =
		[&](
			Axis axis,
			f32 angle
			) -> Vec3
		{
			const f32 cosine =
				std::cos(angle);

			const f32 sine =
				std::sin(angle);

			Vec3 worldPoint =
				gizmoOrigin;

			switch (axis)
			{
			case Axis::X:
				worldPoint.y +=
					cosine * ringRadius;

				worldPoint.z +=
					sine * ringRadius;
				break;

			case Axis::Y:
				worldPoint.x +=
					cosine * ringRadius;

				worldPoint.z +=
					sine * ringRadius;
				break;

			case Axis::Z:
				worldPoint.x +=
					cosine * ringRadius;

				worldPoint.y +=
					sine * ringRadius;
				break;

			case Axis::None:
			default:
				break;
			}

			return worldPoint;
		};

	auto findClosestRingAngle =
		[&](
			Axis axis,
			f32& closestAngle
			) -> f32
		{
			f32 closestDistance =
				1000000.0f;

			closestAngle = 0.0f;

			ScreenPoint previousPoint{};
			bool previousVisible = false;

			f32 previousAngle = 0.0f;

			for (ui32 index = 0;
				index <= segmentCount;
				++index)
			{
				const f32 currentAngle =
					fullRotation *
					static_cast<f32>(index) /
					static_cast<f32>(segmentCount);

				const Vec3 worldPoint =
					getRingWorldPoint(
						axis,
						currentAngle
					);

				ScreenPoint currentPoint{};

				const bool currentVisible =
					projectWorldToScreen(
						worldPoint,
						viewProjectionMatrix,
						viewportArea,
						currentPoint
					);

				if (previousVisible &&
					currentVisible)
				{
					f32 segmentParameter = 0.0f;

					const f32 distance =
						distanceToLineSegmentWithParameter(
							mousePosition,
							previousPoint,
							currentPoint,
							segmentParameter
						);

					if (distance < closestDistance)
					{
						closestDistance =
							distance;

						closestAngle =
							previousAngle +
							(
								currentAngle -
								previousAngle
								) *
							segmentParameter;
					}
				}

				previousPoint =
					currentPoint;

				previousVisible =
					currentVisible;

				previousAngle =
					currentAngle;
			}

			return closestDistance;
		};

	ScreenPoint centerScreen{};

	const bool centerVisible =
		projectWorldToScreen(
			gizmoOrigin,
			viewProjectionMatrix,
			viewportArea,
			centerScreen
		);

	if (!centerVisible)
		return;

	// --------------------------------------------------
	// Rotation ring hover detection
	// --------------------------------------------------

	m_hoveredAxis = Axis::None;

	if (!m_isUsing && canHoverGizmo)
	{
		f32 closestDistance =
			hoverDistance;

		f32 unusedAngle = 0.0f;

		const f32 xDistance =
			findClosestRingAngle(
				Axis::X,
				unusedAngle
			);

		if (xDistance < closestDistance)
		{
			closestDistance = xDistance;
			m_hoveredAxis = Axis::X;
		}

		const f32 yDistance =
			findClosestRingAngle(
				Axis::Y,
				unusedAngle
			);

		if (yDistance < closestDistance)
		{
			closestDistance = yDistance;
			m_hoveredAxis = Axis::Y;
		}

		const f32 zDistance =
			findClosestRingAngle(
				Axis::Z,
				unusedAngle
			);

		if (zDistance < closestDistance)
		{
			closestDistance = zDistance;
			m_hoveredAxis = Axis::Z;
		}
	}

	const bool leftMouseClicked =
		ImGui::IsMouseClicked(
			ImGuiMouseButton_Left
		);

	const bool leftMouseDown =
		ImGui::IsMouseDown(
			ImGuiMouseButton_Left
		);

	const bool leftMouseReleased =
		ImGui::IsMouseReleased(
			ImGuiMouseButton_Left
		);

	// --------------------------------------------------
	// Start rotation dragging
	// --------------------------------------------------

	if (!m_isUsing &&
		canHoverGizmo &&
		m_hoveredAxis != Axis::None &&
		leftMouseClicked)
	{
		f32 startingAngle = 0.0f;

		const f32 startingDistance =
			findClosestRingAngle(
				m_hoveredAxis,
				startingAngle
			);

		if (startingDistance <= hoverDistance)
		{
			m_activeAxis =
				m_hoveredAxis;

			m_previousRotationAngle =
				startingAngle;

			m_isUsing = true;
		}
	}

	// --------------------------------------------------
	// Continue rotation dragging
	// --------------------------------------------------

	if (m_isUsing)
	{
		if (leftMouseReleased ||
			!leftMouseDown)
		{
			m_activeAxis = Axis::None;
			m_isUsing = false;
		}
		else
		{
			f32 currentAngle = 0.0f;

			const f32 currentDistance =
				findClosestRingAngle(
					m_activeAxis,
					currentAngle
				);

			if (currentDistance < 1000000.0f)
			{
				f32 angleDifference =
					currentAngle -
					m_previousRotationAngle;

				if (angleDifference >
					MathUtils::PI)
				{
					angleDifference -=
						fullRotation;
				}

				if (angleDifference <
					-MathUtils::PI)
				{
					angleDifference +=
						fullRotation;
				}

				auto& transform =
					selectedObject->getTransform();

				auto rotation =
					transform.getRotation();

				switch (m_activeAxis)
				{
				case Axis::X:
					rotation.x +=
						angleDifference;
					break;

				case Axis::Y:
					rotation.y +=
						angleDifference;
					break;

				case Axis::Z:
					rotation.z +=
						angleDifference;
					break;

				case Axis::None:
				default:
					break;
				}

				transform.setRotation(rotation);

				m_previousRotationAngle =
					currentAngle;
			}
		}
	}

	const ImU32 normalXAxisColor =
		IM_COL32(230, 70, 70, 255);

	const ImU32 normalYAxisColor =
		IM_COL32(70, 220, 90, 255);

	const ImU32 normalZAxisColor =
		IM_COL32(70, 130, 240, 255);

	const ImU32 highlightedColor =
		IM_COL32(255, 220, 70, 255);

	const bool xRingHighlighted =
		m_isUsing
		? m_activeAxis == Axis::X
		: m_hoveredAxis == Axis::X;

	const bool yRingHighlighted =
		m_isUsing
		? m_activeAxis == Axis::Y
		: m_hoveredAxis == Axis::Y;

	const bool zRingHighlighted =
		m_isUsing
		? m_activeAxis == Axis::Z
		: m_hoveredAxis == Axis::Z;

	const ImU32 xRingColor =
		xRingHighlighted
		? highlightedColor
		: normalXAxisColor;

	const ImU32 yRingColor =
		yRingHighlighted
		? highlightedColor
		: normalYAxisColor;

	const ImU32 zRingColor =
		zRingHighlighted
		? highlightedColor
		: normalZAxisColor;

	drawList->PushClipRect(
		clipMinimum,
		clipMaximum,
		true
	);

	auto drawRing =
		[&](
			Axis axis,
			ImU32 color
			)
		{
			ScreenPoint previousPoint{};
			bool previousVisible = false;

			for (ui32 index = 0;
				index <= segmentCount;
				++index)
			{
				const f32 angle =
					fullRotation *
					static_cast<f32>(index) /
					static_cast<f32>(segmentCount);

				const Vec3 worldPoint =
					getRingWorldPoint(
						axis,
						angle
					);

				ScreenPoint currentPoint{};

				const bool currentVisible =
					projectWorldToScreen(
						worldPoint,
						viewProjectionMatrix,
						viewportArea,
						currentPoint
					);

				if (previousVisible &&
					currentVisible)
				{
					drawList->AddLine(
						{
							previousPoint.x,
							previousPoint.y
						},
					{
						currentPoint.x,
						currentPoint.y
					},
						color,
						ringThickness
					);
				}

				previousPoint =
					currentPoint;

				previousVisible =
					currentVisible;
			}
		};

	drawRing(
		Axis::X,
		xRingColor
	);

	drawRing(
		Axis::Y,
		yRingColor
	);

	drawRing(
		Axis::Z,
		zRingColor
	);

	drawList->AddCircleFilled(
		{
			centerScreen.x,
			centerScreen.y
		},
		4.0f,
		IM_COL32(240, 240, 240, 255)
	);

	drawList->PopClipRect();
}

void dx3d::TransformGizmo::draw(
	GameObject* selectedObject,
	CameraComponent* camera,
	const ViewportArea& viewportArea
)
{
	m_hoveredAxis = Axis::None;

	if (!selectedObject || !camera)
	{
		m_activeAxis = Axis::None;
		m_isUsing = false;
		return;
	}

	if (viewportArea.width <= 0.0f ||
		viewportArea.height <= 0.0f)
	{
		m_activeAxis = Axis::None;
		m_isUsing = false;
		return;
	}

	const bool isTranslate =
		m_operation == Operation::Translate;

	const bool isRotate =
		m_operation == Operation::Rotate;

	const bool isScale =
		m_operation == Operation::Scale;

	if (!isTranslate &&
		!isRotate &&
		!isScale)
	{
		return;
	}

	const Mat4x4 viewMatrix =
		camera->getViewMatrix();

	const Mat4x4 projectionMatrix =
		camera->getProjectionMatrix();

	const Mat4x4 viewProjectionMatrix =
		viewMatrix * projectionMatrix;

	const Vec3 gizmoOrigin =
		selectedObject->getTransform().getPosition();

	const Vec4 originInViewSpace =
		viewMatrix.transform(
			{
				gizmoOrigin.x,
				gizmoOrigin.y,
				gizmoOrigin.z,
				1.0f
			}
		);

	if (originInViewSpace.z <= 0.001f)
	{
		m_activeAxis = Axis::None;
		m_isUsing = false;
		return;
	}

	f32 axisLength =
		originInViewSpace.z * 0.10f;

	if (axisLength < 0.5f)
		axisLength = 0.5f;

	if (axisLength > 5.0f)
		axisLength = 5.0f;

	if (isRotate)
	{
		const f32 ringRadius =
			axisLength * 0.85f;

		drawRotationGizmo(
			selectedObject,
			gizmoOrigin,
			ringRadius,
			viewProjectionMatrix,
			viewportArea
		);

		return;
	}

	const Vec3 xAxisEnd =
		gizmoOrigin +
		Vec3{ axisLength, 0.0f, 0.0f };

	const Vec3 yAxisEnd =
		gizmoOrigin +
		Vec3{ 0.0f, axisLength, 0.0f };

	const Vec3 zAxisEnd =
		gizmoOrigin +
		Vec3{ 0.0f, 0.0f, axisLength };

	ScreenPoint originScreen{};
	ScreenPoint xAxisScreen{};
	ScreenPoint yAxisScreen{};
	ScreenPoint zAxisScreen{};

	const bool originVisible =
		projectWorldToScreen(
			gizmoOrigin,
			viewProjectionMatrix,
			viewportArea,
			originScreen
		);

	const bool xAxisVisible =
		projectWorldToScreen(
			xAxisEnd,
			viewProjectionMatrix,
			viewportArea,
			xAxisScreen
		);

	const bool yAxisVisible =
		projectWorldToScreen(
			yAxisEnd,
			viewProjectionMatrix,
			viewportArea,
			yAxisScreen
		);

	const bool zAxisVisible =
		projectWorldToScreen(
			zAxisEnd,
			viewProjectionMatrix,
			viewportArea,
			zAxisScreen
		);

	const ImVec2 mousePositionImGui =
		ImGui::GetMousePos();

	const ScreenPoint mousePosition
	{
		mousePositionImGui.x,
		mousePositionImGui.y
	};

	const bool mouseInsideViewport =
		mousePosition.x >= viewportArea.x &&
		mousePosition.x <=
		viewportArea.x + viewportArea.width &&
		mousePosition.y >= viewportArea.y &&
		mousePosition.y <=
		viewportArea.y + viewportArea.height;

	const bool rightMouseDown =
		ImGui::IsMouseDown(
			ImGuiMouseButton_Right
		);

	const bool canHoverGizmo =
		mouseInsideViewport &&
		!rightMouseDown;

	if (originVisible &&
		!m_isUsing &&
		canHoverGizmo)
	{
		constexpr f32 hoverDistance = 10.0f;

		f32 closestDistance =
			hoverDistance;

		if (xAxisVisible)
		{
			const f32 distance =
				distanceToLineSegment(
					mousePosition,
					originScreen,
					xAxisScreen
				);

			if (distance < closestDistance)
			{
				closestDistance = distance;
				m_hoveredAxis = Axis::X;
			}
		}

		if (yAxisVisible)
		{
			const f32 distance =
				distanceToLineSegment(
					mousePosition,
					originScreen,
					yAxisScreen
				);

			if (distance < closestDistance)
			{
				closestDistance = distance;
				m_hoveredAxis = Axis::Y;
			}
		}

		if (zAxisVisible)
		{
			const f32 distance =
				distanceToLineSegment(
					mousePosition,
					originScreen,
					zAxisScreen
				);

			if (distance < closestDistance)
			{
				closestDistance = distance;
				m_hoveredAxis = Axis::Z;
			}
		}
	}

	const bool leftMouseClicked =
		ImGui::IsMouseClicked(
			ImGuiMouseButton_Left
		);

	const bool leftMouseDown =
		ImGui::IsMouseDown(
			ImGuiMouseButton_Left
		);

	const bool leftMouseReleased =
		ImGui::IsMouseReleased(
			ImGuiMouseButton_Left
		);

	if (!m_isUsing &&
		canHoverGizmo &&
		m_hoveredAxis != Axis::None &&
		leftMouseClicked)
	{
		m_activeAxis =
			m_hoveredAxis;

		m_previousMousePosition =
			mousePosition;

		m_isUsing = true;
	}

	if (m_isUsing)
	{
		if (leftMouseReleased ||
			!leftMouseDown)
		{
			m_activeAxis = Axis::None;
			m_isUsing = false;
		}
		else if (originVisible)
		{
			ScreenPoint activeAxisScreen{};
			Vec3 activeWorldAxis{};

			bool activeAxisVisible = false;

			switch (m_activeAxis)
			{
			case Axis::X:
				activeAxisScreen =
					xAxisScreen;

				activeWorldAxis =
				{ 1.0f, 0.0f, 0.0f };

				activeAxisVisible =
					xAxisVisible;
				break;

			case Axis::Y:
				activeAxisScreen =
					yAxisScreen;

				activeWorldAxis =
				{ 0.0f, 1.0f, 0.0f };

				activeAxisVisible =
					yAxisVisible;
				break;

			case Axis::Z:
				activeAxisScreen =
					zAxisScreen;

				activeWorldAxis =
				{ 0.0f, 0.0f, 1.0f };

				activeAxisVisible =
					zAxisVisible;
				break;

			case Axis::None:
			default:
				break;
			}

			if (activeAxisVisible)
			{
				const f32 axisScreenX =
					activeAxisScreen.x -
					originScreen.x;

				const f32 axisScreenY =
					activeAxisScreen.y -
					originScreen.y;

				const f32 axisScreenLength =
					std::sqrt(
						axisScreenX * axisScreenX +
						axisScreenY * axisScreenY
					);

				if (axisScreenLength > 1.0f)
				{
					const f32 normalizedAxisX =
						axisScreenX /
						axisScreenLength;

					const f32 normalizedAxisY =
						axisScreenY /
						axisScreenLength;

					const f32 mouseDeltaX =
						mousePosition.x -
						m_previousMousePosition.x;

					const f32 mouseDeltaY =
						mousePosition.y -
						m_previousMousePosition.y;

					const f32 pixelMovement =
						mouseDeltaX *
						normalizedAxisX +
						mouseDeltaY *
						normalizedAxisY;

					auto& transform =
						selectedObject->getTransform();

					if (isTranslate)
					{
						const f32 worldUnitsPerPixel =
							axisLength /
							axisScreenLength;

						const f32 worldMovement =
							pixelMovement *
							worldUnitsPerPixel;

						auto position =
							transform.getPosition();

						position.x +=
							activeWorldAxis.x *
							worldMovement;

						position.y +=
							activeWorldAxis.y *
							worldMovement;

						position.z +=
							activeWorldAxis.z *
							worldMovement;

						transform.setPosition(
							position
						);
					}
					else if (isScale)
					{
						constexpr f32 scaleSensitivity =
							0.01f;

						constexpr f32 minimumScale =
							0.05f;

						const f32 scaleMovement =
							pixelMovement *
							scaleSensitivity;

						auto scale =
							transform.getScale();

						switch (m_activeAxis)
						{
						case Axis::X:
							scale.x += scaleMovement;

							if (scale.x < minimumScale)
								scale.x = minimumScale;
							break;

						case Axis::Y:
							scale.y += scaleMovement;

							if (scale.y < minimumScale)
								scale.y = minimumScale;
							break;

						case Axis::Z:
							scale.z += scaleMovement;

							if (scale.z < minimumScale)
								scale.z = minimumScale;
							break;

						case Axis::None:
						default:
							break;
						}

						transform.setScale(scale);
					}
				}
			}

			m_previousMousePosition =
				mousePosition;
		}
	}

	if (!originVisible)
		return;

	ImDrawList* drawList =
		ImGui::GetForegroundDrawList();

	const ImVec2 clipMinimum
	{
		viewportArea.x,
		viewportArea.y
	};

	const ImVec2 clipMaximum
	{
		viewportArea.x + viewportArea.width,
		viewportArea.y + viewportArea.height
	};

	drawList->PushClipRect(
		clipMinimum,
		clipMaximum,
		true
	);

	const ImVec2 originPosition
	{
		originScreen.x,
		originScreen.y
	};

	const ImU32 normalXAxisColor =
		IM_COL32(230, 70, 70, 255);

	const ImU32 normalYAxisColor =
		IM_COL32(70, 220, 90, 255);

	const ImU32 normalZAxisColor =
		IM_COL32(70, 130, 240, 255);

	const ImU32 highlightedAxisColor =
		IM_COL32(255, 220, 70, 255);

	const bool xAxisHighlighted =
		m_isUsing
		? m_activeAxis == Axis::X
		: m_hoveredAxis == Axis::X;

	const bool yAxisHighlighted =
		m_isUsing
		? m_activeAxis == Axis::Y
		: m_hoveredAxis == Axis::Y;

	const bool zAxisHighlighted =
		m_isUsing
		? m_activeAxis == Axis::Z
		: m_hoveredAxis == Axis::Z;

	const ImU32 xAxisColor =
		xAxisHighlighted
		? highlightedAxisColor
		: normalXAxisColor;

	const ImU32 yAxisColor =
		yAxisHighlighted
		? highlightedAxisColor
		: normalYAxisColor;

	const ImU32 zAxisColor =
		zAxisHighlighted
		? highlightedAxisColor
		: normalZAxisColor;

	constexpr f32 axisThickness = 4.0f;
	constexpr f32 handleRadius = 6.0f;

	auto drawAxisHandle =
		[&](
			const ImVec2& endpoint,
			ImU32 color
			)
		{
			if (isScale)
			{
				const ImVec2 minimum
				{
					endpoint.x - handleRadius,
					endpoint.y - handleRadius
				};

				const ImVec2 maximum
				{
					endpoint.x + handleRadius,
					endpoint.y + handleRadius
				};

				drawList->AddRectFilled(
					minimum,
					maximum,
					color
				);
			}
			else
			{
				drawList->AddCircleFilled(
					endpoint,
					handleRadius,
					color
				);
			}
		};

	if (xAxisVisible)
	{
		const ImVec2 endpoint
		{
			xAxisScreen.x,
			xAxisScreen.y
		};

		drawList->AddLine(
			originPosition,
			endpoint,
			xAxisColor,
			axisThickness
		);

		drawAxisHandle(
			endpoint,
			xAxisColor
		);

		drawList->AddText(
			{
				endpoint.x + 8.0f,
				endpoint.y - 8.0f
			},
			xAxisColor,
			"X"
		);
	}

	if (yAxisVisible)
	{
		const ImVec2 endpoint
		{
			yAxisScreen.x,
			yAxisScreen.y
		};

		drawList->AddLine(
			originPosition,
			endpoint,
			yAxisColor,
			axisThickness
		);

		drawAxisHandle(
			endpoint,
			yAxisColor
		);

		drawList->AddText(
			{
				endpoint.x + 8.0f,
				endpoint.y - 8.0f
			},
			yAxisColor,
			"Y"
		);
	}

	if (zAxisVisible)
	{
		const ImVec2 endpoint
		{
			zAxisScreen.x,
			zAxisScreen.y
		};

		drawList->AddLine(
			originPosition,
			endpoint,
			zAxisColor,
			axisThickness
		);

		drawAxisHandle(
			endpoint,
			zAxisColor
		);

		drawList->AddText(
			{
				endpoint.x + 8.0f,
				endpoint.y - 8.0f
			},
			zAxisColor,
			"Z"
		);
	}

	drawList->AddCircleFilled(
		originPosition,
		5.0f,
		IM_COL32(240, 240, 240, 255)
	);

	drawList->PopClipRect();
}
