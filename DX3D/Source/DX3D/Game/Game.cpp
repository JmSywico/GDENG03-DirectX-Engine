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
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Physics/PhysicsWorld.h>

#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iterator>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <functional>

#include <imgui.h>
#include <imgui_internal.h>
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

		style.WindowPadding = ImVec2(10.0f, 8.0f);
		style.FramePadding = ImVec2(7.0f, 4.0f);
		style.CellPadding = ImVec2(7.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 5.0f);
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
		colors[ImGuiCol_Text] = ImVec4(0.88f, 0.87f, 0.84f, 1.0f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.49f, 0.46f, 1.0f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.075f, 0.082f, 1.0f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.055f, 0.060f, 1.0f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.085f, 0.083f, 0.086f, 0.98f);
		colors[ImGuiCol_Border] = ImVec4(0.24f, 0.235f, 0.220f, 1.0f);
		colors[ImGuiCol_BorderShadow] = rgba(0, 0, 0, 0);
		colors[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.132f, 0.130f, 1.0f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.205f, 0.195f, 0.180f, 1.0f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.330f, 0.260f, 0.140f, 1.0f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.070f, 0.070f, 0.075f, 1.0f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.095f, 0.092f, 0.090f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.070f, 0.070f, 0.075f, 1.0f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.075f, 0.075f, 0.080f, 1.0f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.055f, 0.060f, 1.0f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.215f, 0.205f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.295f, 0.265f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.35f, 0.29f, 1.0f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.92f, 0.67f, 0.30f, 1.0f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.74f, 0.55f, 0.29f, 1.0f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.94f, 0.70f, 0.32f, 1.0f);
		colors[ImGuiCol_Button] = ImVec4(0.145f, 0.142f, 0.140f, 1.0f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.240f, 0.225f, 0.200f, 1.0f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.380f, 0.300f, 0.155f, 1.0f);
		colors[ImGuiCol_Header] = ImVec4(0.190f, 0.180f, 0.160f, 1.0f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.300f, 0.260f, 0.185f, 1.0f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.420f, 0.320f, 0.160f, 1.0f);
		colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.235f, 0.220f, 1.0f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.62f, 0.46f, 0.24f, 1.0f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.92f, 0.67f, 0.30f, 1.0f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.42f, 0.32f, 0.16f, 0.45f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.74f, 0.55f, 0.29f, 0.75f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.94f, 0.70f, 0.32f, 1.0f);
		colors[ImGuiCol_Tab] = ImVec4(0.110f, 0.108f, 0.105f, 1.0f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.300f, 0.260f, 0.185f, 1.0f);
		colors[ImGuiCol_TabActive] = ImVec4(0.195f, 0.175f, 0.135f, 1.0f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.92f, 0.67f, 0.30f, 0.40f);
		colors[ImGuiCol_TableHeaderBg] = rgba(26, 34, 35);
		colors[ImGuiCol_TableBorderStrong] = rgba(55, 66, 66);
		colors[ImGuiCol_TableBorderLight] = rgba(40, 49, 49);
		colors[ImGuiCol_TableRowBgAlt] = rgba(255, 255, 255, 7);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.50f, 0.36f, 0.16f, 0.55f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.92f, 0.67f, 0.30f, 0.75f);
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
	m_physicsWorld = std::make_unique<PhysicsWorld>();

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
	refreshAssetLens();

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

	pushUndoSnapshot();

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
	mergedObject->createOrGetComponent<MaterialComponent>();

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
	copiedData.parentEntityId =
		m_selectedObject->getParent()
		? m_selectedObject->getParent()->getEntityId()
		: 0;

	if (auto* material = m_selectedObject->getComponent<MaterialComponent>())
	{
		copiedData.hasMaterial = true;
		copiedData.materialMode = material->getMode();
		copiedData.materialAlbedo = material->getAlbedo();
		copiedData.materialEmissive = material->getEmissive();
		copiedData.materialEmissionStrength = material->getEmissionStrength();
	}

	if (auto* rigidBody = m_selectedObject->getComponent<RigidBodyComponent>())
	{
		copiedData.hasRigidBody = true;
		copiedData.rigidBodyType = rigidBody->getBodyType();
		copiedData.colliderShape = rigidBody->getColliderShape();
		copiedData.colliderHalfExtents = rigidBody->getHalfExtents();
		copiedData.colliderRadius = rigidBody->getRadius();
		copiedData.rigidBodyMass = rigidBody->getMass();
		copiedData.rigidBodyRestitution = rigidBody->getRestitution();
		copiedData.gravityEnabled = rigidBody->isGravityEnabled();
	}

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

	pushUndoSnapshot();

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

	if (m_objectClipboard.parentEntityId != 0)
	{
		m_world->setParent(
			pastedObject,
			m_world->findGameObject(m_objectClipboard.parentEntityId)
		);
	}

	if (m_objectClipboard.hasMaterial)
	{
		auto* material = pastedObject->createOrGetComponent<MaterialComponent>();
		material->setMode(m_objectClipboard.materialMode);
		material->setAlbedo(m_objectClipboard.materialAlbedo);
		material->setEmissive(m_objectClipboard.materialEmissive);
		material->setEmissionStrength(m_objectClipboard.materialEmissionStrength);
	}

	if (m_objectClipboard.hasRigidBody)
	{
		auto* rigidBody = pastedObject->createOrGetComponent<RigidBodyComponent>();
		rigidBody->setBodyType(m_objectClipboard.rigidBodyType);
		rigidBody->setColliderShape(m_objectClipboard.colliderShape);
		rigidBody->setHalfExtents(m_objectClipboard.colliderHalfExtents);
		rigidBody->setRadius(m_objectClipboard.colliderRadius);
		rigidBody->setMass(m_objectClipboard.rigidBodyMass);
		rigidBody->setRestitution(m_objectClipboard.rigidBodyRestitution);
		rigidBody->setGravityEnabled(m_objectClipboard.gravityEnabled);
	}

	// Automatically select the newly pasted object.
	selectOnly(
		pastedObject
	);
}

