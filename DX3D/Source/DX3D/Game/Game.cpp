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
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CylinderComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/ColliderComponent.h>
#include <DX3D/Component/ComponentCatalog.h>
#include <DX3D/Component/RotatorComponent.h>
#include <DX3D/Component/FlyControllerComponent.h>
#include <DX3D/Component/TextureComponent.h>
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
#include <sstream>
#include <fstream>
#include <commdlg.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

namespace
{
	bool loadObjMeshData(const std::filesystem::path& path, dx3d::MeshData& output)
	{
		std::ifstream stream(path);
		if (!stream) return false;
		std::vector<dx3d::Vec3> positions{};
		std::vector<dx3d::Vec3> normals{};
		struct Reference { int position{}; int normal{}; };
		auto resolveIndex = [](int index, size_t count) -> int
		{
			if (index > 0) return index - 1;
			if (index < 0) return static_cast<int>(count) + index;
			return -1;
		};
		auto parseReference = [](const std::string& token)
		{
			Reference result{};
			const size_t first = token.find('/');
			const size_t second = first == std::string::npos ? std::string::npos : token.find('/', first + 1);
			try
			{
				result.position = std::stoi(token.substr(0, first));
				if (second != std::string::npos && second + 1 < token.size())
					result.normal = std::stoi(token.substr(second + 1));
			}
			catch (...) { return Reference{}; }
			return result;
		};
		std::string line{};
		while (std::getline(stream, line))
		{
			std::istringstream row(line);
			std::string kind{};
			row >> kind;
			if (kind == "v")
			{
				dx3d::Vec3 value{};
				if (row >> value.x >> value.y >> value.z) positions.push_back(value);
			}
			else if (kind == "vn")
			{
				dx3d::Vec3 value{};
				if (row >> value.x >> value.y >> value.z) normals.push_back(value);
			}
			else if (kind == "f")
			{
				std::vector<Reference> face{};
				std::string token{};
				while (row >> token) face.push_back(parseReference(token));
				for (size_t triangle = 1; triangle + 1 < face.size(); ++triangle)
				{
					const Reference references[3]{ face[0], face[triangle], face[triangle + 1] };
					dx3d::Vec3 vertices[3]{};
					bool valid = true;
					for (size_t corner = 0; corner < 3; ++corner)
					{
						const int index = resolveIndex(references[corner].position, positions.size());
						if (index < 0 || static_cast<size_t>(index) >= positions.size()) { valid = false; break; }
						vertices[corner] = positions[index];
					}
					if (!valid) continue;
					const dx3d::Vec3 first{ vertices[1].x - vertices[0].x, vertices[1].y - vertices[0].y, vertices[1].z - vertices[0].z };
					const dx3d::Vec3 second{ vertices[2].x - vertices[0].x, vertices[2].y - vertices[0].y, vertices[2].z - vertices[0].z };
					dx3d::Vec3 faceNormal{
						first.y * second.z - first.z * second.y,
						first.z * second.x - first.x * second.z,
						first.x * second.y - first.y * second.x };
					const float normalLength = std::sqrt(faceNormal.x * faceNormal.x + faceNormal.y * faceNormal.y + faceNormal.z * faceNormal.z);
					if (normalLength > 0.00001f) faceNormal = faceNormal * (1.0f / normalLength);
					for (size_t corner = 0; corner < 3; ++corner)
					{
						dx3d::Vec3 normal = faceNormal;
						const int normalIndex = resolveIndex(references[corner].normal, normals.size());
						if (normalIndex >= 0 && static_cast<size_t>(normalIndex) < normals.size()) normal = normals[normalIndex];
						output.indices.push_back(static_cast<dx3d::ui32>(output.vertices.size()));
						output.vertices.push_back({ vertices[corner], { 1, 1, 1, 1 }, normal });
					}
				}
			}
		}
		return !output.empty();
	}

	bool projectWorldPoint(
		const dx3d::Vec3& world,
		const dx3d::Mat4x4& viewProjection,
		const dx3d::TransformGizmo::ViewportArea& viewport,
		ImVec2& screen)
	{
		const dx3d::Vec4 clip = viewProjection.transform(
			{ world.x, world.y, world.z, 1.0f });
		if (clip.w <= 0.001f) return false;
		const float inverseW = 1.0f / clip.w;
		const float x = clip.x * inverseW;
		const float y = clip.y * inverseW;
		const float z = clip.z * inverseW;
		if (z < 0.0f || z > 1.0f) return false;
		screen = {
			viewport.x + (x * 0.5f + 0.5f) * viewport.width,
			viewport.y + (-y * 0.5f + 0.5f) * viewport.height };
		return true;
	}

	bool isEditorCamera(const dx3d::GameObject* object)
	{
		return object && object->getName() == "Editor Camera";
	}

	std::filesystem::path chooseScenePath(HWND owner, const std::filesystem::path& initial)
	{
		const DPI_AWARENESS_CONTEXT previousDpiContext =
			SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		wchar_t path[MAX_PATH]{};
		if (!initial.empty()) wcsncpy_s(path, initial.c_str(), _TRUNCATE);
		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = owner;
		dialog.lpstrFilter =
			L"jnpf. Scenes (*.dx3dscene;*.escene)\0*.dx3dscene;*.escene\0"
			L"jnpf. DirectX Scene (*.dx3dscene)\0*.dx3dscene\0"
			L"jnpf. Original Scene (*.escene)\0*.escene\0All Files (*.*)\0*.*\0";
		dialog.lpstrFile = path;
		dialog.nMaxFile = static_cast<DWORD>(std::size(path));
		dialog.lpstrDefExt = L"dx3dscene";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		const bool accepted = GetOpenFileNameW(&dialog) != FALSE;
		if (previousDpiContext) SetThreadDpiAwarenessContext(previousDpiContext);
		return accepted ? std::filesystem::path(path) : std::filesystem::path{};
	}

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
	m_startupScenePath = desc.startupScenePath;

	m_logger = std::make_unique<Logger>(desc.logLevel);

	DX3DLogInfo("GDENG03 | DirectX Game Engine");
	DX3DLogInfo("--------------------------------------");

	m_inputSystem = std::make_unique<InputSystem>(InputSystemDesc{ *m_logger });
	m_graphicsDevice = std::make_shared<GraphicsDevice>(GraphicsDeviceDesc{ *m_logger });
	m_display = std::make_unique<Display>(DisplayDesc{ {*m_logger,desc.windowSize},*m_graphicsDevice });
	m_world = std::make_unique<World>(WorldDesc{ BaseDesc{*m_logger}, GameContext{*m_inputSystem} });
	m_worldRenderer = std::make_unique<WorldRenderer>(WorldRendererDesc{ {*m_logger},*m_graphicsDevice });
	m_physicsWorld = std::make_unique<PhysicsWorld>();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_uiScale = std::clamp(
		static_cast<f32>(GetDpiForWindow(
			static_cast<HWND>(m_display->getNativeHandle()))) / 96.0f,
		0.75f,
		3.0f);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	applyWorkbenchStyle();
	if (m_uiScale != 1.0f)
		ImGui::GetStyle().ScaleAllSizes(m_uiScale);

	ImFont* editorFont = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\SegUIVar.ttf",
		18.0f * m_uiScale
	);

	if (!editorFont)
	{
		editorFont = io.Fonts->AddFontFromFileTTF(
			"C:\\Windows\\Fonts\\segoeui.ttf",
			18.0f * m_uiScale
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

	if (object->getComponent<SphereComponent>())
	{
		return &getSphereMeshData();
	}

	if (object->getComponent<CylinderComponent>())
		return &getCylinderMeshData();

	if (object->getComponent<CapsuleComponent>())
		return &getCapsuleMeshData();

	if (object->getComponent<PlaneComponent>())
	{
		return &getPlaneMeshData();
	}

	return nullptr;
}

bool dx3d::Game::canMergeSelectedObjects() const noexcept
{
	if (m_editorMode != EditorMode::Editing)
		return false;

	if (m_selectedObjects.size() < 2)
		return false;

	for (auto* object : m_selectedObjects)
	{
		if (!object)
			return false;

		if (object->getComponent<CameraComponent>())
			return false;

		if (!getObjectMeshData(object))
			return false;
	}

	return true;
}

void dx3d::Game::mergeSelectedObjects()
{
	if (m_editorMode != EditorMode::Editing)
		return;

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


	for (auto& vertex : combinedMesh.vertices)
	{
		vertex.position.x -= mergedPivot.x;
		vertex.position.y -= mergedPivot.y;
		vertex.position.z -= mergedPivot.z;
	}

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

	for (auto* originalObject : originalObjects)
	{
		m_world->destroyGameObject(
			originalObject
		);
	}


	selectOnly(mergedObject);
}

bool dx3d::Game::canCopySelectedObject() const noexcept
{
	if (!m_selectedObject)
		return false;

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
	if (m_selectedObject->getComponent<SphereComponent>() ||
		m_selectedObject->getComponent<CylinderComponent>() ||
		m_selectedObject->getComponent<CapsuleComponent>()) return true;

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
	copiedData.activeSelf = m_selectedObject->isActiveSelf();

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
		copiedData.rigidBodyFriction = rigidBody->getFriction();
		copiedData.rigidBodyRestitution = rigidBody->getRestitution();
		copiedData.rigidBodyLinearDamping = rigidBody->getLinearDamping();
		copiedData.rigidBodyAngularDamping = rigidBody->getAngularDamping();
		copiedData.rigidBodyGravityFactor = rigidBody->getGravityFactor();
		copiedData.rigidBodyEnabled = rigidBody->isEnabled();
	}
	if (auto* texture = m_selectedObject->getComponent<TextureComponent>())
	{
		copiedData.hasTexture = true;
		copiedData.textureAssetPath = texture->getAssetPath();
		copiedData.textureEnabled = texture->isEnabled();
	}
	if (auto* collider = m_selectedObject->getComponent<ColliderComponent>())
	{
		copiedData.hasCollider = true;
		copiedData.colliderShape = collider->getShape();
		copiedData.colliderHalfExtents = collider->getHalfExtents();
		copiedData.colliderRadius = collider->getRadius();
	}

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
	else if (m_selectedObject->getComponent<SphereComponent>())
		copiedData.type = CopiedObjectType::Sphere;
	else if (m_selectedObject->getComponent<CylinderComponent>())
		copiedData.type = CopiedObjectType::Cylinder;
	else if (m_selectedObject->getComponent<CapsuleComponent>())
		copiedData.type = CopiedObjectType::Capsule;
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

	copiedData.pasteCount = 0;

	m_objectClipboard =
		copiedData;
}

