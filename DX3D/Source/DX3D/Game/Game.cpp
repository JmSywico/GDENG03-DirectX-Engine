#include <DX3D/Game/Game.h>
#include <DX3D/Window/Window.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Graphics/MeshMerger.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Core/Logger.h>
#include <DX3D/Input/InputSystem.h>
#include <DX3D/Game/Display.h>
#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/WorldRenderer.h>
#include <DX3D/Game/SceneSerializer.h>
#include <DX3D/Editor/ViewportPicker.h>

#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>

#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iterator>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

namespace
{
	ImVec4 rgba(
		int red,
		int green,
		int blue,
		int alpha = 255
	)
	{
		return ImVec4(
			red / 255.0f,
			green / 255.0f,
			blue / 255.0f,
			alpha / 255.0f
		);
	}

	void applyWorkbenchStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowPadding = ImVec2(10.0f, 9.0f);
		style.FramePadding = ImVec2(7.0f, 4.0f);
		style.CellPadding = ImVec2(7.0f, 5.0f);
		style.ItemSpacing = ImVec2(7.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
		style.ScrollbarSize = 11.0f;
		style.GrabMinSize = 8.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		style.WindowRounding = 2.0f;
		style.ChildRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 2.0f;
		style.GrabRounding = 1.0f;
		style.TabRounding = 2.0f;
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

		auto* colors = style.Colors;
		colors[ImGuiCol_Text] = rgba(220, 224, 219);
		colors[ImGuiCol_TextDisabled] = rgba(119, 129, 126);
		colors[ImGuiCol_WindowBg] = rgba(20, 25, 27, 247);
		colors[ImGuiCol_ChildBg] = rgba(17, 22, 24, 235);
		colors[ImGuiCol_PopupBg] = rgba(24, 30, 31, 252);
		colors[ImGuiCol_Border] = rgba(55, 66, 66);
		colors[ImGuiCol_BorderShadow] = rgba(0, 0, 0, 0);
		colors[ImGuiCol_FrameBg] = rgba(31, 38, 39);
		colors[ImGuiCol_FrameBgHovered] = rgba(42, 53, 52);
		colors[ImGuiCol_FrameBgActive] = rgba(52, 67, 64);
		colors[ImGuiCol_TitleBg] = rgba(17, 22, 24);
		colors[ImGuiCol_TitleBgActive] = rgba(23, 30, 31);
		colors[ImGuiCol_TitleBgCollapsed] = rgba(17, 22, 24);
		colors[ImGuiCol_MenuBarBg] = rgba(14, 19, 21);
		colors[ImGuiCol_ScrollbarBg] = rgba(15, 20, 22);
		colors[ImGuiCol_ScrollbarGrab] = rgba(55, 65, 64);
		colors[ImGuiCol_ScrollbarGrabHovered] = rgba(70, 83, 80);
		colors[ImGuiCol_ScrollbarGrabActive] = rgba(91, 108, 102);
		colors[ImGuiCol_CheckMark] = rgba(114, 211, 174);
		colors[ImGuiCol_SliderGrab] = rgba(96, 177, 148);
		colors[ImGuiCol_SliderGrabActive] = rgba(126, 226, 189);
		colors[ImGuiCol_Button] = rgba(36, 45, 45);
		colors[ImGuiCol_ButtonHovered] = rgba(51, 65, 62);
		colors[ImGuiCol_ButtonActive] = rgba(65, 88, 79);
		colors[ImGuiCol_Header] = rgba(45, 64, 59);
		colors[ImGuiCol_HeaderHovered] = rgba(56, 78, 71);
		colors[ImGuiCol_HeaderActive] = rgba(67, 94, 84);
		colors[ImGuiCol_Separator] = rgba(49, 60, 60);
		colors[ImGuiCol_SeparatorHovered] = rgba(89, 150, 129);
		colors[ImGuiCol_SeparatorActive] = rgba(114, 211, 174);
		colors[ImGuiCol_ResizeGrip] = rgba(70, 91, 84, 90);
		colors[ImGuiCol_ResizeGripHovered] = rgba(114, 211, 174, 160);
		colors[ImGuiCol_ResizeGripActive] = rgba(114, 211, 174, 220);
		colors[ImGuiCol_Tab] = rgba(24, 31, 32);
		colors[ImGuiCol_TabHovered] = rgba(48, 66, 62);
		colors[ImGuiCol_TabActive] = rgba(39, 55, 51);
		colors[ImGuiCol_DockingPreview] = rgba(114, 211, 174, 115);
		colors[ImGuiCol_TableHeaderBg] = rgba(26, 34, 35);
		colors[ImGuiCol_TableBorderStrong] = rgba(55, 66, 66);
		colors[ImGuiCol_TableBorderLight] = rgba(40, 49, 49);
		colors[ImGuiCol_TableRowBgAlt] = rgba(255, 255, 255, 7);
		colors[ImGuiCol_TextSelectedBg] = rgba(80, 151, 126, 110);
		colors[ImGuiCol_NavHighlight] = rgba(114, 211, 174, 180);
	}
}