void dx3d::Game::duplicateSelectedObject()
{
	if (!canCopySelectedObject())
		return;

	copySelectedObject();
	pasteCopiedObject();
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
	pushUndoSnapshot();

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
	m_sceneDirty = true;
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
		m_sceneDirty = false;
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
	pushUndoSnapshot();

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
	m_sceneDirty = false;

	DX3DLogInfo(
		"Scene loaded."
	);
}

void dx3d::Game::pushUndoSnapshot()
{
	pushUndoSnapshot(SceneSerializer::serialize(*m_world));
}

void dx3d::Game::pushUndoSnapshot(
	const std::string& snapshot
)
{
	if (snapshot.empty())
		return;

	if (!m_undoSnapshots.empty() &&
		m_undoSnapshots.back() == snapshot)
	{
		return;
	}

	constexpr size_t maximumHistory = 64;
	m_undoSnapshots.push_back(snapshot);

	if (m_undoSnapshots.size() > maximumHistory)
		m_undoSnapshots.pop_front();

	m_redoSnapshots.clear();
	m_sceneDirty = true;
}

void dx3d::Game::undo()
{
	if (m_editorMode != EditorMode::Editing ||
		m_undoSnapshots.empty())
	{
		return;
	}

	const std::string current =
		SceneSerializer::serialize(*m_world);
	const std::string target = m_undoSnapshots.back();
	m_undoSnapshots.pop_back();

	if (!current.empty())
		m_redoSnapshots.push_back(current);

	if (SceneSerializer::deserialize(*m_world, target).success)
	{
		clearSelection();
		m_sceneDirty = true;
		m_sceneStatusMessage = "Undo";
	}
}

void dx3d::Game::redo()
{
	if (m_editorMode != EditorMode::Editing ||
		m_redoSnapshots.empty())
	{
		return;
	}

	const std::string current =
		SceneSerializer::serialize(*m_world);
	const std::string target = m_redoSnapshots.back();
	m_redoSnapshots.pop_back();

	if (!current.empty())
		m_undoSnapshots.push_back(current);

	if (SceneSerializer::deserialize(*m_world, target).success)
	{
		clearSelection();
		m_sceneDirty = true;
		m_sceneStatusMessage = "Redo";
	}
}

void dx3d::Game::startPlayMode()
{
	if (m_editorMode != EditorMode::Editing)
		return;

	m_editorSceneSnapshot =
		SceneSerializer::serialize(*m_world);

	if (m_editorSceneSnapshot.empty())
	{
		m_sceneStatusMessage = "Could not enter Play Mode";
		return;
	}

	// Reconstruct authorable objects so runtime mutations are isolated from
	// the editor scene. The editor camera is intentionally preserved.
	SceneSerializer::deserialize(*m_world, m_editorSceneSnapshot);
	m_physicsWorld->reset(*m_world);
	clearSelection();
	m_fixedStepAccumulator = 0.0f;
	m_editorMode = EditorMode::Playing;
	m_sceneStatusMessage = "Play Mode";
}

void dx3d::Game::stopPlayMode()
{
	if (m_editorMode == EditorMode::Editing)
		return;

	if (!m_editorSceneSnapshot.empty())
	{
		SceneSerializer::deserialize(
			*m_world,
			m_editorSceneSnapshot
		);
	}

	clearSelection();
	m_editorSceneSnapshot.clear();
	m_fixedStepAccumulator = 0.0f;
	m_singleStepRequested = false;
	m_editorMode = EditorMode::Editing;
	m_sceneStatusMessage = "Edit Mode";
}