void dx3d::Game::pasteCopiedObject()
{
	if (m_editorMode != EditorMode::Editing || !m_objectClipboard.isValid)
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
	case CopiedObjectType::Sphere:
		pastedObject->createOrGetComponent<SphereComponent>();
		break;
	case CopiedObjectType::Cylinder:
		pastedObject->createOrGetComponent<CylinderComponent>();
		break;
	case CopiedObjectType::Capsule:
		pastedObject->createOrGetComponent<CapsuleComponent>();
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
	pastedObject->setActive(m_objectClipboard.activeSelf);

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
		rigidBody->setFriction(m_objectClipboard.rigidBodyFriction);
		rigidBody->setRestitution(m_objectClipboard.rigidBodyRestitution);
		rigidBody->setLinearDamping(m_objectClipboard.rigidBodyLinearDamping);
		rigidBody->setAngularDamping(m_objectClipboard.rigidBodyAngularDamping);
		rigidBody->setGravityFactor(m_objectClipboard.rigidBodyGravityFactor);
		rigidBody->setEnabled(m_objectClipboard.rigidBodyEnabled);
	}
	if (m_objectClipboard.hasTexture)
	{
		auto* texture = pastedObject->createOrGetComponent<TextureComponent>();
		texture->setAssetPath(m_objectClipboard.textureAssetPath);
		texture->setEnabled(m_objectClipboard.textureEnabled);
	}
	if (m_objectClipboard.hasCollider)
	{
		auto* collider = pastedObject->createOrGetComponent<ColliderComponent>();
		collider->setShape(m_objectClipboard.colliderShape);
		collider->setHalfExtents(m_objectClipboard.colliderHalfExtents);
		collider->setRadius(m_objectClipboard.colliderRadius);
	}

	selectOnly(
		pastedObject
	);
}

void dx3d::Game::duplicateSelectedObject()
{
	if (m_editorMode != EditorMode::Editing)
		return;

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
	if (!m_sceneViewportHovered)
		return;

	const bool rightMouseDown =
		m_inputSystem->isKeyDown(
			KeyCode::MouseRight
		);

	if (rightMouseDown)
		return;

	if (m_transformGizmo.isUsing() ||
		m_transformGizmo.getHoveredAxis() !=
		TransformGizmo::Axis::None)
	{
		return;
	}

	if (ImGui::GetIO().KeyAlt)
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

	// Camera entities have no mesh to ray-test, so their Scene-view icon is
	// an explicit selectable target just like it is in enignE.
	GameObject* closestCamera = nullptr;
	f32 closestCameraDistanceSquared = std::numeric_limits<f32>::max();
	const Mat4x4 viewProjection = camera->getViewMatrix() * camera->getProjectionMatrix();
	for (auto* object : m_world->getGameObjects())
	{
		if (!object || !object->isActiveInHierarchy() || isEditorCamera(object) ||
			!object->getComponent<CameraComponent>()) continue;
		ImVec2 iconPosition{};
		if (!projectWorldPoint(object->getTransform().getPosition(), viewProjection,
			viewportArea, iconPosition)) continue;
		const f32 deltaX = mousePosition.x - iconPosition.x;
		const f32 deltaY = mousePosition.y - iconPosition.y;
		const f32 distanceSquared = deltaX * deltaX + deltaY * deltaY;
		if (distanceSquared < closestCameraDistanceSquared)
		{
			closestCameraDistanceSquared = distanceSquared;
			closestCamera = object;
		}
	}
	const f32 cameraPickRadius = 16.0f * m_uiScale;
	if (closestCamera && closestCameraDistanceSquared <= cameraPickRadius * cameraPickRadius)
	{
		if (ImGui::GetIO().KeyCtrl) toggleObjectSelection(closestCamera);
		else selectOnly(closestCamera);
		return;
	}

	GameObject* closestLight = nullptr;
	f32 closestLightDistanceSquared = std::numeric_limits<f32>::max();
	for (auto* object : m_world->getGameObjects())
	{
		if (!object || !object->isActiveInHierarchy() ||
			!object->getComponent<DirectionalLightComponent>()) continue;
		ImVec2 iconPosition{};
		if (!projectWorldPoint(object->getTransform().getPosition(), viewProjection,
			viewportArea, iconPosition)) continue;
		const f32 deltaX = mousePosition.x - iconPosition.x;
		const f32 deltaY = mousePosition.y - iconPosition.y;
		const f32 distanceSquared = deltaX * deltaX + deltaY * deltaY;
		if (distanceSquared < closestLightDistanceSquared)
		{
			closestLightDistanceSquared = distanceSquared;
			closestLight = object;
		}
	}
	const f32 lightPickRadius = 16.0f * m_uiScale;
	if (closestLight && closestLightDistanceSquared <= lightPickRadius * lightPickRadius)
	{
		if (ImGui::GetIO().KeyCtrl) toggleObjectSelection(closestLight);
		else selectOnly(closestLight);
		return;
	}

	const PickingViewportArea pickingViewport
	{
		viewportArea.x,
		viewportArea.y,
		viewportArea.width,
		viewportArea.height
	};

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
				if (!object || !object->isActiveInHierarchy())
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

		findClosestHit(
			pickingRay,
			false,
			meshObject,
			meshDistance
		);

		findClosestHit(
			pickingRay,
			true,
			planeObject,
			planeDistance
		);

		if (offsetIndex == 0)
		{
			centerPlaneObject =
				planeObject;

			centerPlaneDistance =
				planeDistance;
		}

		if (!meshObject)
			continue;

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
		closestObject =
			bestMeshObject;
	}
	else if (centerPlaneObject)
	{
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
	if (m_editorMode != EditorMode::Editing)
		return;

	pushUndoSnapshot();

	SceneSerializer::clear(
		*m_world
	);

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter = 0;
	m_planeCounter = 0;
	m_sceneFilePath = "Scene.dx3dscene";

	m_sceneStatusMessage =
		"New scene created";
	m_sceneDirty = true;
	m_focusSceneViewRequested = true;
	m_focusGameViewRequested = false;
	ensureEditorCamera();
	ensureGameCamera();
}

dx3d::Vec3 dx3d::Game::getSceneSpawnPosition() const noexcept
{
	if (!m_editorCamera) return {};
	auto& transform = m_editorCamera->getTransform();
	return transform.getPosition() + transform.forward() * 5.0f;
}

dx3d::GameObject* dx3d::Game::createPrimitiveObject(PrimitiveKind kind, const Vec3& position)
{
	if (m_editorMode != EditorMode::Editing) return nullptr;
	pushUndoSnapshot();
	auto* object = m_world->createGameObject<GameObject>();
	const char* baseName = "Object";
	switch (kind)
	{
	case PrimitiveKind::Cube: baseName = "Cube"; object->createOrGetComponent<CubeComponent>(); break;
	case PrimitiveKind::Sphere: baseName = "Sphere"; object->createOrGetComponent<SphereComponent>(); break;
	case PrimitiveKind::Cylinder: baseName = "Cylinder"; object->createOrGetComponent<CylinderComponent>(); break;
	case PrimitiveKind::Capsule: baseName = "Capsule"; object->createOrGetComponent<CapsuleComponent>(); break;
	case PrimitiveKind::Plane: baseName = "Plane"; object->createOrGetComponent<PlaneComponent>(); break;
	}
	ui32 matchingNames = 0;
	for (auto* existing : m_world->getGameObjects())
		if (existing && existing != object && existing->getName().starts_with(baseName)) ++matchingNames;
	object->setName(matchingNames == 0 ? baseName : std::string(baseName) + " " + std::to_string(matchingNames + 1));
	object->createOrGetComponent<MaterialComponent>();
	object->getTransform().setPosition(position);
	if (kind == PrimitiveKind::Plane) object->getTransform().setScale({ 5.0f, 1.0f, 5.0f });
	selectOnly(object);
	m_sceneDirty = true;
	return object;
}

dx3d::GameObject* dx3d::Game::importObjAsset(const std::string& assetPath, const Vec3& position)
{
	if (m_editorMode != EditorMode::Editing) return nullptr;
	MeshData mesh{};
	if (!loadObjMeshData(assetPath, mesh))
	{
		m_sceneStatusMessage = "OBJ import failed: " + assetPath;
		DX3DLogError("OBJ import failed: {}", assetPath);
		return nullptr;
	}
	pushUndoSnapshot();
	auto* object = m_world->createGameObject<GameObject>();
	object->setName(std::filesystem::path(assetPath).stem().string());
	auto* combined = object->createOrGetComponent<CombinedMeshComponent>();
	combined->setMeshData(std::move(mesh));
	object->createOrGetComponent<MaterialComponent>();
	object->getTransform().setPosition(position);
	selectOnly(object);
	m_sceneDirty = true;
	m_sceneStatusMessage = "Imported OBJ: " + assetPath;
	DX3DLogInfo("Imported OBJ: {}", assetPath);
	return object;
}