dx3d::Game::Game(const GameDesc& desc)
{
	m_logger = std::make_unique<Logger>(desc.logLevel);

	DX3DLogInfo("GDENG03 | DirectX Game Engine");
	DX3DLogInfo("--------------------------------------");

	m_inputSystem = std::make_unique<InputSystem>(InputSystemDesc{ *m_logger });
	m_graphicsDevice = std::make_shared<GraphicsDevice>(GraphicsDeviceDesc{ *m_logger });
	m_display = std::make_unique<Display>(DisplayDesc{ {*m_logger,desc.windowSize},*m_graphicsDevice });
	m_world = std::make_unique<World>(WorldDesc{ BaseDesc{*m_logger}, GameContext{*m_inputSystem} });
	m_worldRenderer = std::make_unique<WorldRenderer>(WorldRendererDesc{ {*m_logger},*m_graphicsDevice });

	// Initialize Dear ImGui.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	applyWorkbenchStyle();

	// Prefer the native Windows variable UI face. Keep startup resilient on
	// older Windows installations where this font is not present.
	ImFont* editorFont = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\SegUIVar.ttf",
		16.0f
	);

	if (!editorFont)
	{
		editorFont = io.Fonts->AddFontFromFileTTF(
			"C:\\Windows\\Fonts\\segoeui.ttf",
			16.0f
		);
	}

	if (editorFont)
	{
		io.FontDefault = editorFont;
	}

	if (!ImGui_ImplWin32_Init(
		m_display->getNativeHandle()
	))
	{
		DX3DLogThrowError(
			"ImGui Win32 initialization failed."
		);
	}

	if (!ImGui_ImplDX11_Init(
		m_graphicsDevice->getD3DDevice(),
		m_graphicsDevice->getImmediateContext()
	))
	{
		DX3DLogThrowError(
			"ImGui DirectX 11 initialization failed."
		);
	}

	m_inputSystem->setCursorLockArea(m_display->getClientAreaInScreenSpace());

	DX3DLogInfo("Game initialized.");
}