void dx3d::Game::refreshAssetLens()
{
	m_assetPaths.clear();

	const std::filesystem::path assetRoot =
		std::filesystem::path("DX3D") / "Assets";

	std::error_code error{};
	if (std::filesystem::exists(assetRoot, error))
	{
		for (std::filesystem::recursive_directory_iterator iterator(
			assetRoot,
			std::filesystem::directory_options::skip_permission_denied,
			error
		); iterator != std::filesystem::recursive_directory_iterator();
			iterator.increment(error))
		{
			if (error)
			{
				error.clear();
				continue;
			}

			if (iterator->is_regular_file(error))
			{
				m_assetPaths.push_back(
					iterator->path().generic_string()
				);
			}
		}
	}

	for (const auto& entry : std::filesystem::directory_iterator(
		std::filesystem::current_path(),
		error
	))
	{
		if (entry.is_regular_file(error) &&
			entry.path().extension() == ".dx3dscene")
		{
			m_assetPaths.push_back(entry.path().filename().generic_string());
		}
	}

	std::sort(m_assetPaths.begin(), m_assetPaths.end());
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

	m_frameTimes[m_frameTimeCursor] = deltaTime * 1000.0f;
	m_frameTimeCursor =
		(m_frameTimeCursor + 1) % m_frameTimes.size();

	onUpdate(deltaTime);

	if (!m_isRunning)
		return;

	constexpr f32 fixedTimeStep = 1.0f / 60.0f;

	if (m_editorMode == EditorMode::Editing)
	{
		m_world->update(deltaTime);
	}
	else if (m_editorMode == EditorMode::Playing)
	{
		m_fixedStepAccumulator += std::min(deltaTime, 0.25f);
		ui32 steps = 0;

		while (m_fixedStepAccumulator >= fixedTimeStep && steps < 8)
		{
			m_world->update(fixedTimeStep);
			m_physicsWorld->step(*m_world, fixedTimeStep);
			m_fixedStepAccumulator -= fixedTimeStep;
			++steps;
		}
	}
	else if (m_singleStepRequested)
	{
		m_world->update(fixedTimeStep);
		m_physicsWorld->step(*m_world, fixedTimeStep);
		m_singleStepRequested = false;
	}

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
		ImGui::TextColored(rgba(235, 171, 77), "enignE / DX11");
		ImGui::Separator();
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
				pushUndoSnapshot();
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
				cube->createOrGetComponent<MaterialComponent>();

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
				pushUndoSnapshot();
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
				plane->createOrGetComponent<MaterialComponent>();

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
				pushUndoSnapshot();
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

			if (ImGui::MenuItem(
				"Duplicate Selected",
				"Ctrl+D",
				false,
				canCopySelectedObject()
			))
			{
				duplicateSelectedObject();
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
				pushUndoSnapshot();
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

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem(
				"Undo", "Ctrl+Z", false,
				m_editorMode == EditorMode::Editing && !m_undoSnapshots.empty()))
			{
				undo();
			}

			if (ImGui::MenuItem(
				"Redo", "Ctrl+Y", false,
				m_editorMode == EditorMode::Editing && !m_redoSnapshots.empty()))
			{
				redo();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Stats", nullptr, &m_showStats);
			ImGui::MenuItem("Asset Lens", nullptr, &m_showAssetLens);
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

	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(workSize, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	ImGui::Begin(
		"##EngineWorkbenchDockspaceHost",
		nullptr,
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoSavedSettings
	);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspaceId =
		ImGui::GetID("EngineWorkbenchDockspaceV4");

	if (!m_defaultDockLayoutBuilt)
	{
		if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
		{
			ImGui::DockBuilderRemoveNode(dockspaceId);
			ImGui::DockBuilderAddNode(
				dockspaceId,
				ImGuiDockNodeFlags_DockSpace
			);
			ImGui::DockBuilderSetNodeSize(dockspaceId, workSize);

			ImGuiID center = dockspaceId;
			const ImGuiID signal = ImGui::DockBuilderSplitNode(
				center, ImGuiDir_Right, 0.28f, nullptr, &center);
			ImGuiID registry = ImGui::DockBuilderSplitNode(
				center, ImGuiDir_Down, 0.24f, nullptr, &center);
			ImGuiID elements = registry;
			const ImGuiID assets = ImGui::DockBuilderSplitNode(
				elements, ImGuiDir_Right, 0.62f, nullptr, &elements);

			ImGui::DockBuilderDockWindow("ELEMENTS##Workbench", elements);
			ImGui::DockBuilderDockWindow("ASSET LENS##Workbench", assets);
			ImGui::DockBuilderDockWindow("STATS##Workbench", signal);
			ImGui::DockBuilderDockWindow("INSPECTOR##Workbench", signal);
			ImGui::DockBuilderFinish(dockspaceId);
		}
		m_defaultDockLayoutBuilt = true;
	}

	ImGui::DockSpace(
		dockspaceId,
		{ 0.0f, 0.0f },
		ImGuiDockNodeFlags_PassthruCentralNode
	);
	ImGui::End();

	// Compact engine-state toolbar. It deliberately exposes simulation state
	// without turning the editor into a ribbon-heavy clone.
	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		{ std::max(260.0f, workSize.x - 390.0f), 42.0f },
		ImGuiCond_FirstUseEver
	);
	ImGui::Begin(
		"FRAME CONTROL##Workbench",
		nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings
	);

	if (m_editorMode == EditorMode::Editing)
	{
		if (ImGui::Button("PLAY  >"))
			startPlayMode();
	}
	else
	{
		if (ImGui::Button("STOP  []"))
			stopPlayMode();

		ImGui::SameLine();
		const bool paused = m_editorMode == EditorMode::Paused;
		if (ImGui::Button(paused ? "RESUME  >" : "PAUSE  ||"))
		{
			m_editorMode = paused
				? EditorMode::Playing
				: EditorMode::Paused;
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!paused);
		if (ImGui::Button("STEP  >|"))
			m_singleStepRequested = true;
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	const char* modeText = m_editorMode == EditorMode::Editing
		? "EDIT"
		: (m_editorMode == EditorMode::Playing ? "PLAY" : "PAUSED");
	ImGui::TextColored(
		m_editorMode == EditorMode::Editing
			? rgba(150, 160, 158)
			: rgba(235, 171, 77),
		"%s  /  %s%s",
		modeText,
		m_sceneStatusMessage.c_str(),
		m_sceneDirty ? "  *" : ""
	);
	ImGui::End();

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
		workPosition.x,
		workPosition.y + 42.0f,
		workSize.x * 0.72f,
		std::max(1.0f, workSize.y * 0.76f - 42.0f)
	};

	if (m_editorMode == EditorMode::Editing &&
		m_selectedObject &&
		m_inputSystem->isKeyPressed(KeyCode::MouseLeft) &&
		m_transformGizmo.getHoveredAxis() != TransformGizmo::Axis::None)
	{
		pushUndoSnapshot();
	}

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
		ImGuiCond_FirstUseEver
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			elementsHeight
		},
		ImGuiCond_FirstUseEver
	);

	ImGui::Begin("ELEMENTS##Workbench");

	const auto objects = m_world->getGameObjects();

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##RegistryFilter",
		"Filter registry by name...",
		m_registryFilter,
		sizeof(m_registryFilter)
	);
	ImGui::TextDisabled("REGISTRY LENS  /  %zu ENTITIES", objects.size());

	auto normalizedContains = [](const std::string& value, const char* filter)
	{
		if (!filter || filter[0] == '\0') return true;
		std::string haystack = value;
		std::string needle = filter;
		std::transform(haystack.begin(), haystack.end(), haystack.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(needle.begin(), needle.end(), needle.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return haystack.find(needle) != std::string::npos;
	};

	std::function<bool(GameObject*)> subtreeMatches;
	subtreeMatches = [&](GameObject* object)
	{
		if (!object) return false;
		if (normalizedContains(object->getName(), m_registryFilter)) return true;
		for (auto* child : object->getChildren())
			if (subtreeMatches(child)) return true;
		return false;
	};

	GameObject* requestedChild = nullptr;
	GameObject* requestedParent = nullptr;
	GameObject* requestedDelete = nullptr;
	GameObject* requestedDuplicate = nullptr;
	bool reparentRequested = false;

	if (ImGui::BeginTable(
		"RegistryLens", 4,
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerH |
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("ENTITY", ImGuiTableColumnFlags_WidthStretch, 0.44f);
		ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("PARENT", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("COMPOSITION", ImGuiTableColumnFlags_WidthStretch, 0.30f);
		ImGui::TableHeadersRow();

		std::function<void(GameObject*)> drawEntity;
		drawEntity = [&](GameObject* object)
		{
			if (!object || !subtreeMatches(object)) return;

			const bool hasVisibleChildren = std::any_of(
				object->getChildren().begin(), object->getChildren().end(),
				[&](GameObject* child) { return subtreeMatches(child); });
			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_SpanAllColumns |
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick;
			if (!hasVisibleChildren)
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (isObjectSelected(object))
				flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::PushID(object);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const bool open = ImGui::TreeNodeEx(
				"##Entity", flags, "%s", object->getName().c_str());

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				if (ImGui::GetIO().KeyCtrl) toggleObjectSelection(object);
				else selectOnly(object);
			}

			if (m_editorMode == EditorMode::Editing && ImGui::BeginDragDropSource())
			{
				const ui64 entityId = object->getEntityId();
				ImGui::SetDragDropPayload("DX3D_ENTITY_ID", &entityId, sizeof(entityId));
				ImGui::Text("Parent %s", object->getName().c_str());
				ImGui::EndDragDropSource();
			}

			if (m_editorMode == EditorMode::Editing && ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("DX3D_ENTITY_ID"))
				{
					const ui64 draggedId = *static_cast<const ui64*>(payload->Data);
					requestedChild = m_world->findGameObject(draggedId);
					requestedParent = object;
					reparentRequested = true;
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::BeginPopupContextItem("EntityActions"))
			{
				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false,
					m_editorMode == EditorMode::Editing &&
					object->getComponent<CameraComponent>() == nullptr))
				{
					requestedDuplicate = object;
				}
				if (ImGui::MenuItem("Unparent", nullptr, false,
					m_editorMode == EditorMode::Editing && object->getParent() != nullptr))
				{
					requestedChild = object;
					requestedParent = nullptr;
					reparentRequested = true;
				}
				if (ImGui::MenuItem("Delete", "Delete", false,
					m_editorMode == EditorMode::Editing &&
					object->getComponent<CameraComponent>() == nullptr))
				{
					requestedDelete = object;
				}
				ImGui::EndPopup();
			}

			ImGui::TableNextColumn();
			ImGui::Text("%llu", static_cast<unsigned long long>(object->getEntityId()));
			ImGui::TableNextColumn();
			if (object->getParent())
				ImGui::Text("%llu", static_cast<unsigned long long>(object->getParent()->getEntityId()));
			else
				ImGui::TextDisabled("ROOT");

			ImGui::TableNextColumn();
			std::string composition = "Transform";
			if (object->getComponent<CameraComponent>()) composition += " + Camera";
			if (object->getComponent<CubeComponent>()) composition += " + Cube";
			if (object->getComponent<PlaneComponent>()) composition += " + Plane";
			if (object->getComponent<CombinedMeshComponent>()) composition += " + Mesh";
			if (object->getComponent<DirectionalLightComponent>()) composition += " + Light";
			if (object->getComponent<MaterialComponent>()) composition += " + Material";
			if (object->getComponent<RigidBodyComponent>()) composition += " + Physics";
			ImGui::TextDisabled("%s", composition.c_str());

			if (hasVisibleChildren && open)
			{
				for (auto* child : object->getChildren()) drawEntity(child);
				ImGui::TreePop();
			}
			ImGui::PopID();
		};

		for (auto* object : objects)
			if (object && object->getParent() == nullptr) drawEntity(object);

		ImGui::EndTable();
	}

	ImGui::TextDisabled("DROP HERE TO MOVE ENTITY TO ROOT");
	if (m_editorMode == EditorMode::Editing && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload("DX3D_ENTITY_ID"))
		{
			const ui64 draggedId = *static_cast<const ui64*>(payload->Data);
			requestedChild = m_world->findGameObject(draggedId);
			requestedParent = nullptr;
			reparentRequested = true;
		}
		ImGui::EndDragDropTarget();
	}

	if (reparentRequested && requestedChild)
	{
		pushUndoSnapshot();
		if (!m_world->setParent(requestedChild, requestedParent))
			m_sceneStatusMessage = "Hierarchy rejected: cycle or invalid link";
		else
			m_sceneStatusMessage = "Hierarchy updated";
	}

	if (requestedDuplicate)
	{
		selectOnly(requestedDuplicate);
		duplicateSelectedObject();
	}

	if (requestedDelete)
	{
		pushUndoSnapshot();
		m_world->destroyGameObject(requestedDelete);
		removeObjectFromSelection(requestedDelete);
	}

	ImGui::End();

	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x - panelWidth,
			workPosition.y + elementsHeight
		},
		ImGuiCond_FirstUseEver
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			workSize.y - elementsHeight
		},
		ImGuiCond_FirstUseEver
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
		const std::string inspectorSnapshot =
			m_editorMode == EditorMode::Editing
			? SceneSerializer::serialize(*m_world)
			: std::string{};

		ImGui::TextDisabled("ACTIVE OBJECT");
		ImGui::TextColored(
			rgba(235, 171, 77),
			"%s",
			m_selectedObject->getName().c_str()
		);
		ImGui::TextDisabled(
			"ENTITY %llu  /  PARENT %s",
			static_cast<unsigned long long>(m_selectedObject->getEntityId()),
			m_selectedObject->getParent()
				? m_selectedObject->getParent()->getName().c_str()
				: "ROOT"
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

		char objectName[256]{};
		std::snprintf(
			objectName,
			sizeof(objectName),
			"%s",
			m_selectedObject->getName().c_str()
		);
		ImGui::BeginDisabled(m_editorMode != EditorMode::Editing);
		const bool nameChanged = ImGui::InputText(
			"Name",
			objectName,
			sizeof(objectName)
		);
		if (ImGui::IsItemActivated())
			pushUndoSnapshot(inspectorSnapshot);
		if (nameChanged)
			m_selectedObject->setName(objectName);

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

		const bool positionChanged = ImGui::DragFloat3(
			"Position",
			positionValues,
			0.05f
		);
		if (ImGui::IsItemActivated())
			pushUndoSnapshot(inspectorSnapshot);
		if (positionChanged)
		{
			transform.setPosition(
				{
					positionValues[0],
					positionValues[1],
					positionValues[2]
				}
			);
		}

		const bool rotationChanged = ImGui::DragFloat3(
			"Rotation",
			rotationValues,
			0.01f
		);
		if (ImGui::IsItemActivated())
			pushUndoSnapshot(inspectorSnapshot);
		if (rotationChanged)
		{
			transform.setRotation(
				{
					rotationValues[0],
					rotationValues[1],
					rotationValues[2]
				}
			);
		}

		const bool scaleChanged = ImGui::DragFloat3(
			"Scale",
			scaleValues,
			0.05f
		);
		if (ImGui::IsItemActivated())
			pushUndoSnapshot(inspectorSnapshot);
		if (scaleChanged)
		{
			transform.setScale(
				{
					scaleValues[0],
					scaleValues[1],
					scaleValues[2]
				}
			);
		}

		if (auto* camera = m_selectedObject->getComponent<CameraComponent>())
		{
			ImGui::Spacing();
			ImGui::SeparatorText("CAMERA WORKBENCH  /  ENGINE STATE");
			float fieldOfView = camera->getFieldOfView();
			float nearPlane = camera->getNearPlane();
			float farPlane = camera->getFarPlane();
			if (ImGui::DragFloat("Field of View", &fieldOfView, 0.01f, 0.1f, 3.0f))
				camera->setFieldOfView(fieldOfView);
			if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.001f, farPlane - 0.01f))
				camera->setNearPlane(nearPlane);
			if (ImGui::DragFloat("Far Plane", &farPlane, 0.5f, nearPlane + 0.01f, 10000.0f))
				camera->setFarPlane(farPlane);
			const Rect cameraViewport = camera->getViewportSize();
			ImGui::TextDisabled(
				"PROJECTION  /  %d x %d",
				cameraViewport.width,
				cameraViewport.height
			);
		}

		const bool isRenderable =
			m_selectedObject->getComponent<CubeComponent>() ||
			m_selectedObject->getComponent<PlaneComponent>() ||
			m_selectedObject->getComponent<CombinedMeshComponent>();

		if (isRenderable)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("MATERIAL MODE MATRIX");
			auto* material = m_selectedObject->getComponent<MaterialComponent>();

			if (!material && ImGui::Button("ADD MATERIAL COMPONENT", { -1.0f, 0.0f }))
			{
				pushUndoSnapshot(inspectorSnapshot);
				material = m_selectedObject->createOrGetComponent<MaterialComponent>();
			}

			if (material)
			{
				const char* modeNames[] =
				{
					"Lit / Tint", "Rainbow Debug", "Flat Red",
					"Flat Green", "Flat Blue"
				};
				int mode = static_cast<int>(material->getMode());
				const bool modeChanged = ImGui::Combo(
					"Mode", &mode, modeNames, IM_ARRAYSIZE(modeNames));
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (modeChanged) material->setMode(static_cast<MaterialMode>(mode));

				Vec4 albedo = material->getAlbedo();
				float albedoValues[4]{ albedo.x, albedo.y, albedo.z, albedo.w };
				const bool albedoChanged = ImGui::ColorEdit4("Albedo", albedoValues);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (albedoChanged)
					material->setAlbedo({ albedoValues[0], albedoValues[1], albedoValues[2], albedoValues[3] });

				Vec3 emissive = material->getEmissive();
				float emissiveValues[3]{ emissive.x, emissive.y, emissive.z };
				const bool emissiveChanged = ImGui::ColorEdit3("Emissive", emissiveValues);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (emissiveChanged)
					material->setEmissive({ emissiveValues[0], emissiveValues[1], emissiveValues[2] });

				float emissionStrength = material->getEmissionStrength();
				const bool emissionChanged = ImGui::DragFloat(
					"Emission", &emissionStrength, 0.05f, 0.0f, 100.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (emissionChanged) material->setEmissionStrength(emissionStrength);
			}

			ImGui::Spacing();
			ImGui::SeparatorText("PHYSICS BODY / COLLIDER");
			auto* rigidBody = m_selectedObject->getComponent<RigidBodyComponent>();
			if (!rigidBody && ImGui::Button("ADD RIGID BODY", { -1.0f, 0.0f }))
			{
				pushUndoSnapshot(inspectorSnapshot);
				rigidBody = m_selectedObject->createOrGetComponent<RigidBodyComponent>();
				if (m_selectedObject->getComponent<PlaneComponent>())
					rigidBody->setBodyType(RigidBodyType::Static);
			}

			if (rigidBody)
			{
				const char* bodyNames[]{ "Static", "Dynamic", "Kinematic" };
				int bodyType = static_cast<int>(rigidBody->getBodyType());
				const bool bodyChanged = ImGui::Combo("Body Type", &bodyType, bodyNames, IM_ARRAYSIZE(bodyNames));
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (bodyChanged) rigidBody->setBodyType(static_cast<RigidBodyType>(bodyType));

				const char* shapeNames[]{ "Box", "Sphere" };
				int shape = static_cast<int>(rigidBody->getColliderShape());
				const bool shapeChanged = ImGui::Combo("Collider", &shape, shapeNames, IM_ARRAYSIZE(shapeNames));
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (shapeChanged) rigidBody->setColliderShape(static_cast<ColliderShape>(shape));

				if (rigidBody->getColliderShape() == ColliderShape::Box)
				{
					Vec3 extent = rigidBody->getHalfExtents();
					float values[3]{ extent.x, extent.y, extent.z };
					const bool changed = ImGui::DragFloat3("Half Extents", values, 0.02f, 0.001f, 1000.0f);
					if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
					if (changed) rigidBody->setHalfExtents({ values[0], values[1], values[2] });
				}
				else
				{
					float radius = rigidBody->getRadius();
					const bool changed = ImGui::DragFloat("Radius", &radius, 0.02f, 0.001f, 1000.0f);
					if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
					if (changed) rigidBody->setRadius(radius);
				}

				float mass = rigidBody->getMass();
				const bool massChanged = ImGui::DragFloat("Mass", &mass, 0.05f, 0.001f, 10000.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (massChanged) rigidBody->setMass(mass);
				float restitution = rigidBody->getRestitution();
				const bool restitutionChanged = ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (restitutionChanged) rigidBody->setRestitution(restitution);
				bool gravity = rigidBody->isGravityEnabled();
				const bool gravityChanged = ImGui::Checkbox("Gravity", &gravity);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (gravityChanged) rigidBody->setGravityEnabled(gravity);
			}
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

			const bool colorChanged = ImGui::ColorEdit3(
				"Color",
				colorValues
			);
			if (ImGui::IsItemActivated())
				pushUndoSnapshot(inspectorSnapshot);
			if (colorChanged)
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

			const bool intensityChanged = ImGui::DragFloat(
				"Intensity",
				&intensity,
				0.05f,
				0.0f,
				10.0f
			);
			if (ImGui::IsItemActivated())
				pushUndoSnapshot(inspectorSnapshot);
			if (intensityChanged)
			{
				directionalLight->setIntensity(
					intensity
				);
			}

			float ambientStrength =
				directionalLight->
				getAmbientStrength();

			const bool ambientChanged = ImGui::SliderFloat(
				"Ambient Strength",
				&ambientStrength,
				0.0f,
				1.0f
			);
			if (ImGui::IsItemActivated())
				pushUndoSnapshot(inspectorSnapshot);
			if (ambientChanged)
			{
				directionalLight->
					setAmbientStrength(
						ambientStrength
					);
			}

			float shadowArea =
				directionalLight->
				getShadowArea();

			const bool shadowAreaChanged = ImGui::DragFloat(
				"Shadow Area",
				&shadowArea,
				0.5f,
				1.0f,
				200.0f
			);
			if (ImGui::IsItemActivated())
				pushUndoSnapshot(inspectorSnapshot);
			if (shadowAreaChanged)
			{
				directionalLight->
					setShadowArea(
						shadowArea
					);
			}

			bool castShadows =
				directionalLight->
				getCastShadows();

			const bool castShadowsChanged = ImGui::Checkbox(
				"Cast Shadows",
				&castShadows
			);
			if (ImGui::IsItemActivated())
				pushUndoSnapshot(inspectorSnapshot);
			if (castShadowsChanged)
			{
				directionalLight->
					setCastShadows(
						castShadows
					);
			}
		}

		ImGui::EndDisabled();

	}

	ImGui::End();

	if (m_showStats)
	{
		const float statsWidth = std::clamp(workSize.x * 0.29f, 330.0f, 470.0f);
		const float statsHeight = std::clamp(workSize.y * 0.30f, 210.0f, 300.0f);
		ImGui::SetNextWindowPos(
			{ workPosition.x, workPosition.y + workSize.y - statsHeight },
			ImGuiCond_FirstUseEver
		);
		ImGui::SetNextWindowSize(
			{ statsWidth, statsHeight },
			ImGuiCond_FirstUseEver
		);

		if (ImGui::Begin("STATS##Workbench", &m_showStats))
		{
			f32 averageFrameMs = 0.0f;
			for (const f32 frameMs : m_frameTimes)
				averageFrameMs += frameMs;
			averageFrameMs /= static_cast<f32>(m_frameTimes.size());

			ui32 cubeCount = 0;
			ui32 planeCount = 0;
			ui32 lightCount = 0;
			m_world->getComponents<CubeComponent>(cubeCount);
			m_world->getComponents<PlaneComponent>(planeCount);
			m_world->getComponents<DirectionalLightComponent>(lightCount);

			if (ImGui::BeginTable(
				"FrameSummary", 4,
				ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn(); ImGui::TextDisabled("FRAME");
				ImGui::Text("%.2f ms", averageFrameMs);
				ImGui::TableNextColumn(); ImGui::TextDisabled("FPS");
				ImGui::Text("%.0f", averageFrameMs > 0.0f ? 1000.0f / averageFrameMs : 0.0f);
				ImGui::TableNextColumn(); ImGui::TextDisabled("REGISTRY");
				ImGui::Text("%zu", objects.size());
				ImGui::TableNextColumn(); ImGui::TextDisabled("DRAWS");
				ImGui::Text("%u", cubeCount + planeCount);
				ImGui::EndTable();
			}

			const auto& physicsStats = m_physicsWorld->getStats();
			ImGui::TextDisabled(
				"PHYSICS  /  %.3f ms  /  %u BODIES  /  %u ACTIVE  /  %u CONTACTS",
				physicsStats.stepMilliseconds,
				physicsStats.bodyCount,
				physicsStats.activeBodyCount,
				physicsStats.contactCount
			);

			ImGui::PlotLines(
				"##FrameHistory",
				m_frameTimes.data(),
				static_cast<int>(m_frameTimes.size()),
				static_cast<int>(m_frameTimeCursor),
				"FRAME STRIP  /  120 SAMPLES",
				0.0f, 33.3f,
				{ -1.0f, 58.0f }
			);

			if (ImGui::BeginTable(
				"RenderQueue", 3,
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_ScrollY,
				{ 0.0f, 90.0f }))
			{
				ImGui::TableSetupColumn("RENDER ITEM");
				ImGui::TableSetupColumn("SOURCE");
				ImGui::TableSetupColumn("STATE");
				ImGui::TableHeadersRow();

				for (auto* object : objects)
				{
					if (!object || object->getComponent<CameraComponent>())
						continue;

					const char* source = "Transform";
					if (object->getComponent<CubeComponent>()) source = "Cube";
					else if (object->getComponent<PlaneComponent>()) source = "Plane";
					else if (object->getComponent<CombinedMeshComponent>()) source = "Combined";
					else if (object->getComponent<DirectionalLightComponent>()) source = "Light";

					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextUnformatted(object->getName().c_str());
					ImGui::TableNextColumn(); ImGui::TextUnformatted(source);
					ImGui::TableNextColumn(); ImGui::TextColored(rgba(235, 171, 77), "ACTIVE");
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (m_showAssetLens)
	{
		const float assetWidth = std::clamp(workSize.x * 0.28f, 330.0f, 470.0f);
		const float assetHeight = std::clamp(workSize.y * 0.30f, 210.0f, 300.0f);
		ImGui::SetNextWindowPos(
			{ workPosition.x + std::clamp(workSize.x * 0.29f, 330.0f, 470.0f),
			  workPosition.y + workSize.y - assetHeight },
			ImGuiCond_FirstUseEver
		);
		ImGui::SetNextWindowSize(
			{ assetWidth, assetHeight },
			ImGuiCond_FirstUseEver
		);

		if (ImGui::Begin("ASSET LENS##Workbench", &m_showAssetLens))
		{
			ImGui::SetNextItemWidth(-82.0f);
			ImGui::InputTextWithHint(
				"##AssetFilter", "Filter project assets...",
				m_assetFilter, sizeof(m_assetFilter)
			);
			ImGui::SameLine();
			if (ImGui::Button("REFRESH"))
				refreshAssetLens();

			ImGui::TextDisabled("%zu DISCOVERED  /  PROJECT-RELATIVE", m_assetPaths.size());
			ImGui::Separator();

			auto containsFilter = [this](const std::string& path)
			{
				if (m_assetFilter[0] == '\0') return true;
				std::string haystack = path;
				std::string needle = m_assetFilter;
				std::transform(haystack.begin(), haystack.end(), haystack.begin(),
					[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				std::transform(needle.begin(), needle.end(), needle.begin(),
					[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				return haystack.find(needle) != std::string::npos;
			};

			if (ImGui::BeginTable(
				"Assets", 2,
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_ScrollY))
			{
				ImGui::TableSetupColumn("TYPE", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("PATH");
				ImGui::TableHeadersRow();

				for (const auto& path : m_assetPaths)
				{
					if (!containsFilter(path)) continue;
					const std::string extension =
						std::filesystem::path(path).extension().string();
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextColored(rgba(150, 190, 176), "%s",
						extension.empty() ? "FILE" : extension.c_str() + 1);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(path.c_str());
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

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
		if (m_inputSystem->isKeyPressed(KeyCode::Z))
		{
			undo();
		}
		else if (m_inputSystem->isKeyPressed(KeyCode::Y))
		{
			redo();
		}
		else if (m_inputSystem->isKeyPressed(KeyCode::D))
		{
			duplicateSelectedObject();
		}
		else if (m_inputSystem->isKeyPressed(
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
		pushUndoSnapshot();
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