void dx3d::Game::drawObjectCreationMenu(const Vec3& position)
{
	if (m_editorMode != EditorMode::Editing)
		return;

	if (ImGui::MenuItem("Empty object"))
	{
		pushUndoSnapshot();
		auto* object = m_world->createGameObject<GameObject>();
		object->setName("Empty Object");
		object->getTransform().setPosition(position);
		selectOnly(object);
		m_sceneDirty = true;
	}
	if (ImGui::BeginMenu("3D object"))
	{
		if (ImGui::MenuItem("Cube")) createPrimitiveObject(PrimitiveKind::Cube, position);
		if (ImGui::MenuItem("Sphere")) createPrimitiveObject(PrimitiveKind::Sphere, position);
		if (ImGui::MenuItem("Capsule")) createPrimitiveObject(PrimitiveKind::Capsule, position);
		if (ImGui::MenuItem("Cylinder")) createPrimitiveObject(PrimitiveKind::Cylinder, position);
		if (ImGui::MenuItem("Plane")) createPrimitiveObject(PrimitiveKind::Plane, position);
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Camera"))
	{
		pushUndoSnapshot();
		auto* object = m_world->createGameObject<GameObject>();
		object->setName("Camera");
		object->createOrGetComponent<CameraComponent>();
		object->createOrGetComponent<FlyControllerComponent>();
		object->getTransform().setPosition(position);
		selectOnly(object);
		m_sceneDirty = true;
	}
	if (ImGui::MenuItem("Light"))
	{
		pushUndoSnapshot();
		auto* object = m_world->createGameObject<GameObject>();
		object->setName("Light");
		object->createOrGetComponent<DirectionalLightComponent>();
		object->getTransform().setPosition(position);
		selectOnly(object);
		m_sceneDirty = true;
	}
}

void dx3d::Game::ensureEditorCamera()
{
	ui32 cameraCount = 0;
	const auto cameras = m_world->getComponents<CameraComponent>(cameraCount);
	for (ui32 index = 0; index < cameraCount; ++index)
	{
		if (cameras[index] && isEditorCamera(&cameras[index]->getGameObject()))
		{
			m_editorCamera = &cameras[index]->getGameObject();
			return;
		}
	}

	m_editorCamera = m_world->createGameObject<GameObject>();
	m_editorCamera->setName("Editor Camera");
	auto* camera = m_editorCamera->createOrGetComponent<CameraComponent>();
	camera->setNearPlane(0.1f);
	camera->setFarPlane(100.0f);
	camera->setFieldOfView(MathUtils::PI * 0.25f);
	m_editorCamera->getTransform().setPosition({ 5.0f, 4.5f, -7.5f });
	m_editorCamera->getTransform().setRotation({ 0.322f, -0.633f, 0.0f });
}

void dx3d::Game::ensureGameCamera()
{
	ui32 cameraCount = 0;
	const auto cameras = m_world->getComponents<CameraComponent>(cameraCount);
	CameraComponent* firstGameCamera = nullptr;
	bool hasPrimary = false;
	for (ui32 index = 0; index < cameraCount; ++index)
		if (cameras[index] && !isEditorCamera(&cameras[index]->getGameObject()))
		{
			cameras[index]->getGameObject().createOrGetComponent<FlyControllerComponent>();
			if (!firstGameCamera) firstGameCamera = cameras[index];
			hasPrimary = hasPrimary || cameras[index]->isPrimary();
		}
	if (firstGameCamera)
	{
		if (!hasPrimary) firstGameCamera->setPrimary(true);
		return;
	}

	auto* cameraObject = m_world->createGameObject<GameObject>();
	cameraObject->setName("Main Camera");
	auto* camera = cameraObject->createOrGetComponent<CameraComponent>();
	camera->setNearPlane(0.1f);
	camera->setFarPlane(100.0f);
	camera->setFieldOfView(MathUtils::PI * 0.25f);
	camera->setPrimary(true);
	cameraObject->getTransform().setPosition({ 0.0f, 0.0f, -3.0f });
	cameraObject->getTransform().setRotation({ 0.0f, 0.0f, 0.0f });
	cameraObject->createOrGetComponent<FlyControllerComponent>();
}

void dx3d::Game::saveScene()
{
	if (m_editorMode != EditorMode::Editing) stopPlayMode();
	if (m_editorMode != EditorMode::Editing) return;
	const bool saved =
		SceneSerializer::save(
			*m_world,
			m_sceneFilePath
		);

	if (saved)
	{
		m_sceneStatusMessage =
			"Saved: " + m_sceneFilePath;

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
	if (m_editorMode != EditorMode::Editing) stopPlayMode();
	if (m_editorMode != EditorMode::Editing) return;
	if (m_sceneDirty)
	{
		m_requestSceneLoad = true;
		return;
	}
	openSceneDialog();
}

void dx3d::Game::openSceneDialog()
{
	auto path = chooseScenePath(
		static_cast<HWND>(m_display->getNativeHandle()), m_sceneFilePath);
	if (path.extension() == ".escene")
	{
		auto convertedPath = path;
		convertedPath.replace_extension(".dx3dscene");
		if (!std::filesystem::exists(convertedPath))
		{
			convertedPath = std::filesystem::path("Scenes") / "enignE" / path.filename();
			convertedPath.replace_extension(".dx3dscene");
		}
		if (!std::filesystem::exists(convertedPath))
		{
			m_sceneStatusMessage = "No DX11 counterpart exists for: " + path.string();
			DX3DLogError("Selected .escene has no converted DX11 counterpart.");
			return;
		}
		path = std::move(convertedPath);
	}
	if (!path.empty()) loadScene(path.string());
}

void dx3d::Game::loadScene(const std::string& filePath)
{
	if (m_editorMode != EditorMode::Editing) stopPlayMode();
	if (filePath.empty() || m_editorMode != EditorMode::Editing)
		return;

	const std::string previousScene =
		SceneSerializer::serialize(*m_world);

	const SceneLoadResult result =
		SceneSerializer::load(
			*m_world,
			filePath
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
	ensureEditorCamera();
	ensureGameCamera();

	pushUndoSnapshot(previousScene);

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter =
		result.cubeCount;

	m_planeCounter =
		result.planeCount;

	m_sceneFilePath =
		std::filesystem::path(filePath).generic_string();

	m_sceneStatusMessage =
		"Loaded: " + m_sceneFilePath;
	m_sceneDirty = false;
	m_focusSceneViewRequested = true;
	m_focusGameViewRequested = false;

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
		ensureEditorCamera();
		ensureGameCamera();
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
		ensureEditorCamera();
		ensureGameCamera();
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
	const SceneLoadResult playScene =
		SceneSerializer::deserialize(*m_world, m_editorSceneSnapshot);
	if (!playScene.success)
	{
		m_editorSceneSnapshot.clear();
		m_sceneStatusMessage = "Could not reconstruct Play scene";
		DX3DLogError("Play snapshot deserialization failed.");
		return;
	}
	ensureEditorCamera();
	ensureGameCamera();
	m_physicsWorld->reset(*m_world);
	clearSelection();
	m_fixedStepAccumulator = 0.0f;
	m_editorMode = EditorMode::Playing;
	m_focusSceneViewRequested = false;
	m_focusGameViewRequested = true;
	m_sceneViewportHovered = false;
	m_sceneViewportFocused = false;
	setGameInputCaptured(true);
	m_sceneStatusMessage = "Play Mode";
}

void dx3d::Game::setGameInputCaptured(bool captured)
{
	const bool allowed = captured && m_editorMode == EditorMode::Playing;
	if (m_gameInputCaptured == allowed)
		return;

	m_gameInputCaptured = allowed;
	m_inputSystem->setCursorLocked(allowed);
	m_inputSystem->setCursorVisible(!allowed);
}

void dx3d::Game::updateFlyControllers(f32 deltaTime)
{
	if (m_editorMode != EditorMode::Playing || !m_gameInputCaptured) return;
	ui32 controllerCount = 0;
	auto controllers = m_world->getComponents<FlyControllerComponent>(controllerCount);
	const Vec2 mouseDelta = m_inputSystem->getMouseDelta();
	for (ui32 index = 0; index < controllerCount; ++index)
	{
		auto* controller = controllers[index];
		if (!controller || !controller->isEnabled()) continue;
		auto& object = controller->getGameObject();
		auto* camera = object.getComponent<CameraComponent>();
		if (!object.isActiveInHierarchy() || !camera || !camera->isPrimary()) continue;

		auto& transform = object.getTransform();
		Vec3 rotation = transform.getRotation();
		rotation.y += mouseDelta.x * controller->getLookSensitivity();
		rotation.x += mouseDelta.y * controller->getLookSensitivity();
		const f32 pitchLimit = controller->getPitchLimitDegrees() * MathUtils::PI / 180.0f;
		rotation.x = std::clamp(rotation.x, -pitchLimit, pitchLimit);
		rotation.z = 0.0f;
		transform.setRotation(rotation);

		Vec3 movement{};
		const Vec3 forward = transform.forward();
		const Vec3 right = transform.right();
		if (m_inputSystem->isKeyDown(KeyCode::W)) movement = movement + forward;
		if (m_inputSystem->isKeyDown(KeyCode::S)) movement = movement + forward * -1.0f;
		if (m_inputSystem->isKeyDown(KeyCode::D)) movement = movement + right;
		if (m_inputSystem->isKeyDown(KeyCode::A)) movement = movement + right * -1.0f;
		if (m_inputSystem->isKeyDown(KeyCode::E) || m_inputSystem->isKeyDown(KeyCode::Space)) movement.y += 1.0f;
		if (m_inputSystem->isKeyDown(KeyCode::Q)) movement.y -= 1.0f;
		const f32 length = std::sqrt(movement.x * movement.x + movement.y * movement.y + movement.z * movement.z);
		if (length <= 0.0001f) continue;
		movement = movement * (1.0f / length);
		const f32 boost = m_inputSystem->isKeyDown(KeyCode::Shift)
			? controller->getBoostMultiplier() : 1.0f;
		transform.setPosition(transform.getPosition() +
			movement * (controller->getMoveSpeed() * boost * deltaTime));
	}
}

void dx3d::Game::togglePauseMode()
{
	if (m_editorMode == EditorMode::Editing)
		return;

	if (m_editorMode == EditorMode::Playing)
	{
		m_editorMode = EditorMode::Paused;
		setGameInputCaptured(false);
		m_sceneStatusMessage = "Simulation paused";
		return;
	}

	m_editorMode = EditorMode::Playing;
	setGameInputCaptured(true);
	m_focusSceneViewRequested = false;
	m_focusGameViewRequested = true;
	m_sceneStatusMessage = "Simulation resumed";
}

void dx3d::Game::stepSimulation()
{
	if (m_editorMode == EditorMode::Editing)
		return;

	if (m_editorMode == EditorMode::Playing)
	{
		m_editorMode = EditorMode::Paused;
		setGameInputCaptured(false);
	}

	m_singleStepRequested = true;
	m_sceneStatusMessage = "Simulation step";
}

void dx3d::Game::stopPlayMode()
{
	if (m_editorMode == EditorMode::Editing)
		return;
	setGameInputCaptured(false);

	if (!m_editorSceneSnapshot.empty())
	{
		const SceneLoadResult restoredScene = SceneSerializer::deserialize(
			*m_world,
			m_editorSceneSnapshot
		);
		if (!restoredScene.success)
		{
			m_sceneStatusMessage = "Could not restore scene after Play";
			DX3DLogError("Editor snapshot restoration failed.");
			return;
		}
		ensureEditorCamera();
		ensureGameCamera();
		m_physicsWorld->reset(*m_world);
	}

	clearSelection();
	m_editorSceneSnapshot.clear();
	m_fixedStepAccumulator = 0.0f;
	m_singleStepRequested = false;
	m_editorMode = EditorMode::Editing;
	m_focusSceneViewRequested = true;
	m_focusGameViewRequested = false;
	m_sceneStatusMessage = "Edit Mode";
}

void dx3d::Game::refreshAssetLens()
{
	m_assetPaths.clear();

	std::error_code error{};
	auto scanRoot = [this, &error](const std::filesystem::path& root)
	{
		if (!std::filesystem::exists(root, error))
		{
			error.clear();
			return;
		}

		for (std::filesystem::recursive_directory_iterator iterator(
			root,
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
				std::string extension = iterator->path().extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(),
					[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				if (extension == ".meta" || extension == ".escene" ||
					extension == ".dx3dscene" || extension == ".json" ||
					iterator->path().filename() == ".DS_Store") continue;
				m_assetPaths.push_back(iterator->path().generic_string());
			}
		}
	};

	scanRoot("assets");
	scanRoot(std::filesystem::path("DX3D") / "Assets");

	std::sort(m_assetPaths.begin(), m_assetPaths.end());
	m_assetPaths.erase(
		std::unique(m_assetPaths.begin(), m_assetPaths.end()),
		m_assetPaths.end()
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

	const Rect cursorLockArea = m_gameInputCaptured &&
		m_gameViewportScreenArea.width > 0 && m_gameViewportScreenArea.height > 0
		? m_gameViewportScreenArea
		: m_display->getClientAreaInScreenSpace();
	m_inputSystem->setCursorLockArea(cursorLockArea);

	// Begin the ImGui frame.
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	if (m_gameInputCaptured &&
		m_inputSystem->isKeyPressed(KeyCode::Escape))
	{
		setGameInputCaptured(false);
	}
	if (m_inputSystem->isKeyPressed(KeyCode::F6))
	{
		if (m_editorMode == EditorMode::Editing)
			startPlayMode();
		else
			stopPlayMode();
	}
	else if (m_inputSystem->isKeyPressed(KeyCode::F7))
	{
		togglePauseMode();
	}
	else if (m_inputSystem->isKeyPressed(KeyCode::F8))
	{
		stepSimulation();
	}

	m_frameTimes[m_frameTimeCursor] = deltaTime * 1000.0f;
	m_frameTimeCursor =
		(m_frameTimeCursor + 1) % m_frameTimes.size();

	onUpdate(deltaTime);
	updateFlyControllers(deltaTime);

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
			m_world->fixedUpdate(fixedTimeStep);
			m_physicsWorld->step(*m_world, fixedTimeStep);
			m_fixedStepAccumulator -= fixedTimeStep;
			++steps;
		}
	}
	else if (m_singleStepRequested)
	{
		m_world->update(fixedTimeStep);
		m_world->fixedUpdate(fixedTimeStep);
		m_physicsWorld->step(*m_world, fixedTimeStep);
		m_singleStepRequested = false;
	}

	// Render the 3D scene first.
	m_worldRenderer->render(
		*m_world,
		m_display->getSwapChain(),
		deltaTime,
		false
	);
	m_display->getSwapChain().captureSceneFrame();
	m_worldRenderer->render(
		*m_world,
		m_display->getSwapChain(),
		deltaTime,
		true
	);
	m_display->getSwapChain().captureGameFrame();

	// Native enignE chrome: a fixed 32 px title bar owns the menus and the
	// borderless Win32 caption controls.
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float titleBarHeight = 32.0f * m_uiScale;
	const float captionButtonWidth = 46.0f * m_uiScale;
	const float captionControlsWidth = captionButtonWidth * 3.0f;
	ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
	ImGui::SetNextWindowSize({ viewport->Size.x, titleBarHeight }, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, rgba(18, 18, 20));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	const ImGuiWindowFlags titleBarFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	if (ImGui::Begin("##TitleBar", nullptr, titleBarFlags))
	{
		const ImVec2 barMin = ImGui::GetWindowPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 captionText = IM_COL32(224, 222, 216, 255);
		const ImVec2 iconMin{ barMin.x + 10.0f * m_uiScale, barMin.y + 9.0f * m_uiScale };
		const ImVec2 iconMax{ iconMin.x + 14.0f * m_uiScale, iconMin.y + 14.0f * m_uiScale };
		drawList->AddRect(iconMin, iconMax, captionText);
		drawList->AddLine({ iconMin.x + 1.0f, iconMin.y + 4.0f },
			{ iconMax.x - 1.0f, iconMin.y + 4.0f }, captionText);
		drawList->AddText(
			{ barMin.x + 32.0f * m_uiScale, barMin.y + (titleBarHeight - ImGui::GetFontSize()) * 0.5f },
			captionText, m_sceneDirty ? "jnpf.  *" : "jnpf.");

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.07f, 0.07f, 0.075f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.215f, 0.205f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.285f, 0.15f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8.0f * m_uiScale, 4.0f * m_uiScale });

		ImGui::SetCursorScreenPos({ barMin.x + 92.0f * m_uiScale, barMin.y + 4.0f * m_uiScale });
		if (ImGui::Button("File", { 48.0f * m_uiScale, 24.0f * m_uiScale }))
			ImGui::OpenPopup("##FileMenu");
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			{ 12.0f * m_uiScale, 8.0f * m_uiScale });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			{ 10.0f * m_uiScale, 5.0f * m_uiScale });
		if (ImGui::BeginPopup("##FileMenu"))
		{
			if (false && ImGui::MenuItem(
				"New Scene"
			))
			{
				createNewScene();
			}

			if (ImGui::MenuItem(
				"Save", "Ctrl+S"
			))
			{
				saveScene();
			}

			if (ImGui::MenuItem(
				"Load", "Ctrl+O"
			))
			{
				loadScene();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) m_requestEditorClose = true;

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);

		constexpr bool showLegacyChromeMenus = false;
		ImGui::SetCursorScreenPos({ barMin.x + 196.0f * m_uiScale, barMin.y + 4.0f * m_uiScale });
		if (showLegacyChromeMenus && ImGui::Button("Create", { 56.0f * m_uiScale, 24.0f * m_uiScale }))
			ImGui::OpenPopup("##CreateMenu");
		if (ImGui::BeginPopup("##CreateMenu"))
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

			if (ImGui::MenuItem(
				"Create Light",
				nullptr,
				false,
				true
			))
			{
				pushUndoSnapshot();
				auto* lightObject =
					m_world->createGameObject<GameObject>();

				lightObject->setName(
					"Light"
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
				m_editorMode == EditorMode::Editing &&
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

			ImGui::EndPopup();
		}

		ImGui::SetCursorScreenPos({ barMin.x + 144.0f * m_uiScale, barMin.y + 4.0f * m_uiScale });
		if (ImGui::Button("Edit", { 48.0f * m_uiScale, 24.0f * m_uiScale }))
			ImGui::OpenPopup("##EditMenu");
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			{ 12.0f * m_uiScale, 8.0f * m_uiScale });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			{ 10.0f * m_uiScale, 5.0f * m_uiScale });
		if (ImGui::BeginPopup("##EditMenu"))
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
			ImGui::Separator();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, canCopySelectedObject()))
				copySelectedObject();
			if (ImGui::MenuItem("Paste", "Ctrl+V", false,
				m_editorMode == EditorMode::Editing && m_objectClipboard.isValid))
				pasteCopiedObject();
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false,
				m_editorMode == EditorMode::Editing && canCopySelectedObject()))
				duplicateSelectedObject();
			const bool canDelete = m_editorMode == EditorMode::Editing &&
				m_selectedObject && !isEditorCamera(m_selectedObject);
			if (ImGui::MenuItem("Delete Selected", "Del", false, canDelete))
			{
				pushUndoSnapshot();
				GameObject* objectToDelete = m_selectedObject;
				m_world->destroyGameObject(objectToDelete);
				removeObjectFromSelection(objectToDelete);
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);

		ImGui::SetCursorScreenPos({ barMin.x + 196.0f * m_uiScale, barMin.y + 4.0f * m_uiScale });
		if (ImGui::Button("Window", { 64.0f * m_uiScale, 24.0f * m_uiScale }))
			ImGui::OpenPopup("##ViewMenu");
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			{ 14.0f * m_uiScale, 10.0f * m_uiScale });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			{ 10.0f * m_uiScale, 7.0f * m_uiScale });
		if (ImGui::BeginPopup("##ViewMenu"))
		{
			ImGui::MenuItem("Stats", nullptr, &m_showStats);
			ImGui::MenuItem("Asset Lens", nullptr, &m_showAssetLens);
			ImGui::MenuItem("Console", nullptr, &m_showConsole);
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);

		ImGui::SetCursorScreenPos({ barMin.x + 308.0f * m_uiScale, barMin.y + 4.0f * m_uiScale });
		if (showLegacyChromeMenus && ImGui::Button("Gizmo", { 56.0f * m_uiScale, 24.0f * m_uiScale }))
			ImGui::OpenPopup("##GizmoMenu");
		if (ImGui::BeginPopup("##GizmoMenu"))
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

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		const ImVec2 dragMin{ barMin.x + 268.0f * m_uiScale, barMin.y };
		const ImVec2 dragMax{
			barMin.x + ImGui::GetWindowWidth() - captionControlsWidth,
			barMin.y + titleBarHeight
		};
		if (ImGui::IsMouseHoveringRect(dragMin, dragMax) &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			m_display->toggleMaximizeRestore();
		}
		else if (ImGui::IsMouseHoveringRect(dragMin, dragMax) &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			m_display->beginTitleBarDrag();
		}

		const auto drawCaptionButton = [drawList, barMin, captionButtonWidth, titleBarHeight](
			const char* id, float x, ImU32 hoveredColor, ImU32 activeColor,
			const std::function<void(ImVec2, ImU32)>& drawGlyph)
		{
			const ImVec2 position{ x, barMin.y };
			ImGui::SetCursorScreenPos(position);
			ImGui::InvisibleButton(id, { captionButtonWidth, titleBarHeight });
			if (ImGui::IsItemHovered() || ImGui::IsItemActive())
			{
				drawList->AddRectFilled(position,
					{ position.x + captionButtonWidth, position.y + titleBarHeight },
					ImGui::IsItemActive() ? activeColor : hoveredColor);
			}
			drawGlyph({ position.x + captionButtonWidth * 0.5f,
				position.y + titleBarHeight * 0.5f }, IM_COL32(245, 245, 245, 255));
			return ImGui::IsItemClicked();
		};

		const float controlsX = barMin.x + ImGui::GetWindowWidth() - captionControlsWidth;
		if (drawCaptionButton("##Minimize", controlsX,
			IM_COL32(44, 43, 42, 255), IM_COL32(72, 58, 34, 255),
			[drawList](ImVec2 center, ImU32 color)
			{
				drawList->AddLine({ center.x - 5.0f, center.y + 3.0f },
					{ center.x + 5.0f, center.y + 3.0f }, color);
			})) m_display->minimize();

		const bool maximized = m_display->isMaximized();
		if (drawCaptionButton("##MaximizeRestore", controlsX + captionButtonWidth,
			IM_COL32(44, 43, 42, 255), IM_COL32(72, 58, 34, 255),
			[drawList, maximized](ImVec2 center, ImU32 color)
			{
				if (maximized)
				{
					drawList->AddRect({ center.x - 3.0f, center.y - 5.0f },
						{ center.x + 5.0f, center.y + 3.0f }, color);
					drawList->AddRect({ center.x - 5.0f, center.y - 3.0f },
						{ center.x + 3.0f, center.y + 5.0f }, color);
				}
				else drawList->AddRect({ center.x - 5.0f, center.y - 5.0f },
					{ center.x + 5.0f, center.y + 5.0f }, color);
			})) m_display->toggleMaximizeRestore();

		if (drawCaptionButton("##Close", controlsX + captionButtonWidth * 2.0f,
			IM_COL32(196, 43, 54, 255), IM_COL32(160, 32, 42, 255),
			[drawList](ImVec2 center, ImU32 color)
			{
				drawList->AddLine({ center.x - 5.0f, center.y - 5.0f },
					{ center.x + 5.0f, center.y + 5.0f }, color);
				drawList->AddLine({ center.x + 5.0f, center.y - 5.0f },
					{ center.x - 5.0f, center.y + 5.0f }, color);
			})) m_requestEditorClose = true;
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor();

	if (m_requestSceneLoad)
	{
		m_requestSceneLoad = false;
		ImGui::OpenPopup("Unsaved scene");
	}
	if (ImGui::BeginPopupModal(
		"Unsaved scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("The current scene has unsaved changes.");
		ImGui::TextUnformatted("Save before loading another scene?");
		if (ImGui::Button("Save and load"))
		{
			saveScene();
			if (!m_sceneDirty)
			{
				openSceneDialog();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and load"))
		{
			openSceneDialog();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (m_requestEditorClose)
	{
		m_requestEditorClose = false;
		if (m_sceneDirty) ImGui::OpenPopup("Unsaved scene before exit");
		else m_display->close();
	}
	if (ImGui::BeginPopupModal(
		"Unsaved scene before exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("The current scene has unsaved changes.");
		if (ImGui::Button("Save and exit"))
		{
			saveScene();
			if (!m_sceneDirty)
			{
				m_display->close();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and exit"))
		{
			m_display->close();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	const ImVec2 workPosition =
		{ viewport->Pos.x, viewport->Pos.y + titleBarHeight };

	const ImVec2 workSize =
		{ viewport->Size.x, viewport->Size.y - titleBarHeight };

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
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoSavedSettings
	);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspaceId =
		ImGui::GetID("EngineWorkbenchDockspaceV5");

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

			ImGui::DockBuilderDockWindow("Game", center);
			// Dock Scene last so a fresh workbench selects it by default.
			ImGui::DockBuilderDockWindow("Scene", center);
			ImGui::DockBuilderDockWindow("Elements##Workbench", elements);
			ImGui::DockBuilderDockWindow("Console##Workbench", assets);
			ImGui::DockBuilderDockWindow("Asset Lens##Workbench", assets);
			ImGui::DockBuilderDockWindow("Stats##Workbench", signal);
			ImGui::DockBuilderDockWindow("Inspector##Workbench", signal);
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

	ImVec2 sceneViewportOrigin = workPosition;
	ImVec2 sceneViewportSize = workSize;
	bool sceneViewportVisible = false;
	m_sceneViewportHovered = false;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	sceneViewportVisible = ImGui::Begin("Scene", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground);
	if (sceneViewportVisible)
	{
		// Dock siblings share a root window. Exact focus is required so the
		// active Game tab can never authorize Scene-camera navigation.
		m_sceneViewportFocused = ImGui::IsWindowFocused();
		if (m_focusSceneViewRequested && m_sceneViewportFocused)
			m_focusSceneViewRequested = false;
		sceneViewportOrigin = ImGui::GetCursorScreenPos();
		sceneViewportSize = ImGui::GetContentRegionAvail();
		if (ID3D11ShaderResourceView* sceneView =
			m_display->getSwapChain().getSceneFrameView())
			ImGui::Image(sceneView, sceneViewportSize);
		else
			ImGui::Dummy(sceneViewportSize);
		m_sceneViewportHovered = ImGui::IsItemHovered();
		if (m_editorMode == EditorMode::Editing && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DX3D_ASSET_PATH"))
			{
				const char* path = static_cast<const char*>(payload->Data);
				std::string extension = std::filesystem::path(path).extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(),
					[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				if (extension == ".obj") importObjAsset(path, getSceneSpawnPosition());
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			{ 14.0f * m_uiScale, 10.0f * m_uiScale });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			{ 10.0f * m_uiScale, 7.0f * m_uiScale });
		if (m_editorMode == EditorMode::Editing &&
			ImGui::BeginPopupContextItem("##SceneObjectCreation", ImGuiPopupFlags_MouseButtonRight))
		{
			ImGui::TextDisabled("ADD TO SCENE");
			ImGui::Separator();
			drawObjectCreationMenu(getSceneSpawnPosition());
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);
	}
	else
	{
		m_sceneViewportFocused = false;
	}
	ImGui::End();
	ImVec2 gameViewportOrigin = workPosition;
	ImVec2 gameViewportSize = workSize;
	bool gameViewportVisible = false;
	gameViewportVisible = ImGui::Begin("Game", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground);
	if (gameViewportVisible)
	{
		if (m_focusGameViewRequested && ImGui::IsWindowFocused())
			m_focusGameViewRequested = false;
		gameViewportOrigin = ImGui::GetCursorScreenPos();
		gameViewportSize = ImGui::GetContentRegionAvail();
		m_gameViewportScreenArea = {
			static_cast<i32>(gameViewportOrigin.x),
			static_cast<i32>(gameViewportOrigin.y),
			(std::max)(1, static_cast<i32>(gameViewportSize.x)),
			(std::max)(1, static_cast<i32>(gameViewportSize.y))
		};
		if (ID3D11ShaderResourceView* gameView =
			m_display->getSwapChain().getGameFrameView())
			ImGui::Image(gameView, gameViewportSize);
		else
			ImGui::Dummy(gameViewportSize);
		if (ImGui::IsItemHovered() &&
			m_editorMode == EditorMode::Playing &&
			m_inputSystem->isKeyPressed(KeyCode::MouseLeft))
		{
			m_inputSystem->setCursorLockArea(m_gameViewportScreenArea);
			setGameInputCaptured(true);
		}

		// Game owns this dock surface for the frame. Clear the previous Scene
		// input state before MainGame consumes it on the next update.
		m_sceneViewportHovered = false;
		m_sceneViewportFocused = false;
	}
	ImGui::End();
	// Select dock tabs only after both windows have been submitted. This wins
	// over persisted ImGui tab state and DockBuilder's last-docked selection.
	if (m_focusSceneViewRequested)
	{
		ImGui::SetWindowFocus("Scene");
	}
	else if (m_focusGameViewRequested)
	{
		ImGui::SetWindowFocus("Game");
	}
	ImGui::PopStyleVar();

	// Scene and Game expose the same compact workbench controls. The toolbar
	// is shared chrome only; manipulation and navigation remain Scene-owned.
	const auto drawViewportTools = [this](
		const char* windowId, const ImVec2& origin)
	{
	ImGui::SetNextWindowPos(
		{ origin.x + 12.0f * m_uiScale, origin.y + 12.0f * m_uiScale },
		ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.78f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5.0f * m_uiScale, 5.0f * m_uiScale });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4.0f * m_uiScale, 0.0f });
	ImGui::Begin(windowId, nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking);
	const auto drawToolButton = [this](const char* id, bool active, const char* tooltip,
		const std::function<void(ImDrawList*, ImVec2, ImVec2, ImU32)>& drawIcon)
	{
		if (active)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.30f, 0.155f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.46f, 0.36f, 0.18f, 1.0f));
		}
		const bool pressed = ImGui::Button(id, { 30.0f * m_uiScale, 30.0f * m_uiScale });
		const ImVec2 minimum = ImGui::GetItemRectMin();
		const ImVec2 maximum = ImGui::GetItemRectMax();
		const ImU32 color = active ? IM_COL32(245, 194, 105, 255) : IM_COL32(205, 215, 210, 255);
		drawIcon(ImGui::GetWindowDrawList(), minimum, maximum, color);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
		if (active) ImGui::PopStyleColor(2);
		return pressed;
	};
	if (drawToolButton("##MoveTool",
		m_transformGizmo.getOperation() == TransformGizmo::Operation::Translate, "Move [W]",
		[](ImDrawList* list, ImVec2 minimum, ImVec2 maximum, ImU32 color)
		{
			const ImVec2 center{ (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f };
			list->AddLine({ center.x - 8.0f, center.y }, { center.x + 8.0f, center.y }, color, 2.0f);
			list->AddLine({ center.x, center.y - 8.0f }, { center.x, center.y + 8.0f }, color, 2.0f);
			list->AddTriangleFilled({ center.x + 8.0f, center.y }, { center.x + 4.0f, center.y - 3.0f }, { center.x + 4.0f, center.y + 3.0f }, color);
			list->AddTriangleFilled({ center.x, center.y - 8.0f }, { center.x - 3.0f, center.y - 4.0f }, { center.x + 3.0f, center.y - 4.0f }, color);
		})) m_transformGizmo.setOperation(TransformGizmo::Operation::Translate);
	ImGui::SameLine();
	if (drawToolButton("##RotateTool",
		m_transformGizmo.getOperation() == TransformGizmo::Operation::Rotate, "Rotate [E]",
		[](ImDrawList* list, ImVec2 minimum, ImVec2 maximum, ImU32 color)
		{
			const ImVec2 center{ (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f };
			list->AddCircle(center, 8.0f, color, 24, 2.0f);
			list->AddTriangleFilled({ center.x + 8.0f, center.y - 4.0f }, { center.x + 12.0f, center.y - 4.0f }, { center.x + 9.0f, center.y }, color);
		})) m_transformGizmo.setOperation(TransformGizmo::Operation::Rotate);
	ImGui::SameLine();
	if (drawToolButton("##ScaleTool",
		m_transformGizmo.getOperation() == TransformGizmo::Operation::Scale, "Scale [R]",
		[](ImDrawList* list, ImVec2 minimum, ImVec2 maximum, ImU32 color)
		{
			const ImVec2 center{ (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f };
			list->AddRect({ center.x - 7.0f, center.y - 7.0f }, { center.x + 7.0f, center.y + 7.0f }, color, 1.5f, 0, 2.0f);
			list->AddLine({ center.x - 10.0f, center.y + 10.0f }, { center.x - 4.0f, center.y + 4.0f }, color, 2.0f);
			list->AddLine({ center.x + 4.0f, center.y - 4.0f }, { center.x + 10.0f, center.y - 10.0f }, color, 2.0f);
		})) m_transformGizmo.setOperation(TransformGizmo::Operation::Scale);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (m_editorMode == EditorMode::Editing)
	{
		const bool playPressed = ImGui::Button(
			"Play", { 46.0f * m_uiScale, 30.0f * m_uiScale });
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play [F6]");
		if (playPressed) startPlayMode();
	}
	else
	{
		const bool paused = m_editorMode == EditorMode::Paused;
		const bool stopPressed = ImGui::Button(
			"Stop", { 46.0f * m_uiScale, 30.0f * m_uiScale });
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop [F6]");
		ImGui::SameLine();
		const bool pausePressed = ImGui::Button(
			paused ? "Resume" : "Pause", { 58.0f * m_uiScale, 30.0f * m_uiScale });
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s [F7]", paused ? "Resume" : "Pause");
		ImGui::SameLine();
		const bool stepPressed = ImGui::Button(
			"Step", { 46.0f * m_uiScale, 30.0f * m_uiScale });
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step one fixed frame [F8]");

		if (stopPressed) stopPlayMode();
		else if (pausePressed) togglePauseMode();
		else if (stepPressed) stepSimulation();
	}
	ImGui::End();
	ImGui::PopStyleVar(2);
	};
	if (sceneViewportVisible)
		drawViewportTools("##SceneViewportTools", sceneViewportOrigin);
	if (gameViewportVisible)
		drawViewportTools("##GameViewportTools", gameViewportOrigin);

	const float panelWidth = std::clamp(
		workSize.x * 0.245f,
		300.0f * m_uiScale,
		380.0f * m_uiScale
	);
	const float elementsHeight = std::clamp(
		workSize.y * 0.38f,
		210.0f * m_uiScale,
		340.0f * m_uiScale
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

		if (cameraComponent && isEditorCamera(object))
		{
			editorCameraComponent =
				cameraComponent;

			break;
		}
	}

	const TransformGizmo::ViewportArea gizmoViewport
	{
		sceneViewportOrigin.x,
		sceneViewportOrigin.y,
		std::max(1.0f, sceneViewportSize.x),
		std::max(1.0f, sceneViewportSize.y)
	};
	const bool viewportOverlaysAllowed =
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

	if (sceneViewportVisible && viewportOverlaysAllowed &&
		m_editorMode == EditorMode::Editing &&
		m_selectedObject &&
		m_inputSystem->isKeyPressed(KeyCode::MouseLeft) &&
		m_transformGizmo.getHoveredAxis() != TransformGizmo::Axis::None)
	{
		pushUndoSnapshot();
	}

	if (sceneViewportVisible && viewportOverlaysAllowed &&
		m_editorMode == EditorMode::Editing)
	{
		m_transformGizmo.draw(
			m_selectedObject,
			editorCameraComponent,
			gizmoViewport
		);
	}
	else
	{
		// An inactive Scene tab must not retain a drag or paint editor tooling
		// over the clean Game-camera output.
		m_transformGizmo.draw(nullptr, nullptr, gizmoViewport);
	}

	// Authored cameras are scene objects. Draw the same compact camera marker
	// and selected-camera frustum used by enignE's Scene viewport.
	if (sceneViewportVisible && viewportOverlaysAllowed && editorCameraComponent)
	{
		const Mat4x4 viewProjection = editorCameraComponent->getViewMatrix() *
			editorCameraComponent->getProjectionMatrix();
		ImDrawList* overlay = ImGui::GetForegroundDrawList();
		overlay->PushClipRect(
			{ gizmoViewport.x, gizmoViewport.y },
			{ gizmoViewport.x + gizmoViewport.width,
			  gizmoViewport.y + gizmoViewport.height }, true);

		for (auto* object : sceneObjects)
		{
			if (!object || !object->isActiveInHierarchy() || isEditorCamera(object)) continue;
			auto* camera = object->getComponent<CameraComponent>();
			if (!camera) continue;

			ImVec2 icon{};
			if (!projectWorldPoint(object->getTransform().getPosition(),
				viewProjection, gizmoViewport, icon)) continue;

			const bool selected = isObjectSelected(object);
			const ImU32 color = selected
				? IM_COL32(255, 194, 72, 255)
				: camera->isPrimary()
					? IM_COL32(92, 218, 164, 255)
					: IM_COL32(112, 190, 255, 255);
			const float scale = m_uiScale;
			const ImVec2 bodyMin{ icon.x - 10.0f * scale, icon.y - 7.0f * scale };
			const ImVec2 bodyMax{ icon.x + 5.0f * scale, icon.y + 7.0f * scale };
			overlay->AddRect(bodyMin, bodyMax, color, 2.0f * scale, 0,
				selected ? 2.5f * scale : 2.0f * scale);
			overlay->AddTriangle(
				{ bodyMax.x, icon.y - 5.0f * scale },
				{ icon.x + 12.0f * scale, icon.y - 9.0f * scale },
				{ icon.x + 12.0f * scale, icon.y + 9.0f * scale },
				color, selected ? 2.5f * scale : 2.0f * scale);
			overlay->AddCircleFilled(icon, selected ? 2.5f * scale : 2.0f * scale, color);

			if (!selected) continue;
			auto& transform = object->getTransform();
			const Vec3 origin = transform.getPosition();
			const Vec3 forward = transform.forward();
			const Vec3 right = transform.right();
			const Vec3 up = transform.up();
			const float distance = 1.5f;
			const float halfHeight = std::tan(camera->getFieldOfView() * 0.5f) * distance;
			const float halfWidth = halfHeight *
				(std::max(1.0f, sceneViewportSize.x) / std::max(1.0f, sceneViewportSize.y));
			const Vec3 center{
				origin.x + forward.x * distance,
				origin.y + forward.y * distance,
				origin.z + forward.z * distance };
			const std::array<Vec3, 4> corners{{
				{ center.x - right.x * halfWidth - up.x * halfHeight,
				  center.y - right.y * halfWidth - up.y * halfHeight,
				  center.z - right.z * halfWidth - up.z * halfHeight },
				{ center.x + right.x * halfWidth - up.x * halfHeight,
				  center.y + right.y * halfWidth - up.y * halfHeight,
				  center.z + right.z * halfWidth - up.z * halfHeight },
				{ center.x + right.x * halfWidth + up.x * halfHeight,
				  center.y + right.y * halfWidth + up.y * halfHeight,
				  center.z + right.z * halfWidth + up.z * halfHeight },
				{ center.x - right.x * halfWidth + up.x * halfHeight,
				  center.y - right.y * halfWidth + up.y * halfHeight,
				  center.z - right.z * halfWidth + up.z * halfHeight }
			}};
			std::array<ImVec2, 4> projected{};
			bool visible = true;
			for (size_t index = 0; index < corners.size(); ++index)
				visible = projectWorldPoint(corners[index], viewProjection,
					gizmoViewport, projected[index]) && visible;
			if (visible)
			{
				for (const ImVec2& corner : projected)
					overlay->AddLine(icon, corner, color, 1.5f * scale);
				for (size_t index = 0; index < projected.size(); ++index)
					overlay->AddLine(projected[index], projected[(index + 1) % 4],
						color, 1.5f * scale);
			}
		}

		// Lights use a compact bulb marker so non-mesh light entities remain
		// visible and directly selectable in the Scene viewport.
		for (auto* object : sceneObjects)
		{
			if (!object || !object->isActiveInHierarchy()) continue;
			auto* light = object->getComponent<DirectionalLightComponent>();
			if (!light) continue;
			ImVec2 icon{};
			if (!projectWorldPoint(object->getTransform().getPosition(),
				viewProjection, gizmoViewport, icon)) continue;

			const bool selected = isObjectSelected(object);
			const ImU32 color = selected
				? IM_COL32(255, 194, 72, 255)
				: IM_COL32(255, 222, 92, 245);
			const float scale = m_uiScale;
			const float thickness = (selected ? 2.5f : 2.0f) * scale;
			const ImVec2 bulbCenter{ icon.x, icon.y - 2.0f * scale };
			overlay->AddCircle(bulbCenter, 6.0f * scale, color, 20, thickness);
			overlay->AddLine(
				{ icon.x - 4.0f * scale, icon.y + 3.0f * scale },
				{ icon.x - 3.0f * scale, icon.y + 7.0f * scale }, color, thickness);
			overlay->AddLine(
				{ icon.x + 4.0f * scale, icon.y + 3.0f * scale },
				{ icon.x + 3.0f * scale, icon.y + 7.0f * scale }, color, thickness);
			overlay->AddLine(
				{ icon.x - 3.0f * scale, icon.y + 7.0f * scale },
				{ icon.x + 3.0f * scale, icon.y + 7.0f * scale }, color, thickness);
			overlay->AddLine(
				{ icon.x - 2.0f * scale, icon.y + 10.0f * scale },
				{ icon.x + 2.0f * scale, icon.y + 10.0f * scale }, color, thickness);
			for (int ray = 0; ray < 5; ++ray)
			{
				const float angle = -3.14159265f + ray * (3.14159265f / 4.0f);
				const ImVec2 direction{ std::cos(angle), std::sin(angle) };
				overlay->AddLine(
					{ bulbCenter.x + direction.x * 9.0f * scale,
					  bulbCenter.y + direction.y * 9.0f * scale },
					{ bulbCenter.x + direction.x * 12.0f * scale,
					  bulbCenter.y + direction.y * 12.0f * scale }, color, thickness);
			}

			if (!selected) continue;
			const auto drawWorldLine = [&](const Vec3& start, const Vec3& end)
			{
				ImVec2 screenStart{};
				ImVec2 screenEnd{};
				if (projectWorldPoint(start, viewProjection, gizmoViewport, screenStart) &&
					projectWorldPoint(end, viewProjection, gizmoViewport, screenEnd))
				{
					overlay->AddLine(screenStart, screenEnd, color, 1.5f * scale);
				}
			};
			const Vec3 origin = object->getTransform().getPosition();
			const Vec3 forward = object->getTransform().forward();
			const Vec3 right = object->getTransform().right();
			const Vec3 up = object->getTransform().up();
			constexpr int helperSegments = 48;

			if (light->getLightType() == LightType::Point)
			{
				const float range = light->getRange();
				for (int plane = 0; plane < 3; ++plane)
				{
					Vec3 previous{};
					for (int segment = 0; segment <= helperSegments; ++segment)
					{
						const float angle = 6.2831853f * segment / helperSegments;
						const float first = std::cos(angle) * range;
						const float second = std::sin(angle) * range;
						const Vec3 current = plane == 0
							? Vec3{ origin.x + first, origin.y + second, origin.z }
							: plane == 1
								? Vec3{ origin.x + first, origin.y, origin.z + second }
								: Vec3{ origin.x, origin.y + first, origin.z + second };
						if (segment > 0) drawWorldLine(previous, current);
						previous = current;
					}
				}
			}
			else if (light->getLightType() == LightType::Spot)
			{
				const float range = light->getRange();
				const float radius = std::tan(
					light->getSpotAngle() * 0.5f * 3.14159265f / 180.0f) * range;
				const Vec3 coneCenter{
					origin.x + forward.x * range,
					origin.y + forward.y * range,
					origin.z + forward.z * range };
				std::array<Vec3, 4> cardinal{};
				Vec3 previous{};
				for (int segment = 0; segment <= helperSegments; ++segment)
				{
					const float angle = 6.2831853f * segment / helperSegments;
					const float horizontal = std::cos(angle) * radius;
					const float vertical = std::sin(angle) * radius;
					const Vec3 current{
						coneCenter.x + right.x * horizontal + up.x * vertical,
						coneCenter.y + right.y * horizontal + up.y * vertical,
						coneCenter.z + right.z * horizontal + up.z * vertical };
					if (segment > 0) drawWorldLine(previous, current);
					if (segment < helperSegments && segment % (helperSegments / 4) == 0)
						cardinal[segment / (helperSegments / 4)] = current;
					previous = current;
				}
				for (const Vec3& edge : cardinal) drawWorldLine(origin, edge);
			}
			else
			{
				for (int offset = -1; offset <= 1; ++offset)
				{
					const Vec3 start{
						origin.x + right.x * offset * 0.45f,
						origin.y + right.y * offset * 0.45f,
						origin.z + right.z * offset * 0.45f };
					const Vec3 end{
						start.x + forward.x * 2.5f,
						start.y + forward.y * 2.5f,
						start.z + forward.z * 2.5f };
					drawWorldLine(start, end);
				}
			}
		}
		overlay->PopClipRect();
	}

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

	ImGui::Begin("Elements##Workbench");

	const auto objects = m_world->getGameObjects();

	const size_t authoredObjectCount = static_cast<size_t>(std::count_if(
		objects.begin(), objects.end(), [](const GameObject* object)
		{ return object && !isEditorCamera(object); }));
	ImGui::TextUnformatted("Scene");
	ImGui::SameLine();
	ImGui::TextDisabled("%zu elements", authoredObjectCount);
	ImGui::Separator();
	ImGui::BeginDisabled(m_editorMode != EditorMode::Editing);
	if (ImGui::Button("+ Add object", { -1.0f, 0.0f }))
		ImGui::OpenPopup("##HierarchyAddObject");
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
		{ 14.0f * m_uiScale, 10.0f * m_uiScale });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
		{ 10.0f * m_uiScale, 7.0f * m_uiScale });
	if (ImGui::BeginPopup("##HierarchyAddObject"))
	{
		drawObjectCreationMenu({});
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
	ImGui::EndDisabled();
	ImGui::Spacing();

	GameObject* requestedChild = nullptr;
	GameObject* requestedParent = nullptr;
	GameObject* requestedDelete = nullptr;
	GameObject* requestedDuplicate = nullptr;
	bool reparentRequested = false;

	std::function<void(GameObject*)> drawEntity;
	drawEntity = [&](GameObject* object)
	{
		if (!object || isEditorCamera(object)) return;

		const bool hasVisibleChildren = std::any_of(
			object->getChildren().begin(), object->getChildren().end(),
			[](GameObject* child) { return child && !isEditorCamera(child); });
		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_OpenOnArrow;
		if (!hasVisibleChildren)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (isObjectSelected(object))
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::PushID(object);
		const bool activeInHierarchy = object->isActiveInHierarchy();
		if (!activeInHierarchy)
			ImGui::PushStyleColor(ImGuiCol_Text, rgba(125, 123, 118));
		const bool open = ImGui::TreeNodeEx(
			"##Entity", flags, "%s", object->getName().c_str());
		if (!activeInHierarchy)
			ImGui::PopStyleColor();

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			if (ImGui::GetIO().KeyCtrl) toggleObjectSelection(object);
			else selectOnly(object);
		}

		if (m_editorMode == EditorMode::Editing && ImGui::BeginDragDropSource())
		{
			const ui64 entityId = object->getEntityId();
			ImGui::SetDragDropPayload("DX3D_ENTITY_ID", &entityId, sizeof(entityId));
			ImGui::TextUnformatted(object->getName().c_str());
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
			if (ImGui::MenuItem("Move to Root", nullptr, false,
				m_editorMode == EditorMode::Editing && object->getParent() != nullptr))
			{
				requestedChild = object;
				requestedParent = nullptr;
				reparentRequested = true;
			}
			if (ImGui::MenuItem("Delete", "Delete", false,
				m_editorMode == EditorMode::Editing))
			{
				requestedDelete = object;
			}
			ImGui::EndPopup();
		}

		if (hasVisibleChildren && open)
		{
			for (auto* child : object->getChildren()) drawEntity(child);
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	for (auto* object : objects)
		if (object && object->getParent() == nullptr) drawEntity(object);

	ImGui::InvisibleButton("##HierarchyRootTarget",
		{ ImGui::GetContentRegionAvail().x,
		  std::max(24.0f * m_uiScale, ImGui::GetContentRegionAvail().y) });
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

	ImGui::Begin("Inspector##Workbench");

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

		char objectName[256]{};
		std::snprintf(
			objectName,
			sizeof(objectName),
			"%s",
			m_selectedObject->getName().c_str()
		);
		ImGui::BeginDisabled(m_editorMode != EditorMode::Editing);
		bool activeSelf = m_selectedObject->isActiveSelf();
		if (ImGui::Checkbox("##ObjectActive", &activeSelf))
		{
			pushUndoSnapshot(inspectorSnapshot);
			m_selectedObject->setActive(activeSelf);
			m_sceneDirty = true;
		}
		if (ImGui::IsItemHovered())
		{
			if (activeSelf && !m_selectedObject->isActiveInHierarchy())
				ImGui::SetTooltip("Active locally; disabled by an inactive parent");
			else
				ImGui::SetTooltip(activeSelf ? "Deactivate object" : "Activate object");
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-54.0f * m_uiScale);
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
			bool primary = camera->isPrimary();
			if (m_selectedObject->getName() != "Editor Camera")
			{
				const bool primaryChanged = ImGui::Checkbox("Primary", &primary);
				if (primaryChanged)
				{
					pushUndoSnapshot(inspectorSnapshot);
					camera->setPrimary(primary);
				}
			}
			const bool fieldOfViewChanged = ImGui::DragFloat(
				"Field of View", &fieldOfView, 0.01f, 0.1f, 3.0f);
			if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
			if (fieldOfViewChanged)
				camera->setFieldOfView(fieldOfView);
			const bool nearPlaneChanged = ImGui::DragFloat(
				"Near Plane", &nearPlane, 0.01f, 0.001f, farPlane - 0.01f);
			if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
			if (nearPlaneChanged)
				camera->setNearPlane(nearPlane);
			const bool farPlaneChanged = ImGui::DragFloat(
				"Far Plane", &farPlane, 0.5f, nearPlane + 0.01f, 10000.0f);
			if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
			if (farPlaneChanged)
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
			m_selectedObject->getComponent<SphereComponent>() ||
			m_selectedObject->getComponent<CylinderComponent>() ||
			m_selectedObject->getComponent<CapsuleComponent>() ||
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
			ImGui::Separator();
			if (ImGui::Button("Add Component", { -1.0f, 0.0f }))
				ImGui::OpenPopup("##AddComponentPopup");

			if (ImGui::BeginPopup("##AddComponentPopup"))
			{
				bool hasAvailableComponent = false;
				for (const auto& descriptor : ComponentCatalog::descriptors())
				{
					if (ComponentCatalog::has(*m_selectedObject, descriptor.kind)) continue;
					hasAvailableComponent = true;
					ImGui::PushID(static_cast<int>(descriptor.kind));
					if (ImGui::MenuItem(descriptor.name))
					{
						pushUndoSnapshot(inspectorSnapshot);
						ComponentCatalog::addDefault(*m_selectedObject, descriptor.kind);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s component", descriptor.category);
					ImGui::PopID();
				}
				if (!hasAvailableComponent)
					ImGui::TextDisabled("All available components are attached");
				ImGui::EndPopup();
			}
			auto* rigidBody = m_selectedObject->getComponent<RigidBodyComponent>();
			auto* collider = m_selectedObject->getComponent<ColliderComponent>();
			auto* texture = m_selectedObject->getComponent<TextureComponent>();

			if (texture)
			{
				ImGui::SeparatorText("TEXTURE");
				if (ImGui::SmallButton("REMOVE##Texture"))
				{
					pushUndoSnapshot(inspectorSnapshot);
					ComponentCatalog::remove(*m_selectedObject, ComponentKind::Texture);
					texture = nullptr;
				}
			}
			if (texture)
			{
				char texturePath[512]{};
				std::snprintf(texturePath, sizeof(texturePath), "%s", texture->getAssetPath().c_str());
				if (ImGui::InputTextWithHint("Asset", "Drop a texture or enter its path",
					texturePath, sizeof(texturePath)))
				{
					texture->setAssetPath(texturePath);
					m_sceneDirty = true;
				}
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DX3D_ASSET_PATH"))
					{
						const char* path = static_cast<const char*>(payload->Data);
						std::string extension = std::filesystem::path(path).extension().string();
						std::transform(extension.begin(), extension.end(), extension.begin(),
							[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
						if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
							extension == ".bmp" || extension == ".dds" || extension == ".tga")
						{
							pushUndoSnapshot(inspectorSnapshot);
							texture->setAssetPath(path);
							m_sceneDirty = true;
						}
					}
					ImGui::EndDragDropTarget();
				}
				bool enabled = texture->isEnabled();
				if (ImGui::Checkbox("Enabled##Texture", &enabled))
				{
					pushUndoSnapshot(inspectorSnapshot);
					texture->setEnabled(enabled);
					m_sceneDirty = true;
				}
				ImGui::TextDisabled("Loaded and sampled by the DX11 material pipeline.");
			}

			if (rigidBody)
			{
				ImGui::SeparatorText("RIGID BODY");
				if (ImGui::SmallButton("REMOVE##RigidBody"))
				{
					pushUndoSnapshot(inspectorSnapshot);
					ComponentCatalog::remove(*m_selectedObject, ComponentKind::RigidBody);
					rigidBody = nullptr;
				}
			}
			if (rigidBody)
			{
				const char* bodyNames[]{ "Static", "Dynamic", "Kinematic" };
				int bodyType = static_cast<int>(rigidBody->getBodyType());
				const bool bodyChanged = ImGui::Combo("Body Type", &bodyType, bodyNames, IM_ARRAYSIZE(bodyNames));
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (bodyChanged) rigidBody->setBodyType(static_cast<RigidBodyType>(bodyType));

				float friction = rigidBody->getFriction();
				const bool frictionChanged = ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (frictionChanged) rigidBody->setFriction(friction);
				float restitution = rigidBody->getRestitution();
				const bool restitutionChanged = ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (restitutionChanged) rigidBody->setRestitution(restitution);
				float linearDamping = rigidBody->getLinearDamping();
				const bool linearDampingChanged = ImGui::DragFloat(
					"Linear Damping", &linearDamping, 0.01f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (linearDampingChanged)
					rigidBody->setLinearDamping(linearDamping);
				float angularDamping = rigidBody->getAngularDamping();
				const bool angularDampingChanged = ImGui::DragFloat(
					"Angular Damping", &angularDamping, 0.01f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (angularDampingChanged)
					rigidBody->setAngularDamping(angularDamping);
				float gravityFactor = rigidBody->getGravityFactor();
				const bool gravityChanged = ImGui::DragFloat("Gravity Factor", &gravityFactor, 0.01f, -10.0f, 10.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (gravityChanged) rigidBody->setGravityFactor(gravityFactor);
				bool enabled = rigidBody->isEnabled();
				if (ImGui::Checkbox("Enabled", &enabled))
				{
					pushUndoSnapshot(inspectorSnapshot);
					rigidBody->setEnabled(enabled);
				}
			}

			if (collider)
			{
				ImGui::SeparatorText("COLLIDER");
				if (ImGui::SmallButton("REMOVE##Collider"))
				{
					pushUndoSnapshot(inspectorSnapshot);
					ComponentCatalog::remove(*m_selectedObject, ComponentKind::Collider);
					collider = nullptr;
				}
			}
			if (collider)
			{
				const char* shapeNames[]{ "Box", "Sphere", "Cylinder", "Capsule" };
				int shape = static_cast<int>(collider->getShape());
				const bool shapeChanged = ImGui::Combo(
					"Shape", &shape, shapeNames, IM_ARRAYSIZE(shapeNames));
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (shapeChanged)
					collider->setShape(static_cast<ColliderShape>(shape));
				if (collider->getShape() == ColliderShape::Box)
				{
					Vec3 extent = collider->getHalfExtents();
					float values[3]{ extent.x, extent.y, extent.z };
					const bool extentChanged = ImGui::DragFloat3(
						"Half Extents", values, 0.02f, 0.001f, 1000.0f);
					if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
					if (extentChanged)
						collider->setHalfExtents({ values[0], values[1], values[2] });
				}
				else
				{
					float radius = collider->getRadius();
					const bool radiusChanged = ImGui::DragFloat(
						"Radius", &radius, 0.02f, 0.001f, 1000.0f);
					if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
					if (radiusChanged)
						collider->setRadius(radius);

					if (collider->getShape() == ColliderShape::Cylinder ||
						collider->getShape() == ColliderShape::Capsule)
					{
						Vec3 extent = collider->getHalfExtents();
						float halfHeight = extent.y;
						const bool heightChanged = ImGui::DragFloat(
							"Half Height", &halfHeight, 0.02f, 0.001f, 1000.0f);
						if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
						if (heightChanged)
							collider->setHalfExtents({ extent.x, halfHeight, extent.z });
					}
				}
			}

			auto* rotator = m_selectedObject->getComponent<RotatorComponent>();
			if (rotator)
			{
				ImGui::SeparatorText("ROTATOR");
				if (ImGui::SmallButton("REMOVE##Rotator"))
				{
					pushUndoSnapshot(inspectorSnapshot);
					ComponentCatalog::remove(*m_selectedObject, ComponentKind::Rotator);
					rotator = nullptr;
				}
			}
			if (rotator)
			{
				Vec3 velocity = rotator->getAngularVelocity();
				float values[3]{ velocity.x, velocity.y, velocity.z };
				const bool changed = ImGui::DragFloat3("Angular Velocity", values, 0.01f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (changed) rotator->setAngularVelocity({ values[0], values[1], values[2] });
				bool enabled = rotator->isEnabled();
				if (ImGui::Checkbox("Enabled##Rotator", &enabled))
				{
					pushUndoSnapshot(inspectorSnapshot);
					rotator->setEnabled(enabled);
				}
			}

			auto* fly = m_selectedObject->getComponent<FlyControllerComponent>();
			if (fly)
			{
				ImGui::SeparatorText("FLY CONTROLLER");
				if (ImGui::SmallButton("REMOVE##FlyController"))
				{
					pushUndoSnapshot(inspectorSnapshot);
					ComponentCatalog::remove(*m_selectedObject, ComponentKind::FlyController);
					fly = nullptr;
				}
			}
			if (fly)
			{
				float moveSpeed = fly->getMoveSpeed();
				const bool moveSpeedChanged = ImGui::DragFloat(
					"Move Speed", &moveSpeed, 0.1f, 0.0f, 1000.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (moveSpeedChanged) fly->setMoveSpeed(moveSpeed);
				float sensitivity = fly->getLookSensitivity();
				const bool sensitivityChanged = ImGui::DragFloat(
					"Look Sensitivity", &sensitivity, 0.0001f, 0.0f, 1.0f, "%.4f");
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (sensitivityChanged) fly->setLookSensitivity(sensitivity);
				float boost = fly->getBoostMultiplier();
				const bool boostChanged = ImGui::DragFloat(
					"Boost Multiplier", &boost, 0.1f, 1.0f, 100.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (boostChanged) fly->setBoostMultiplier(boost);
				float pitchLimit = fly->getPitchLimitDegrees();
				const bool pitchLimitChanged = ImGui::SliderFloat(
					"Pitch Limit", &pitchLimit, 1.0f, 90.0f, "%.1f deg");
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (pitchLimitChanged) fly->setPitchLimitDegrees(pitchLimit);
				bool enabled = fly->isEnabled();
				if (ImGui::Checkbox("Enabled##FlyController", &enabled))
				{
					pushUndoSnapshot(inspectorSnapshot);
					fly->setEnabled(enabled);
				}
			}
		}

		if (auto* directionalLight =
			m_selectedObject->getComponent<
			DirectionalLightComponent
			>())
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Light");
			ImGui::Spacing();
			int lightType = static_cast<int>(directionalLight->getLightType());
			const char* lightTypes[] = { "Directional", "Point", "Spot" };
			if (ImGui::Combo("Mode", &lightType, lightTypes, 3))
			{
				pushUndoSnapshot(inspectorSnapshot);
				directionalLight->setLightType(static_cast<LightType>(lightType));
			}

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

			if (directionalLight->getLightType() != LightType::Directional)
			{
				float range = directionalLight->getRange();
				const bool rangeChanged = ImGui::DragFloat(
					"Range", &range, 0.1f, 0.1f, 10000.0f);
				if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
				if (rangeChanged)
					directionalLight->setRange(range);
				if (directionalLight->getLightType() == LightType::Spot)
				{
					float angle = directionalLight->getSpotAngle();
					const bool angleChanged = ImGui::SliderFloat(
						"Spot Angle", &angle, 1.0f, 179.0f, "%.1f deg");
					if (ImGui::IsItemActivated()) pushUndoSnapshot(inspectorSnapshot);
					if (angleChanged)
						directionalLight->setSpotAngle(angle);
				}
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

			if (directionalLight->getLightType() == LightType::Directional)
			{
				float shadowArea = directionalLight->getShadowArea();
				const bool shadowAreaChanged = ImGui::DragFloat(
					"Shadow Area", &shadowArea, 0.5f, 1.0f, 200.0f);
				if (ImGui::IsItemActivated())
					pushUndoSnapshot(inspectorSnapshot);
				if (shadowAreaChanged)
					directionalLight->setShadowArea(shadowArea);
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

			const char* shadowMode =
				directionalLight->getLightType() == LightType::Point
				? "Cube depth / 6 faces / 1024"
				: directionalLight->getLightType() == LightType::Spot
					? "Perspective depth / 2048"
					: "Orthographic depth / 2048";
			ImGui::TextDisabled("SHADOW MODE  /  %s", shadowMode);
			ImGui::TextDisabled("First active shadow-casting light drives the shadow pass.");
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

		if (ImGui::Begin("Stats##Workbench", &m_showStats))
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

		if (ImGui::Begin("Asset Lens##Workbench"))
		{
			ImGui::SetNextItemWidth(-82.0f);
			ImGui::InputTextWithHint(
				"##AssetFilter", "Filter project assets...",
				m_assetFilter, sizeof(m_assetFilter)
			);
			ImGui::SameLine();
			if (ImGui::Button("REFRESH"))
				refreshAssetLens();

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

				auto assetType = [](std::string extension)
				{
					std::transform(extension.begin(), extension.end(), extension.begin(),
						[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
					if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" ||
						extension == ".glb" || extension == ".dae" || extension == ".3ds" ||
						extension == ".stl" || extension == ".ply" || extension == ".blend") return "MODEL";
					if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
						extension == ".tga" || extension == ".bmp" || extension == ".dds" ||
						extension == ".hdr" || extension == ".exr") return "TEX";
					if (extension == ".ematerial" || extension == ".mtl") return "MAT";
					if (extension == ".eprefab") return "PREFAB";
					if (extension == ".hlsl") return "SHADER";
					return "FILE";
				};

				for (const auto& path : m_assetPaths)
				{
					if (!containsFilter(path)) continue;
					std::string extension =
						std::filesystem::path(path).extension().string();
					std::transform(extension.begin(), extension.end(), extension.begin(),
						[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextColored(rgba(150, 190, 176), "%s", assetType(extension));
					ImGui::TableNextColumn();
					const bool selectedAsset = ImGui::Selectable(path.c_str(), false);
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
					{
						ImGui::SetDragDropPayload("DX3D_ASSET_PATH", path.c_str(), path.size() + 1);
						ImGui::TextUnformatted(path.c_str());
						ImGui::EndDragDropSource();
					}
					if (selectedAsset && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
						extension == ".obj" && m_editorMode == EditorMode::Editing)
						importObjAsset(path, getSceneSpawnPosition());
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (m_showConsole)
	{
		if (ImGuiWindow* assetsWindow = ImGui::FindWindowByName("Asset Lens##Workbench");
			assetsWindow && assetsWindow->DockId != 0)
			ImGui::SetNextWindowDockID(assetsWindow->DockId, ImGuiCond_FirstUseEver);
		ImGui::Begin("Console##Workbench", &m_showConsole);
		ImGui::SetNextItemWidth(-190.0f * m_uiScale);
		ImGui::InputTextWithHint("##ConsoleFilter", "Filter log messages...",
			m_consoleFilter, sizeof(m_consoleFilter));
		ImGui::SameLine();
		ImGui::Checkbox("Follow", &m_consoleAutoScroll);
		ImGui::SameLine();
		if (ImGui::Button("Clear")) m_logger->clear();
		ImGui::Separator();
		const std::string filter = m_consoleFilter;
		const auto entries = m_logger->getEntries();
		ImGui::BeginChild("##ConsoleRows", { 0.0f, 0.0f }, false,
			ImGuiWindowFlags_HorizontalScrollbar);
		for (const auto& entry : entries)
		{
			if (!filter.empty() && entry.message.find(filter) == std::string::npos) continue;
			const char* level = "INFO";
			ImVec4 color = rgba(158, 188, 180);
			if (entry.level == Logger::LogLevel::Warning)
			{
				level = "WARN";
				color = rgba(226, 174, 85);
			}
			else if (entry.level == Logger::LogLevel::Error)
			{
				level = "ERROR";
				color = rgba(225, 102, 92);
			}
			ImGui::TextColored(color, "%s", level);
			ImGui::SameLine(62.0f * m_uiScale);
			ImGui::TextUnformatted(entry.message.c_str());
		}
		if (m_consoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::End();
	}

	if (m_focusAssetLensRequested && m_showAssetLens)
	{
		ImGuiWindow* assetWindow = ImGui::FindWindowByName("Asset Lens##Workbench");
		ImGuiWindow* sceneWindow = ImGui::FindWindowByName("Scene");
		if (assetWindow)
		{
			ImGui::FocusWindow(assetWindow);
			if (sceneWindow) ImGui::FocusWindow(sceneWindow);
			m_focusAssetLensRequested = false;
		}
	}

	if (sceneViewportVisible && viewportOverlaysAllowed)
	{
		handleViewportPicking(
			editorCameraComponent,
			gizmoViewport
		);
	}

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
		sceneViewportVisible &&
		m_sceneViewportFocused &&
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
		!m_gameInputCaptured &&
		controlHeld &&
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen;

	if (allowClipboardShortcuts)
	{
		if (m_inputSystem->isKeyPressed(KeyCode::S))
		{
			saveScene();
		}
		else if (m_inputSystem->isKeyPressed(KeyCode::O))
		{
			loadScene();
		}
		else if (m_inputSystem->isKeyPressed(KeyCode::Z))
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

	const bool deletePressed =
		m_inputSystem->isKeyPressed(KeyCode::Delete);

	const bool editingInspectorValue =
		ImGui::IsAnyItemActive();

	const bool typingText =
		ImGui::GetIO().WantTextInput;

	if (m_editorMode == EditorMode::Editing &&
		!m_gameInputCaptured && deletePressed &&
		!m_transformGizmo.isUsing() &&
		!editingInspectorValue &&
		!typingText &&
		m_selectedObject &&
		!isEditorCamera(m_selectedObject))
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

	ImGui::Render();

	m_graphicsDevice->bindBackBuffer(
		m_display->getSwapChain()
	);

	ImGui_ImplDX11_RenderDrawData(
		ImGui::GetDrawData()
	);

	m_display->getSwapChain().present();
}