dx3d::Game::~Game()
{
	DX3DLogInfo("Game is shutting down...");

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

dx3d::World& dx3d::Game::getWorld() noexcept
{
	return *m_world;
}

dx3d::Logger& dx3d::Game::getLogger() noexcept
{
	return *m_logger;
}

dx3d::InputSystem& dx3d::Game::getInputSystem() noexcept
{
	return *m_inputSystem;
}

void dx3d::Game::requestExit() noexcept
{
	m_isRunning = false;
}

bool dx3d::Game::isObjectSelected(
	const GameObject* object
) const noexcept
{
	if (!object)
		return false;

	return std::find(
		m_selectedObjects.begin(),
		m_selectedObjects.end(),
		object
	) != m_selectedObjects.end();
}

void dx3d::Game::selectOnly(
	GameObject* object
)
{
	m_selectedObjects.clear();
	m_selectedObject = object;

	if (object)
	{
		m_selectedObjects.push_back(object);
	}
}

void dx3d::Game::toggleObjectSelection(
	GameObject* object
)
{
	if (!object)
		return;

	const auto selectedIt =
		std::find(
			m_selectedObjects.begin(),
			m_selectedObjects.end(),
			object
		);

	if (selectedIt == m_selectedObjects.end())
	{
		m_selectedObjects.push_back(object);
		m_selectedObject = object;
		return;
	}

	m_selectedObjects.erase(selectedIt);

	if (m_selectedObject == object)
	{
		if (m_selectedObjects.empty())
		{
			m_selectedObject = nullptr;
		}
		else
		{
			m_selectedObject =
				m_selectedObjects.back();
		}
	}
}

void dx3d::Game::removeObjectFromSelection(
	GameObject* object
)
{
	if (!object)
		return;

	m_selectedObjects.erase(
		std::remove(
			m_selectedObjects.begin(),
			m_selectedObjects.end(),
			object
		),
		m_selectedObjects.end()
	);

	if (m_selectedObject == object)
	{
		if (m_selectedObjects.empty())
		{
			m_selectedObject = nullptr;
		}
		else
		{
			m_selectedObject =
				m_selectedObjects.back();
		}
	}
}

void dx3d::Game::clearSelection() noexcept
{
	m_selectedObjects.clear();
	m_selectedObject = nullptr;
}

const dx3d::MeshData*
dx3d::Game::getObjectMeshData(
	GameObject* object
) const noexcept
{
	if (!object)
		return nullptr;

	// Check custom combined meshes first.
	if (auto* combinedComponent =
		object->getComponent<CombinedMeshComponent>())
	{
		if (combinedComponent->hasMeshData())
		{
			return &combinedComponent->getMeshData();
		}

		return nullptr;
	}

	if (object->getComponent<CubeComponent>())
	{
		return &getCubeMeshData();
	}

	if (object->getComponent<PlaneComponent>())
	{
		return &getPlaneMeshData();
	}

	return nullptr;
}

bool dx3d::Game::canMergeSelectedObjects() const noexcept
{
	if (m_selectedObjects.size() < 2)
		return false;

	for (auto* object : m_selectedObjects)
	{
		if (!object)
			return false;

		// The editor camera and unsupported object types
		// cannot participate in a mesh merge.
		if (object->getComponent<CameraComponent>())
			return false;

		if (!getObjectMeshData(object))
			return false;
	}

	return true;
}

void dx3d::Game::mergeSelectedObjects()
{
	if (!canMergeSelectedObjects())
		return;

	std::vector<MeshMergeSource> sources{};
	sources.reserve(m_selectedObjects.size());

	for (auto* object : m_selectedObjects)
	{
		const MeshData* meshData =
			getObjectMeshData(object);

		if (!meshData)
			return;

		sources.push_back(
			{
				meshData,
				object->getTransform()
					.getAffineWorldMatrix()
			}
		);
	}

	MeshData combinedMesh =
		mergeMeshes(sources);

	if (combinedMesh.empty())
		return;

	// Calculate a bounding-box center for the new pivot.
	Vec3 minimum =
		combinedMesh.vertices.front().position;

	Vec3 maximum =
		combinedMesh.vertices.front().position;

	for (const auto& vertex : combinedMesh.vertices)
	{
		const Vec3& position =
			vertex.position;

		minimum.x =
			std::min(minimum.x, position.x);

		minimum.y =
			std::min(minimum.y, position.y);

		minimum.z =
			std::min(minimum.z, position.z);

		maximum.x =
			std::max(maximum.x, position.x);

		maximum.y =
			std::max(maximum.y, position.y);

		maximum.z =
			std::max(maximum.z, position.z);
	}

	const Vec3 mergedPivot
	{
		(minimum.x + maximum.x) * 0.5f,
		(minimum.y + maximum.y) * 0.5f,
		(minimum.z + maximum.z) * 0.5f
	};

	// The vertices are currently in world space.
	// Move them relative to the new object's centered pivot.
	for (auto& vertex : combinedMesh.vertices)
	{
		vertex.position.x -= mergedPivot.x;
		vertex.position.y -= mergedPivot.y;
		vertex.position.z -= mergedPivot.z;
	}

	// Keep a copy because selection will be replaced after
	// creating the merged object.
	const std::vector<GameObject*> originalObjects =
		m_selectedObjects;

	auto* mergedObject =
		m_world->createGameObject<GameObject>();

	mergedObject->setName(
		"Merged Object"
	);

	auto* combinedComponent =
		mergedObject->createOrGetComponent<
		CombinedMeshComponent>();

	combinedComponent->setMeshData(
		combinedMesh
	);

	mergedObject->getTransform().setPosition(
		mergedPivot
	);

	mergedObject->getTransform().setRotation(
		{ 0.0f, 0.0f, 0.0f }
	);

	mergedObject->getTransform().setScale(
		{ 1.0f, 1.0f, 1.0f }
	);

	// Queue the original objects for removal only after
	// the new merged object was created successfully.
	for (auto* originalObject : originalObjects)
	{
		m_world->destroyGameObject(
			originalObject
		);
	}

	// Remove every old pointer from editor selection and
	// make the new combined object active.
	selectOnly(mergedObject);
}

bool dx3d::Game::canCopySelectedObject() const noexcept
{
	if (!m_selectedObject)
		return false;

	// Do not allow the editor camera to be copied.
	if (m_selectedObject->getComponent<
		CameraComponent>())
	{
		return false;
	}

	if (auto* combinedComponent =
		m_selectedObject->getComponent<
		CombinedMeshComponent>())
	{
		return combinedComponent->hasMeshData();
	}

	if (m_selectedObject->getComponent<
		CubeComponent>())
	{
		return true;
	}

	if (m_selectedObject->getComponent<
		PlaneComponent>())
	{
		return true;
	}

	return false;
}

void dx3d::Game::copySelectedObject()
{
	if (!canCopySelectedObject())
		return;

	ObjectCopyData copiedData{};

	copiedData.isValid = true;
	copiedData.sourceName =
		m_selectedObject->getName();

	auto& transform =
		m_selectedObject->getTransform();

	copiedData.position =
		transform.getPosition();

	copiedData.rotation =
		transform.getRotation();

	copiedData.scale =
		transform.getScale();

	// Combined meshes must copy their complete custom
	// vertex and index data.
	if (auto* combinedComponent =
		m_selectedObject->getComponent<
		CombinedMeshComponent>())
	{
		copiedData.type =
			CopiedObjectType::CombinedMesh;

		copiedData.meshData =
			combinedComponent->getMeshData();
	}
	else if (
		m_selectedObject->getComponent<
		CubeComponent>())
	{
		copiedData.type =
			CopiedObjectType::Cube;
	}
	else if (
		m_selectedObject->getComponent<
		PlaneComponent>())
	{
		copiedData.type =
			CopiedObjectType::Plane;
	}
	else
	{
		return;
	}

	// A new copy operation restarts the pasted-object count.
	copiedData.pasteCount = 0;

	m_objectClipboard =
		copiedData;
}

void dx3d::Game::pasteCopiedObject()
{
	if (!m_objectClipboard.isValid)
		return;

	auto* pastedObject =
		m_world->createGameObject<GameObject>();

	switch (m_objectClipboard.type)
	{
	case CopiedObjectType::Cube:
		pastedObject->createOrGetComponent<
			CubeComponent>();
		break;

	case CopiedObjectType::Plane:
		pastedObject->createOrGetComponent<
			PlaneComponent>();
		break;

	case CopiedObjectType::CombinedMesh:
	{
		auto* combinedComponent =
			pastedObject->createOrGetComponent<
			CombinedMeshComponent>();

		combinedComponent->setMeshData(
			m_objectClipboard.meshData
		);

		break;
	}

	case CopiedObjectType::None:
	default:
		m_world->destroyGameObject(
			pastedObject
		);
		return;
	}

	++m_objectClipboard.pasteCount;

	std::string pastedName =
		m_objectClipboard.sourceName +
		" Copy";

	if (m_objectClipboard.pasteCount > 1)
	{
		pastedName +=
			" (" +
			std::to_string(
				m_objectClipboard.pasteCount
			) +
			")";
	}

	pastedObject->setName(
		pastedName
	);

	// Preserve the exact transform of the copied object.
	auto& pastedTransform =
		pastedObject->getTransform();

	pastedTransform.setPosition(
		m_objectClipboard.position
	);

	pastedTransform.setRotation(
		m_objectClipboard.rotation
	);

	pastedTransform.setScale(
		m_objectClipboard.scale
	);

	// Automatically select the newly pasted object.
	selectOnly(
		pastedObject
	);
}

void dx3d::Game::handleViewportPicking(
	CameraComponent* camera,
	const TransformGizmo::ViewportArea& viewportArea
)
{
	if (!camera)
		return;

	const bool leftMousePressed =
		m_inputSystem->isKeyPressed(
			KeyCode::MouseLeft
		);

	if (!leftMousePressed)
		return;

	const bool rightMouseDown =
		m_inputSystem->isKeyDown(
			KeyCode::MouseRight
		);

	if (rightMouseDown)
		return;

	// A gizmo click must not trigger object picking.
	if (m_transformGizmo.isUsing() ||
		m_transformGizmo.getHoveredAxis() !=
		TransformGizmo::Axis::None)
	{
		return;
	}

	// Do not pick through editor windows or menus.
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	if (ImGui::IsPopupOpen(
		nullptr,
		ImGuiPopupFlags_AnyPopupId
	))
	{
		return;
	}

	const Vec2 mousePosition =
		m_inputSystem->getMousePosition();

	const PickingViewportArea pickingViewport
	{
		viewportArea.x,
		viewportArea.y,
		viewportArea.width,
		viewportArea.height
	};

	// Finds the closest hit from one ray.
	// planeObjectsOnly determines whether this pass searches
	// planes or all other mesh objects.
	auto findClosestHit =
		[&](
			const PickingRay& ray,
			bool planeObjectsOnly,
			GameObject*& outputObject,
			f32& outputDistance
			)
		{
			outputObject = nullptr;

			outputDistance =
				std::numeric_limits<f32>::max();

			const auto objects =
				m_world->getGameObjects();

			for (auto* object : objects)
			{
				if (!object)
					continue;

				if (object->getComponent<
					CameraComponent>())
				{
					continue;
				}

				const bool isPlaneObject =
					object->getComponent<
					PlaneComponent>() != nullptr;

				if (planeObjectsOnly !=
					isPlaneObject)
				{
					continue;
				}

				const MeshData* meshData =
					getObjectMeshData(object);

				if (!meshData)
					continue;

				f32 hitDistance = 0.0f;

				const bool wasHit =
					intersectRayWithMesh(
						ray,
						*meshData,
						object->getTransform()
						.getAffineWorldMatrix(),
						hitDistance
					);

				if (!wasHit)
					continue;

				if (hitDistance <
					outputDistance)
				{
					outputDistance =
						hitDistance;

					outputObject =
						object;
				}
			}
		};

	struct MouseOffset
	{
		f32 x{};
		f32 y{};
	};

	// The first sample is the exact cursor position.
	// The other samples provide a small selection tolerance.
	constexpr f32 pickingRadius = 6.0f;
	constexpr f32 diagonalRadius = 4.25f;

	const MouseOffset mouseOffsets[]
	{
		{ 0.0f, 0.0f },

		{ -pickingRadius, 0.0f },
		{ pickingRadius, 0.0f },
		{ 0.0f, -pickingRadius },
		{ 0.0f, pickingRadius },

		{ -diagonalRadius, -diagonalRadius },
		{ diagonalRadius, -diagonalRadius },
		{ -diagonalRadius, diagonalRadius },
		{ diagonalRadius, diagonalRadius }
	};

	GameObject* bestMeshObject = nullptr;

	f32 bestOffsetDistanceSquared =
		std::numeric_limits<f32>::max();

	f32 bestMeshDistance =
		std::numeric_limits<f32>::max();

	// Planes are only selected using the exact center ray.
	GameObject* centerPlaneObject = nullptr;

	f32 centerPlaneDistance =
		std::numeric_limits<f32>::max();

	for (size_t offsetIndex = 0;
		offsetIndex < std::size(mouseOffsets);
		++offsetIndex)
	{
		const MouseOffset& offset =
			mouseOffsets[offsetIndex];

		PickingRay pickingRay{};

		if (!createPickingRay(
			mousePosition.x + offset.x,
			mousePosition.y + offset.y,
			pickingViewport,
			*camera,
			pickingRay
		))
		{
			continue;
		}

		GameObject* meshObject = nullptr;
		GameObject* planeObject = nullptr;

		f32 meshDistance =
			std::numeric_limits<f32>::max();

		f32 planeDistance =
			std::numeric_limits<f32>::max();

		// Search cubes, merged objects, and other
		// non-plane mesh objects.
		findClosestHit(
			pickingRay,
			false,
			meshObject,
			meshDistance
		);

		// Search normal PlaneComponent objects separately.
		findClosestHit(
			pickingRay,
			true,
			planeObject,
			planeDistance
		);

		// Remember the plane directly beneath the cursor.
		// Nearby offset rays should not make the plane
		// selection itself oversized.
		if (offsetIndex == 0)
		{
			centerPlaneObject =
				planeObject;

			centerPlaneDistance =
				planeDistance;
		}

		if (!meshObject)
			continue;

		// Do not select a mesh that is hidden behind a plane
		// along the same sample ray.
		const bool meshIsVisible =
			!planeObject ||
			meshDistance <= planeDistance + 0.0001f;

		if (!meshIsVisible)
			continue;

		const f32 offsetDistanceSquared =
			offset.x * offset.x +
			offset.y * offset.y;

		const bool hasBetterMouseOffset =
			offsetDistanceSquared <
			bestOffsetDistanceSquared;

		const bool sameMouseOffset =
			std::fabs(
				offsetDistanceSquared -
				bestOffsetDistanceSquared
			) <= 0.001f;

		const bool hasBetterDepth =
			meshDistance <
			bestMeshDistance;

		if (hasBetterMouseOffset ||
			(sameMouseOffset && hasBetterDepth))
		{
			bestMeshObject =
				meshObject;

			bestOffsetDistanceSquared =
				offsetDistanceSquared;

			bestMeshDistance =
				meshDistance;
		}
	}

	GameObject* closestObject = nullptr;

	if (bestMeshObject)
	{
		// Prefer a nearby visible cube or merged mesh.
		closestObject =
			bestMeshObject;
	}
	else if (centerPlaneObject)
	{
		// The plane remains the fallback when no smaller
		// mesh was found near the cursor.
		closestObject =
			centerPlaneObject;
	}

	const bool controlHeld =
		ImGui::GetIO().KeyCtrl;

	if (closestObject)
	{
		if (controlHeld)
		{
			toggleObjectSelection(
				closestObject
			);
		}
		else
		{
			selectOnly(
				closestObject
			);
		}

		return;
	}

	if (!controlHeld)
	{
		clearSelection();
	}
}

void dx3d::Game::createNewScene()
{
	SceneSerializer::clear(
		*m_world
	);

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter = 0;
	m_planeCounter = 0;

	m_sceneStatusMessage =
		"New scene created";
}

void dx3d::Game::saveScene()
{
	const bool saved =
		SceneSerializer::save(
			*m_world,
			"Scene.dx3dscene"
		);

	if (saved)
	{
		m_sceneStatusMessage =
			"Saved: Scene.dx3dscene";

		DX3DLogInfo(
			"Scene saved."
		);
	}
	else
	{
		m_sceneStatusMessage =
			"Save failed";

		DX3DLogError(
			"Scene save failed."
		);
	}
}

void dx3d::Game::loadScene()
{
	const SceneLoadResult result =
		SceneSerializer::load(
			*m_world,
			"Scene.dx3dscene"
		);

	if (!result.success)
	{
		m_sceneStatusMessage =
			"Load failed or file not found";

		DX3DLogError(
			"Scene load failed."
		);

		return;
	}

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter =
		result.cubeCount;

	m_planeCounter =
		result.planeCount;

	m_sceneStatusMessage =
		"Loaded: Scene.dx3dscene";

	DX3DLogInfo(
		"Scene loaded."
	);
}

void dx3d::Game::onInternalUpdate()
{
	auto currentTime = std::chrono::steady_clock::now();

	std::chrono::duration<f32> delta =
		currentTime - m_previousTime;

	m_previousTime = currentTime;

	const auto deltaTime = delta.count();

	m_inputSystem->update();

	if (!m_display->update())
	{
		return;
	}

	m_inputSystem->setCursorLockArea(
		m_display->getClientAreaInScreenSpace()
	);

	// Begin the ImGui frame.
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	onUpdate(deltaTime);

	if (!m_isRunning)
		return;

	m_world->update(deltaTime);

	// Render the 3D scene first.
	m_worldRenderer->render(
		*m_world,
		m_display->getSwapChain(),
		deltaTime
	);

	// --------------------
// Main editor menu
// --------------------

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem(
				"New Scene"
			))
			{
				createNewScene();
			}

			if (ImGui::MenuItem(
				"Save Scene"
			))
			{
				saveScene();
			}

			if (ImGui::MenuItem(
				"Load Scene"
			))
			{
				loadScene();
			}

			ImGui::Separator();

			ImGui::TextDisabled(
				"%s",
				m_sceneStatusMessage.c_str()
			);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Game Object"))
		{
			if (ImGui::MenuItem("Create Cube"))
			{
				++m_cubeCounter;

				auto* cube =
					m_world->createGameObject<GameObject>();

				if (m_cubeCounter == 1)
				{
					cube->setName("Cube");
				}
				else
				{
					cube->setName(
						"Cube (" +
						std::to_string(m_cubeCounter) +
						")"
					);
				}

				cube->createOrGetComponent<
					CubeComponent>();

				cube->getTransform().setPosition(
					{
						static_cast<f32>(
							m_cubeCounter - 1
						) * 1.5f,
						0.5f,
						0.0f
					}
				);

				cube->getTransform().setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(cube);
			}

			if (ImGui::MenuItem("Create Plane"))
			{
				++m_planeCounter;

				auto* plane =
					m_world->createGameObject<GameObject>();

				if (m_planeCounter == 1)
				{
					plane->setName("Plane");
				}
				else
				{
					plane->setName(
						"Plane (" +
						std::to_string(m_planeCounter) +
						")"
					);
				}

				plane->createOrGetComponent<
					PlaneComponent>();

				plane->getTransform().setPosition(
					{
						static_cast<f32>(
							m_planeCounter - 1
						) * 5.0f,
						0.0f,
						0.0f
					}
				);

				plane->getTransform().setScale(
					{ 4.0f, 1.0f, 4.0f }
				);

				selectOnly(plane);
			}

			ui32 directionalLightCount = 0;

			m_world->getComponents<
				DirectionalLightComponent
			>(
				directionalLightCount
			);

			const bool canCreateDirectionalLight =
				directionalLightCount == 0;

			if (ImGui::MenuItem(
				"Create Directional Light",
				nullptr,
				false,
				canCreateDirectionalLight
			))
			{
				auto* lightObject =
					m_world->createGameObject<GameObject>();

				lightObject->setName(
					"Directional Light"
				);

				auto* lightComponent =
					lightObject->createOrGetComponent<
					DirectionalLightComponent
					>();

				lightComponent->setColor(
					{ 1.0f, 1.0f, 1.0f }
				);

				lightComponent->setIntensity(
					1.0f
				);

				lightComponent->setAmbientStrength(
					0.20f
				);

				lightComponent->setShadowArea(
					30.0f
				);

				lightComponent->setCastShadows(
					true
				);

				auto& lightTransform =
					lightObject->getTransform();

				lightTransform.setPosition(
					{ 0.0f, 3.0f, 0.0f }
				);

				lightTransform.setRotation(
					{ 1.04f, 1.03f, 0.0f }
				);

				lightTransform.setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(
					lightObject
				);
			}

			ImGui::Separator();

			const bool canMergeSelected =
				canMergeSelectedObjects();

			if (ImGui::MenuItem(
				"Merge Selected",
				nullptr,
				false,
				canMergeSelected
			))
			{
				mergeSelectedObjects();
			}

			const bool canDeleteSelected =
				m_selectedObject != nullptr &&
				m_selectedObject->getComponent<
				CameraComponent>() == nullptr;

			if (ImGui::MenuItem(
				"Delete Selected",
				"Delete",
				false,
				canDeleteSelected
			))
			{
				GameObject* objectToDelete =
					m_selectedObject;

				m_world->destroyGameObject(
					objectToDelete
				);

				removeObjectFromSelection(
					objectToDelete
				);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Gizmo"))
		{
			if (ImGui::MenuItem(
				"Translate",
				"W",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Translate
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Translate
				);
			}

			if (ImGui::MenuItem(
				"Rotate",
				"E",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Rotate
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Rotate
				);
			}

			if (ImGui::MenuItem(
				"Scale",
				"R",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Scale
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Scale
				);
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	const ImGuiViewport* viewport =
		ImGui::GetMainViewport();

	const ImVec2 workPosition =
		viewport->WorkPos;

	const ImVec2 workSize =
		viewport->WorkSize;

	const float panelWidth = std::clamp(
		workSize.x * 0.245f,
		300.0f,
		380.0f
	);
	const float elementsHeight = std::clamp(
		workSize.y * 0.38f,
		210.0f,
		340.0f
	);

	CameraComponent* editorCameraComponent = nullptr;

	const auto sceneObjects =
		m_world->getGameObjects();

	for (auto* object : sceneObjects)
	{
		if (!object)
			continue;

		auto* cameraComponent =
			object->getComponent<CameraComponent>();

		if (cameraComponent)
		{
			editorCameraComponent =
				cameraComponent;

			break;
		}
	}

	const TransformGizmo::ViewportArea gizmoViewport
	{
		viewport->Pos.x,
		viewport->Pos.y,
		viewport->Size.x,
		viewport->Size.y
	};

	m_transformGizmo.draw(
		m_selectedObject,
		editorCameraComponent,
		gizmoViewport
	);

	// A simple scene element list keeps selection immediate and unobtrusive.
	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x - panelWidth,
			workPosition.y
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			elementsHeight
		},
		ImGuiCond_Always
	);

	ImGui::Begin("ELEMENTS##Workbench");

	const auto objects = m_world->getGameObjects();

	ImGui::TextDisabled("%zu IN SCENE", objects.size());
	ImGui::Separator();

	for (auto* object : objects)
	{
		if (!object)
			continue;

		const bool isSelected = isObjectSelected(object);
		ImGui::PushID(object);

		if (ImGui::Selectable(
			object->getName().c_str(),
			isSelected
		))
		{
			if (ImGui::GetIO().KeyCtrl)
				toggleObjectSelection(object);
			else
				selectOnly(object);
		}

		ImGui::PopID();
	}

	ImGui::End();

	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x - panelWidth,
			workPosition.y + elementsHeight
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			workSize.y - elementsHeight
		},
		ImGuiCond_Always
	);

	ImGui::Begin("INSPECTOR##Workbench");

	if (!m_selectedObject)
	{
		ImGui::TextDisabled(
			"Select an element to inspect its state."
		);
	}
	else
	{
		ImGui::TextDisabled("ACTIVE OBJECT");
		ImGui::TextColored(
			rgba(114, 211, 174),
			"%s",
			m_selectedObject->getName().c_str()
		);

		const char* gizmoModeName = "Translate";

		switch (m_transformGizmo.getOperation())
		{
		case TransformGizmo::Operation::Translate:
			gizmoModeName = "Translate";
			break;

		case TransformGizmo::Operation::Rotate:
			gizmoModeName = "Rotate";
			break;

		case TransformGizmo::Operation::Scale:
			gizmoModeName = "Scale";
			break;
		}

		ImGui::SameLine();
		ImGui::TextDisabled(
			"  /  %s",
			gizmoModeName
		);
		ImGui::Separator();

		auto& transform =
			m_selectedObject->getTransform();

		auto position = transform.getPosition();
		auto rotation = transform.getRotation();
		auto scale = transform.getScale();

		float positionValues[3] =
		{
			position.x,
			position.y,
			position.z
		};

		float rotationValues[3] =
		{
			rotation.x,
			rotation.y,
			rotation.z
		};

		float scaleValues[3] =
		{
			scale.x,
			scale.y,
			scale.z
		};

		ImGui::TextDisabled("TRANSFORM FLOW  /  LOCAL STATE");
		ImGui::Spacing();

		if (ImGui::DragFloat3(
			"Position",
			positionValues,
			0.05f
		))
		{
			transform.setPosition(
				{
					positionValues[0],
					positionValues[1],
					positionValues[2]
				}
			);
		}

		if (ImGui::DragFloat3(
			"Rotation",
			rotationValues,
			0.01f
		))
		{
			transform.setRotation(
				{
					rotationValues[0],
					rotationValues[1],
					rotationValues[2]
				}
			);
		}

		if (ImGui::DragFloat3(
			"Scale",
			scaleValues,
			0.05f
		))
		{
			transform.setScale(
				{
					scaleValues[0],
					scaleValues[1],
					scaleValues[2]
				}
			);
		}

		if (auto* directionalLight =
			m_selectedObject->getComponent<
			DirectionalLightComponent
			>())
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Directional Light");
			ImGui::Spacing();

			Vec3 lightColor =
				directionalLight->getColor();

			float colorValues[3]
			{
				lightColor.x,
				lightColor.y,
				lightColor.z
			};

			if (ImGui::ColorEdit3(
				"Color",
				colorValues
			))
			{
				directionalLight->setColor(
					{
						colorValues[0],
						colorValues[1],
						colorValues[2]
					}
				);
			}

			float intensity =
				directionalLight->getIntensity();

			if (ImGui::DragFloat(
				"Intensity",
				&intensity,
				0.05f,
				0.0f,
				10.0f
			))
			{
				directionalLight->setIntensity(
					intensity
				);
			}

			float ambientStrength =
				directionalLight->
				getAmbientStrength();

			if (ImGui::SliderFloat(
				"Ambient Strength",
				&ambientStrength,
				0.0f,
				1.0f
			))
			{
				directionalLight->
					setAmbientStrength(
						ambientStrength
					);
			}

			float shadowArea =
				directionalLight->
				getShadowArea();

			if (ImGui::DragFloat(
				"Shadow Area",
				&shadowArea,
				0.5f,
				1.0f,
				200.0f
			))
			{
				directionalLight->
					setShadowArea(
						shadowArea
					);
			}

			bool castShadows =
				directionalLight->
				getCastShadows();

			if (ImGui::Checkbox(
				"Cast Shadows",
				&castShadows
			))
			{
				directionalLight->
					setCastShadows(
						castShadows
					);
			}
		}

	}

	ImGui::End();

	handleViewportPicking(
		editorCameraComponent,
		gizmoViewport
	);

	// --------------------------------------------------
// Transform gizmo mode shortcuts
// --------------------------------------------------

	const bool rightMouseDown =
		m_inputSystem->isKeyDown(
			KeyCode::MouseRight
		);

	const bool editingImGuiValue =
		ImGui::IsAnyItemActive();

	const bool typingInImGui =
		ImGui::GetIO().WantTextInput;

	const bool popupIsOpen =
		ImGui::IsPopupOpen(
			nullptr,
			ImGuiPopupFlags_AnyPopupId
		);

	const bool allowGizmoShortcuts =
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen;

	if (allowGizmoShortcuts)
	{
		if (m_inputSystem->isKeyPressed(KeyCode::W))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Translate
			);
		}

		if (m_inputSystem->isKeyPressed(KeyCode::E))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Rotate
			);
		}

		if (m_inputSystem->isKeyPressed(KeyCode::R))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Scale
			);
		}
	}

	// --------------------------------------------------
// Object clipboard shortcuts
// Ctrl + C = copy active object
// Ctrl + V = paste copied object
// --------------------------------------------------

	const bool controlHeld =
		ImGui::GetIO().KeyCtrl;

	const bool allowClipboardShortcuts =
		controlHeld &&
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen;

	if (allowClipboardShortcuts)
	{
		if (m_inputSystem->isKeyPressed(
			KeyCode::C
		))
		{
			copySelectedObject();
		}
		else if (m_inputSystem->isKeyPressed(
			KeyCode::V
		))
		{
			pasteCopiedObject();
		}
	}

	// Delete the selected scene object using the keyboard.
	const bool deletePressed =
		m_inputSystem->isKeyPressed(KeyCode::Delete);

	const bool editingInspectorValue =
		ImGui::IsAnyItemActive();

	const bool typingText =
		ImGui::GetIO().WantTextInput;

	if (deletePressed &&
		!m_transformGizmo.isUsing() &&
		!editingInspectorValue &&
		!typingText &&
		m_selectedObject &&
		m_selectedObject->getComponent<CameraComponent>() == nullptr)
	{
		GameObject* objectToDelete =
			m_selectedObject;

		m_world->destroyGameObject(
			objectToDelete
		);

		removeObjectFromSelection(
			objectToDelete
		);
	}

	// Render the UI over the 3D scene.
	ImGui::Render();

	m_graphicsDevice->bindBackBuffer(
		m_display->getSwapChain()
	);

	ImGui_ImplDX11_RenderDrawData(
		ImGui::GetDrawData()
	);

	// Present only after both the scene and UI are rendered.
	m_display->getSwapChain().present();
}
