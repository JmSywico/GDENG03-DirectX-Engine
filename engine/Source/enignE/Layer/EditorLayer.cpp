#include "Layer/EditorLayer.h"

#include "Editor/Commands/EditorCommand.h"
#include "Editor/PrefabAsset.h"
#include "Editor/UI/UIScale.h"
#include "Graphics/Mesh.h"
#include "Graphics/Model.h"
#include "Input/Input.h"
#include "Scene/Components/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneFactory.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <utility>

namespace
{
	constexpr std::size_t MaxGpuLights = 16;
	using namespace DirectX;
	using namespace enignE::Scene;

	constexpr float GizmoLength = 1.5f;
	constexpr float GizmoHitDistance = 8.0f;

	float DistanceToSegment(const ImVec2 point, const ImVec2 start, const ImVec2 end)
	{
		const ImVec2 segment{end.x - start.x, end.y - start.y};
		const ImVec2 relative{point.x - start.x, point.y - start.y};
		const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
		const float t = lengthSquared > 0.0f
			? std::clamp((relative.x * segment.x + relative.y * segment.y) / lengthSquared, 0.0f, 1.0f)
			: 0.0f;
		const float dx = point.x - (start.x + segment.x * t);
		const float dy = point.y - (start.y + segment.y * t);
		return std::sqrt(dx * dx + dy * dy);
	}

	bool ProjectPoint(
		const XMFLOAT3& point,
		const XMFLOAT4X4& view,
		const XMFLOAT4X4& projection,
		const XMUINT2 viewport,
		ImVec2& result)
	{
		const XMVECTOR projected = XMVector3Project(
			XMLoadFloat3(&point),
			0.0f,
			0.0f,
			static_cast<float>(viewport.x),
			static_cast<float>(viewport.y),
			0.0f,
			1.0f,
			XMLoadFloat4x4(&projection),
			XMLoadFloat4x4(&view),
			XMMatrixIdentity());
		XMFLOAT3 screen{};
		XMStoreFloat3(&screen, projected);
		if (!std::isfinite(screen.x) || !std::isfinite(screen.y) || screen.z < 0.0f || screen.z > 1.0f)
			return false;
		result = {screen.x, screen.y};
		return true;
	}

	bool ProjectClippedLine(
		const XMFLOAT3& start,
		const XMFLOAT3& end,
		const XMFLOAT4X4& view,
		const XMFLOAT4X4& projection,
		const XMUINT2 viewport,
		ImVec2& screenStart,
		ImVec2& screenEnd)
	{
		const XMMATRIX viewProjection = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);
		XMVECTOR clipStart = XMVector4Transform(
			XMVectorSet(start.x, start.y, start.z, 1.0f),
			viewProjection);
		XMVECTOR clipEnd = XMVector4Transform(
			XMVectorSet(end.x, end.y, end.z, 1.0f),
			viewProjection);

		XMFLOAT4 first{};
		XMFLOAT4 second{};
		XMStoreFloat4(&first, clipStart);
		XMStoreFloat4(&second, clipEnd);
		float minimum = 0.0f;
		float maximum = 1.0f;

		const auto clipAgainstPlane = [&minimum, &maximum](const float firstDistance, const float secondDistance)
		{
			if (firstDistance < 0.0f && secondDistance < 0.0f)
				return false;
			if (firstDistance >= 0.0f && secondDistance >= 0.0f)
				return true;

			const float intersection = firstDistance / (firstDistance - secondDistance);
			if (firstDistance < 0.0f)
				minimum = std::max(minimum, intersection);
			else
				maximum = std::min(maximum, intersection);
			return minimum <= maximum;
		};

		if (!clipAgainstPlane(first.x + first.w, second.x + second.w)
			|| !clipAgainstPlane(first.w - first.x, second.w - second.x)
			|| !clipAgainstPlane(first.y + first.w, second.y + second.w)
			|| !clipAgainstPlane(first.w - first.y, second.w - second.y)
			|| !clipAgainstPlane(first.z, second.z))
		{
			return false;
		}

		clipStart = XMVectorLerp(clipStart, clipEnd, minimum);
		clipEnd = XMVectorLerp(clipStart, clipEnd, (maximum - minimum) / std::max(1.0f - minimum, 1.0e-6f));
		XMStoreFloat4(&first, clipStart);
		XMStoreFloat4(&second, clipEnd);
		if (std::abs(first.w) < 1.0e-6f || std::abs(second.w) < 1.0e-6f)
			return false;

		const auto toScreen = [viewport](const XMFLOAT4& clip)
		{
			const float normalizedX = clip.x / clip.w;
			const float normalizedY = clip.y / clip.w;
			return ImVec2{
				(normalizedX * 0.5f + 0.5f) * static_cast<float>(viewport.x),
				(0.5f - normalizedY * 0.5f) * static_cast<float>(viewport.y)
			};
		};
		screenStart = toScreen(first);
		screenEnd = toScreen(second);
		return std::isfinite(screenStart.x) && std::isfinite(screenStart.y)
			&& std::isfinite(screenEnd.x) && std::isfinite(screenEnd.y);
	}

	bool RayIntersectsAabb(
		const XMVECTOR rayOrigin,
		const XMVECTOR rayDirection,
		const XMFLOAT3& boundsMin,
		const XMFLOAT3& boundsMax,
		float& distance)
	{
		XMFLOAT3 origin{};
		XMFLOAT3 direction{};
		XMStoreFloat3(&origin, rayOrigin);
		XMStoreFloat3(&direction, rayDirection);
		float nearDistance = 0.0f;
		float farDistance = std::numeric_limits<float>::max();

		for (int axis = 0; axis < 3; ++axis)
		{
			const float o = (&origin.x)[axis];
			const float d = (&direction.x)[axis];
			const float minimum = (&boundsMin.x)[axis];
			const float maximum = (&boundsMax.x)[axis];
			if (std::abs(d) < 1.0e-6f)
			{
				if (o < minimum || o > maximum)
					return false;
				continue;
			}

			float first = (minimum - o) / d;
			float second = (maximum - o) / d;
			if (first > second)
				std::swap(first, second);
			nearDistance = std::max(nearDistance, first);
			farDistance = std::min(farDistance, second);
			if (nearDistance > farDistance)
				return false;
		}

		distance = nearDistance;
		return true;
	}

	bool GetWorldBounds(
		const MeshRendererComponent& renderer,
		const TransformComponent& transform,
		XMFLOAT3& minimum,
		XMFLOAT3& maximum)
	{
		if (!renderer.ModelPtr || renderer.ModelPtr->GetMeshes().empty())
			return false;

		const XMMATRIX world = transform.GetWorldMatrix();
		XMVECTOR worldMinimum = XMVectorReplicate(std::numeric_limits<float>::max());
		XMVECTOR worldMaximum = XMVectorReplicate(-std::numeric_limits<float>::max());
		for (const auto& mesh : renderer.ModelPtr->GetMeshes())
		{
			if (!mesh)
				continue;
			const XMFLOAT3& meshMin = mesh->GetBoundsMin();
			const XMFLOAT3& meshMax = mesh->GetBoundsMax();
			for (int corner = 0; corner < 8; ++corner)
			{
				const XMVECTOR local = XMVectorSet(
					(corner & 1) ? meshMax.x : meshMin.x,
					(corner & 2) ? meshMax.y : meshMin.y,
					(corner & 4) ? meshMax.z : meshMin.z,
					1.0f);
				const XMVECTOR transformed = XMVector3TransformCoord(local, world);
				worldMinimum = XMVectorMin(worldMinimum, transformed);
				worldMaximum = XMVectorMax(worldMaximum, transformed);
			}
		}
		XMStoreFloat3(&minimum, worldMinimum);
		XMStoreFloat3(&maximum, worldMaximum);
		return true;
	}

	const char* PrimitiveName(enignE::Scene::PrimitiveType type)
	{
		switch (type)
		{
		case enignE::Scene::PrimitiveType::Cube: return "Cube";
		case enignE::Scene::PrimitiveType::Rectangle: return "Rectangle";
		case enignE::Scene::PrimitiveType::Pyramid: return "Pyramid";
		case enignE::Scene::PrimitiveType::Plane: return "Plane";
		case enignE::Scene::PrimitiveType::Sphere: return "Sphere";
		default: return "Primitive";
		}
	}

	XMFLOAT3 ScenePointFromViewport(
		const XMFLOAT2& mousePosition,
		const XMUINT2& viewport,
		const XMFLOAT4X4& viewMatrix,
		const XMFLOAT4X4& projectionMatrix)
	{
		if (viewport.x == 0 || viewport.y == 0)
			return {};

		const XMMATRIX projection = XMLoadFloat4x4(&projectionMatrix);
		const XMMATRIX view = XMLoadFloat4x4(&viewMatrix);
		const XMVECTOR nearPoint = XMVector3Unproject(
			XMVectorSet(mousePosition.x, mousePosition.y, 0.0f, 1.0f),
			0.0f, 0.0f, static_cast<float>(viewport.x), static_cast<float>(viewport.y), 0.0f, 1.0f,
			projection, view, XMMatrixIdentity());
		const XMVECTOR farPoint = XMVector3Unproject(
			XMVectorSet(mousePosition.x, mousePosition.y, 1.0f, 1.0f),
			0.0f, 0.0f, static_cast<float>(viewport.x), static_cast<float>(viewport.y), 0.0f, 1.0f,
			projection, view, XMMatrixIdentity());
		const XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));

		const float originY = XMVectorGetY(nearPoint);
		const float directionY = XMVectorGetY(direction);
		const float planeDistance = std::abs(directionY) > 1.0e-5f ? -originY / directionY : 5.0f;
		const XMVECTOR world = XMVectorAdd(
			nearPoint,
			XMVectorScale(direction, planeDistance > 0.0f ? planeDistance : 5.0f));

		XMFLOAT3 result{};
		XMStoreFloat3(&result, world);
		return result;
	}
}

EditorLayer::EditorLayer(
	enignE::Scene::Scene& scene,
	SceneProvider activeSceneProvider,
	MatrixProvider viewProvider,
	MatrixProvider projectionProvider,
	ViewportProvider viewportProvider,
	SceneChangedCallback sceneChangedCallback,
	TextureProvider sceneTextureProvider,
	TextureProvider gameTextureProvider,
	ViewportResizeCallback resizeSceneViewport,
	ViewportResizeCallback resizeGameViewport,
	ViewportInteractionCallback sceneViewportInteraction,
	ViewportStateCallback viewportStateCallback,
	SceneAction saveScene,
	SceneAction loadScene,
	PrimitiveModelFactory createPrimitiveModel,
	TextureLoader loadTexture,
	ModelLoader loadModel,
	enignE::Graphics::AssetRegistry* assets,
	std::filesystem::path assetDirectory,
	SimulationStateProvider simulationStateProvider,
	SimulationAction beginPlay,
	SimulationAction stopPlay,
	SimulationAction togglePause,
	SimulationAction stepSimulation,
	GameInputCaptureAction setGameInputCaptured,
	WindowStateProvider isGameInputCaptured,
	WindowAction minimizeWindow,
	WindowAction toggleMaximizeWindow,
	WindowAction closeWindow,
	WindowAction beginTitleBarDrag,
	WindowStateProvider isWindowMaximized)
	: Layer("EditorLayer")
	, m_context(
		scene,
		std::move(viewProvider),
		std::move(projectionProvider),
		std::move(viewportProvider),
		std::move(sceneChangedCallback),
		std::move(minimizeWindow),
		std::move(toggleMaximizeWindow),
		std::move(closeWindow),
		std::move(beginTitleBarDrag),
		std::move(isWindowMaximized))
	, m_editorScene(&scene)
	, m_activeSceneProvider(std::move(activeSceneProvider))
	, m_sceneTextureProvider(std::move(sceneTextureProvider))
	, m_gameTextureProvider(std::move(gameTextureProvider))
	, m_resizeSceneViewport(std::move(resizeSceneViewport))
	, m_resizeGameViewport(std::move(resizeGameViewport))
	, m_sceneViewportInteraction(std::move(sceneViewportInteraction))
	, m_viewportStateCallback(std::move(viewportStateCallback))
{
	m_context.Commands = &m_commandStack;
	m_context.SaveScene = std::move(saveScene);
	m_context.LoadScene = [this, action = std::move(loadScene)]()
	{
		return QueueEditorOperation(
			"OPENING SCENE",
			"Parsing registry data and resolving resources",
			action,
			[this](bool succeeded)
			{
				if (!succeeded) return;
				m_context.ClearSelection();
				m_commandStack.Clear();
			});
	};
	m_context.CreatePrimitiveModel = std::move(createPrimitiveModel);
	m_context.LoadTexture = std::move(loadTexture);
	m_context.LoadModel = std::move(loadModel);
	m_context.Assets = assets;
	m_context.AssetDirectory = std::move(assetDirectory);
	m_context.GetSimulationState = std::move(simulationStateProvider);
	m_context.BeginPlay = [this, action = std::move(beginPlay)]()
	{
		std::vector<std::uint64_t> selectedIDs;
		for (const entt::entity entity : m_context.GetSelectedEntities())
			if (const std::uint64_t id = m_context.ActiveScene->GetEntityID(entity); id != 0)
				selectedIDs.push_back(id);
		return QueueEditorOperation(
			"BUILDING PLAY SCENE",
			"Cloning the registry and preparing runtime state",
			action,
			[this, selectedIDs = std::move(selectedIDs)](bool succeeded) mutable
			{
				if (!succeeded) return;
				SynchronizeActiveScene(&selectedIDs);
				m_focusGameViewportRequested = true;
				if (m_context.SetGameInputCaptured)
					m_context.SetGameInputCaptured(true);
			});
	};
	m_context.StopPlay = [this, action = std::move(stopPlay)]()
	{
		std::vector<std::uint64_t> selectedIDs;
		for (const entt::entity entity : m_context.GetSelectedEntities())
			if (const std::uint64_t id = m_context.ActiveScene->GetEntityID(entity); id != 0)
				selectedIDs.push_back(id);
		const bool result = action && action();
		if (result)
		{
			SynchronizeActiveScene(&selectedIDs);
			m_focusSceneViewportRequested = true;
		}
		return result;
	};
	m_context.TogglePause = std::move(togglePause);
	m_context.StepSimulation = std::move(stepSimulation);
	m_context.SetGameInputCaptured = std::move(setGameInputCaptured);
	m_context.IsGameInputCaptured = std::move(isGameInputCaptured);
	m_context.InstantiatePrefab = [this](
		std::uint64_t handle,
		const std::string& path,
		std::uint64_t requestedParentID,
		const DirectX::XMFLOAT3* rootPosition)
		{
			if (!m_context.ActiveScene || !m_context.Assets) return false;
			enignE::Editor::PrefabAsset prefab;
			if (!LoadPrefab(path, prefab)) return false;
		const std::uint64_t parentID = requestedParentID;
		auto command = std::make_unique<enignE::Editor::InstantiatePrefabCommand>(
			*m_context.ActiveScene, std::move(prefab), handle, path, parentID,
			rootPosition ? std::optional<DirectX::XMFLOAT3>(*rootPosition) : std::nullopt);
		auto* commandResult = command.get();
		m_commandStack.Execute(std::move(command));
		m_context.SelectEntity(m_context.ActiveScene->FindEntityByID(commandResult->GetEntityID()));
			return commandResult->GetEntityID() != 0;
		};
	m_context.InstantiateModel = [this](
		std::uint64_t handle,
		const std::string& path,
		std::uint64_t parentID,
		const DirectX::XMFLOAT3* position)
	{
		const entt::entity entity = CreateModelAssetEntity(handle, path, parentID, position);
		if (entity == entt::null) return false;
		const enignE::Editor::EntitySnapshot snapshot =
			enignE::Editor::EntitySnapshot::CaptureSubtree(*m_context.ActiveScene, entity);
		m_commandStack.RecordExecuted(std::make_unique<enignE::Editor::AdoptEntityCommand>(
			*m_context.ActiveScene, snapshot));
		m_context.SelectEntity(entity);
		return true;
	};
	m_context.SaveAsPrefab = [this](entt::entity root)
	{
		if (!m_context.ActiveScene || !m_context.Assets
			|| !m_context.ActiveScene->IsEntityValid(root)) return false;
		std::string name = "Prefab";
		if (const auto* tag = m_context.ActiveScene->GetComponent<enignE::Scene::TagComponent>(root))
			name = tag->Tag;
		for (char& character : name)
			if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_')
				character = '_';
		const std::filesystem::path path = m_context.AssetDirectory / (name + ".eprefab");
		std::string error;
		if (!enignE::Editor::PrefabAsset::SaveFromScene(
			*m_context.ActiveScene, root, path, &error,
			[this](const std::string& assetPath)
			{
				return m_context.Assets->MakeProjectRelative(assetPath);
			}))
		{
			LOG_ERRORF("Unable to save prefab: {}", error);
			return false;
		}
		m_context.Assets->Register(path, enignE::Graphics::AssetType::Prefab);
		m_context.Assets->DiscoverAssets(m_context.AssetDirectory);
		return true;
	};
	m_context.CountPrefabReferences = [this](std::uint64_t handle)
	{
		if (!m_context.ActiveScene) return std::size_t{0};
		std::size_t count = 0;
		for (const entt::entity entity : m_context.ActiveScene->GetAllEntities())
		{
			const auto* prefab = m_context.ActiveScene->GetComponent<
				enignE::Scene::PrefabInstanceComponent>(entity);
			if (prefab && prefab->PrefabAssetHandle == handle) ++count;
		}
		return count;
	};
	m_context.DeleteAsset = [this](std::uint64_t handle, const std::string& path)
	{
		if (!m_context.Assets || !std::filesystem::is_regular_file(
			m_context.Assets->ResolvePath(path))) return false;
		const enignE::Graphics::AssetReference asset = m_context.Assets->GetReference(handle);
		if (!asset.IsValid()) return false;
		m_commandStack.Execute(std::make_unique<enignE::Editor::DeleteAssetCommand>(
			*m_context.Assets, asset));
		return !std::filesystem::exists(m_context.Assets->ResolvePath(path));
	};
	if (m_context.Assets) m_context.Assets->DiscoverAssets(m_context.AssetDirectory);
	m_context.CopySelection = [this]() { CopySelection(); };
	m_context.PasteSelection = [this]() { PasteSelection(); };
}

void EditorLayer::OnEvent(Event& event)
{
	if (m_pendingEditorAction)
	{
		event.Handled = true;
		return;
	}
	if (event.Type != EventType::KeyDown)
		return;
	const auto* keyEvent = dynamic_cast<KeyEvent*>(&event);
	if (!keyEvent || keyEvent->Repeat || InputManager::Get().IsMouseButtonDown(2))
		return;
	const bool gameInputCaptured = m_context.IsGameInputCaptured
		&& m_context.IsGameInputCaptured();
	if (keyEvent->Key == VK_F6 && m_context.GetSimulationState)
	{
		const auto state = m_context.GetSimulationState();
		if (state == enignE::Editor::SimulationState::Stopped)
		{
			if (m_context.BeginPlay) m_context.BeginPlay();
		}
		else if (m_context.StopPlay)
			m_context.StopPlay();
		event.Handled = true;
		return;
	}
	if (gameInputCaptured)
		return;

	const bool controlDown = InputManager::Get().IsKeyDown(VK_CONTROL);
	if (controlDown && keyEvent->Key == 'Z')
	{
		m_commandStack.Undo();
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'Y')
	{
		m_commandStack.Redo();
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'S' && m_context.SaveScene)
	{
		if (m_context.SaveScene())
			m_commandStack.MarkSaved();
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'O' && m_context.LoadScene)
	{
		m_context.RequestSceneLoad = true;
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'D')
	{
		DuplicateSelection();
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'C')
	{
		CopySelection();
		event.Handled = true;
		return;
	}
	if (controlDown && keyEvent->Key == 'V')
	{
		PasteSelection();
		event.Handled = true;
		return;
	}

	switch (keyEvent->Key)
	{
	case 'W':
		m_context.CurrentGizmoMode = enignE::Editor::GizmoMode::Translate;
		break;
	case 'E':
		m_context.CurrentGizmoMode = enignE::Editor::GizmoMode::Rotate;
		break;
	case 'R':
		m_context.CurrentGizmoMode = enignE::Editor::GizmoMode::Scale;
		break;
	case VK_DELETE:
		DeleteSelection();
		break;
	default:
		break;
	}
}

bool EditorLayer::QueueEditorOperation(
	const char* title,
	const char* detail,
	std::function<bool()> action,
	std::function<void(bool)> completion)
{
	if (!action || m_pendingEditorAction)
		return false;
	m_pendingEditorTitle = title ? title : "WORKING";
	m_pendingEditorDetail = detail ? detail : "Preparing editor state";
	m_pendingEditorAction = std::move(action);
	m_pendingEditorCompletion = std::move(completion);
	if (m_loadingWindow.Start(m_pendingEditorTitle.c_str(), m_pendingEditorDetail.c_str()))
		LOG_DEBUGF("Presented editor loading window: {}", m_pendingEditorTitle);
	else
		LOG_WARN("Editor loading window could not be created");
	return true;
}

void EditorLayer::OnUpdate(float deltaTime)
{
	UNREFERENCED_PARAMETER(deltaTime);
	if (!m_pendingEditorAction)
		return;

	auto action = std::move(m_pendingEditorAction);
	auto completion = std::move(m_pendingEditorCompletion);
	bool succeeded = false;
	try
	{
		succeeded = action();
	}
	catch (const std::exception& exception)
	{
		LOG_ERRORF("Editor operation '{}' failed: {}", m_pendingEditorTitle, exception.what());
	}
	catch (...)
	{
		LOG_ERRORF("Editor operation '{}' failed with an unknown exception", m_pendingEditorTitle);
	}
	m_pendingEditorTitle.clear();
	m_pendingEditorDetail.clear();
	m_loadingWindow.Stop();
	if (completion)
		completion(succeeded);
}

void EditorLayer::OnImGuiRender()
{
	SynchronizeActiveScene();
	m_assetPreviewTouchedThisFrame = false;
	ApplyWorkbenchStyle();

	m_context.ValidateSelection();

	m_titleBar.Draw(m_context);
	DrawDockspace();
	DrawViewportWindows();
	const bool simulationActive = m_context.GetSimulationState
		&& m_context.GetSimulationState() != enignE::Editor::SimulationState::Stopped;
	if (simulationActive && m_gameViewportHovered
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
		&& m_context.SetGameInputCaptured)
		m_context.SetGameInputCaptured(true);
	UpdateSceneContextPopup();
	m_gizmoToolbar.Draw(m_context, m_sceneViewportOrigin, m_sceneViewportSize);
	m_sceneHierarchyPanel.Draw(m_context);
	m_inspectorPanel.Draw(m_context);
	m_assetBrowserPanel.Draw(m_context);
	DrawLightWorkbench();
	DrawCameraGizmos();
	DrawLightGizmos();
	DrawGizmo();
	DrawSceneContextPopup();
	if (m_assetPreviewRootID != 0 && !m_assetPreviewTouchedThisFrame)
		CancelAssetPreview();

	const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool clicked = mouseDown && !m_mouseWasDown;
	if (clicked
		&& !InputManager::Get().IsMouseButtonDown(2)
		&& !m_gizmoConsumedClick
		&& m_sceneViewportHovered)
	{
		const ImVec2 mouse = ImGui::GetMousePos();
		SelectEntityAt({
			mouse.x - m_sceneViewportOrigin.x,
			mouse.y - m_sceneViewportOrigin.y
		});
	}
	m_mouseWasDown = mouseDown;
	m_gizmoConsumedClick = false;
}

void EditorLayer::ApplyWorkbenchStyle()
{
	if (m_workbenchStyleApplied)
		return;

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = enignE::Editor::UI::Scale(2.0f);
	style.ChildRounding = enignE::Editor::UI::Scale(2.0f);
	style.FrameRounding = enignE::Editor::UI::Scale(2.0f);
	style.PopupRounding = enignE::Editor::UI::Scale(2.0f);
	style.GrabRounding = enignE::Editor::UI::Scale(2.0f);
	style.TabRounding = enignE::Editor::UI::Scale(2.0f);
	style.WindowBorderSize = enignE::Editor::UI::Scale(1.0f);
	style.FrameBorderSize = 0.0f;
	style.WindowPadding = enignE::Editor::UI::Scale(10.0f, 8.0f);
	style.FramePadding = enignE::Editor::UI::Scale(7.0f, 4.0f);
	style.ItemSpacing = enignE::Editor::UI::Scale(8.0f, 5.0f);
	style.CellPadding = enignE::Editor::UI::Scale(6.0f, 4.0f);
	style.IndentSpacing = 14.0f;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.88f, 0.87f, 0.84f, 1.0f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.49f, 0.46f, 1.0f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.075f, 0.082f, 1.0f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.055f, 0.060f, 1.0f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.085f, 0.083f, 0.086f, 0.98f);
	colors[ImGuiCol_Border] = ImVec4(0.24f, 0.235f, 0.220f, 1.0f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.132f, 0.130f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.205f, 0.195f, 0.180f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.330f, 0.260f, 0.140f, 1.0f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.070f, 0.070f, 0.075f, 1.0f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.095f, 0.092f, 0.090f, 1.0f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.075f, 0.075f, 0.080f, 1.0f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.055f, 0.060f, 1.0f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.215f, 0.205f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.295f, 0.265f, 1.0f);
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
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.42f, 0.32f, 0.16f, 0.45f);
	colors[ImGuiCol_Tab] = ImVec4(0.110f, 0.108f, 0.105f, 1.0f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.300f, 0.260f, 0.185f, 1.0f);
	colors[ImGuiCol_TabActive] = ImVec4(0.195f, 0.175f, 0.135f, 1.0f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.92f, 0.67f, 0.30f, 0.40f);

	m_workbenchStyleApplied = true;
}

void EditorLayer::DrawDockspace()
{
	const DirectX::XMUINT2 viewport = m_context.ViewportProviderCallback();
	const float chromeHeight = enignE::Editor::UI::Scale(32.0f);
	const ImVec2 dockspacePosition(0.0f, chromeHeight);
	const ImVec2 dockspaceSize(
		static_cast<float>(viewport.x),
		std::max(1.0f, static_cast<float>(viewport.y) - chromeHeight));

	ImGui::SetNextWindowPos(dockspacePosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(dockspaceSize, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoNavFocus
		| ImGuiWindowFlags_NoSavedSettings;
	ImGui::Begin("##EditorDockspaceHost", nullptr, hostFlags);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspaceID = ImGui::GetID("EngineWorkbenchDockspaceV4");
	if (!m_defaultDockLayoutBuilt)
	{
		if (ImGui::DockBuilderGetNode(dockspaceID) == nullptr)
			BuildDefaultDockLayout(dockspaceID, dockspaceSize);
		m_defaultDockLayoutBuilt = true;
	}

	ImGui::DockSpace(
		dockspaceID,
		ImVec2(0.0f, 0.0f),
		ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

void EditorLayer::BuildDefaultDockLayout(ImGuiID dockspaceID, const ImVec2& dockspaceSize)
{
	ImGui::DockBuilderRemoveNode(dockspaceID);
	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceID, dockspaceSize);

	ImGuiID center = dockspaceID;
	const ImGuiID signal = ImGui::DockBuilderSplitNode(
		center, ImGuiDir_Right, 0.28f, nullptr, &center);
	ImGuiID registry = ImGui::DockBuilderSplitNode(
		center, ImGuiDir_Down, 0.24f, nullptr, &center);
	ImGuiID elements = registry;
	const ImGuiID assets = ImGui::DockBuilderSplitNode(
		elements, ImGuiDir_Right, 0.62f, nullptr, &elements);

	ImGui::DockBuilderDockWindow("Scene", center);
	ImGui::DockBuilderDockWindow("Game", center);
	ImGui::DockBuilderDockWindow("Elements", elements);
	ImGui::DockBuilderDockWindow("Asset Lens", assets);
	ImGui::DockBuilderDockWindow("Stats", signal);
	ImGui::DockBuilderDockWindow("Inspector", signal);
	ImGui::DockBuilderDockWindow("Light Workbench", signal);
	ImGui::DockBuilderFinish(dockspaceID);
}

void EditorLayer::DrawViewportWindows()
{
	const DirectX::XMUINT2 windowSize = m_context.ViewportProviderCallback();
	const float availableWidth = std::max(enignE::Editor::UI::Scale(640.0f),
		static_cast<float>(windowSize.x) - enignE::Editor::UI::Scale(648.0f));
	const float halfHeight = std::max(enignE::Editor::UI::Scale(220.0f),
		(static_cast<float>(windowSize.y) - enignE::Editor::UI::Scale(132.0f)) * 0.5f);

	const auto drawViewport = [this](
		const char* title,
		const ImVec2 defaultPosition,
		const ImVec2 defaultSize,
		const TextureProvider& textureProvider,
		const ViewportResizeCallback& resizeCallback,
		DirectX::XMUINT2& viewportSize,
		DirectX::XMFLOAT2* viewportOrigin,
		bool* hovered,
		bool* visibleOut,
		bool* focusedOut,
		bool acceptPrefabDrop)
	{
		ImGui::SetNextWindowPos(defaultPosition, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		const bool visible = ImGui::Begin(title);
		if (visibleOut)
			*visibleOut = visible;
		if (focusedOut)
			*focusedOut = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (visible)
		{
			const ImVec2 available = ImGui::GetContentRegionAvail();
			const std::uint16_t width = static_cast<std::uint16_t>(
				std::clamp(available.x, 1.0f, 65535.0f));
			const std::uint16_t height = static_cast<std::uint16_t>(
				std::clamp(available.y, 1.0f, 65535.0f));
			if (width != viewportSize.x || height != viewportSize.y)
			{
				viewportSize = {width, height};
				if (resizeCallback)
					resizeCallback(width, height);
			}

			const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
			if (viewportOrigin)
				*viewportOrigin = {imageOrigin.x, imageOrigin.y};
			ID3D11ShaderResourceView* texture = nullptr;
			if (textureProvider)
				texture = textureProvider();
			if (texture)
			{
				ImGui::Image(
					reinterpret_cast<ImTextureID>(texture),
					ImVec2(static_cast<float>(width), static_cast<float>(height)),
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f));
			}
			else
			{
				ImGui::Dummy(ImVec2(static_cast<float>(width), static_cast<float>(height)));
			}
			if (hovered)
				*hovered = ImGui::IsItemHovered();
			if (acceptPrefabDrop && ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					enignE::Editor::AssetDragPayloadID,
					ImGuiDragDropFlags_AcceptBeforeDelivery))
				{
					const auto& asset = *static_cast<const enignE::Editor::AssetDragPayload*>(payload->Data);
					if (asset.Type == enignE::Graphics::AssetType::Prefab
						|| asset.Type == enignE::Graphics::AssetType::Model)
					{
						const ImVec2 mouse = ImGui::GetMousePos();
						const DirectX::XMFLOAT3 position = ScenePointFromViewport(
							{mouse.x - imageOrigin.x, mouse.y - imageOrigin.y},
							viewportSize,
							m_context.ViewProvider(),
							m_context.ProjectionProvider());
						if (UpdateAssetPreview(asset, position) && payload->IsDelivery())
							CommitAssetPreview();
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
		else if (hovered)
		{
			*hovered = false;
		}
		ImGui::End();
		ImGui::PopStyleVar();
	};

	if (m_focusSceneViewportRequested)
	{
		ImGui::SetNextWindowFocus();
		m_focusSceneViewportRequested = false;
	}
	drawViewport(
		"Scene",
		enignE::Editor::UI::Scale(12.0f, 48.0f),
		ImVec2(availableWidth, halfHeight),
		m_sceneTextureProvider,
		m_resizeSceneViewport,
		m_sceneViewportSize,
		&m_sceneViewportOrigin,
		&m_sceneViewportHovered,
		&m_sceneViewportVisible,
		&m_sceneViewportFocused,
		true);
	if (m_focusGameViewportRequested)
	{
		ImGui::SetNextWindowFocus();
		m_focusGameViewportRequested = false;
	}
	drawViewport(
		"Game",
		enignE::Editor::UI::Scale(12.0f, 48.0f),
		ImVec2(std::max(enignE::Editor::UI::Scale(320.0f), static_cast<float>(windowSize.x) * 0.32f), halfHeight),
		m_gameTextureProvider,
		m_resizeGameViewport,
		m_gameViewportSize,
		nullptr,
		&m_gameViewportHovered,
		&m_gameViewportVisible,
		&m_gameViewportFocused,
		false);

	if (m_sceneViewportInteraction)
		m_sceneViewportInteraction(m_sceneViewportHovered);
	if (m_viewportStateCallback)
	{
		m_viewportStateCallback(
			m_sceneViewportVisible,
			m_gameViewportVisible,
			m_sceneViewportFocused,
			m_gameViewportFocused);
	}
}

void EditorLayer::SynchronizeActiveScene(const std::vector<std::uint64_t>* preservedSelection)
{
	if (!m_activeSceneProvider || !m_editorScene) return;
	enignE::Scene::Scene* nextScene = &m_activeSceneProvider();
	if (!nextScene || nextScene == m_context.ActiveScene) return;

	std::vector<std::uint64_t> selectedIDs = preservedSelection
		? *preservedSelection : std::vector<std::uint64_t>{};
	if (!preservedSelection && m_context.ActiveScene)
	{
		for (const entt::entity entity : m_context.GetSelectedEntities())
		{
			const std::uint64_t id = m_context.ActiveScene->GetEntityID(entity);
			if (id != 0) selectedIDs.push_back(id);
		}
	}

	const bool enteringPlay = nextScene != m_editorScene;
	if (enteringPlay && !m_usingPlayScene)
	{
		std::swap(m_commandStack, m_suspendedEditorCommandStack);
		m_commandStack.Clear();
	}
	else if (!enteringPlay && m_usingPlayScene)
	{
		m_commandStack.Clear();
		std::swap(m_commandStack, m_suspendedEditorCommandStack);
	}
	m_usingPlayScene = enteringPlay;
	m_context.ActiveScene = nextScene;
	m_context.ClearSelection();
	std::vector<entt::entity> remapped;
	for (const std::uint64_t id : selectedIDs)
	{
		const entt::entity entity = nextScene->FindEntityByID(id);
		if (entity != entt::null) remapped.push_back(entity);
	}
	m_context.SelectEntities(remapped);
}

bool EditorLayer::LoadPrefab(
	const std::string& path,
	enignE::Editor::PrefabAsset& result) const
{
	if (!m_context.Assets) return false;
	const auto primitiveResolver = [this](const enignE::Scene::PrimitiveDesc& desc)
	{
		enignE::Scene::Scene temporary("PrefabModelResolver");
		const entt::entity entity = enignE::Scene::CreatePrimitive(
			temporary, desc, m_context.CreatePrimitiveModel);
		const auto* renderer = temporary.GetComponent<enignE::Scene::MeshRendererComponent>(entity);
		return renderer ? renderer->ModelPtr : std::shared_ptr<Model>{};
	};
	const auto modelResolver = [this](std::uint64_t, const std::string& assetPath)
	{
		return m_context.LoadModel(m_context.Assets->ResolvePath(assetPath).string());
	};
	const auto textureResolver = [this](std::uint64_t, const std::string& assetPath)
	{
		return m_context.LoadTexture(m_context.Assets->ResolvePath(assetPath).string());
	};
	return enignE::Editor::PrefabAsset::Load(
		m_context.Assets->ResolvePath(path), result,
		primitiveResolver, modelResolver, textureResolver);
}

entt::entity EditorLayer::CreateModelAssetEntity(
	std::uint64_t handle,
	const std::string& path,
	std::uint64_t parentID,
	const DirectX::XMFLOAT3* position)
{
	if (!m_context.ActiveScene || !m_context.Assets || !m_context.LoadModel) return entt::null;
	const std::filesystem::path absolutePath = m_context.Assets->ResolvePath(path);
	std::shared_ptr<Model> model = m_context.LoadModel(absolutePath.string());
	if (!model) return entt::null;
	auto& scene = *m_context.ActiveScene;
	const std::string name = absolutePath.stem().string();
	const entt::entity entity = scene.CreateEntity(name.empty() ? "Model" : name);
	if (entity == entt::null) return entt::null;
	enignE::Scene::TransformComponent transform;
	if (position) transform.SetLocalPosition(*position);
	scene.AddComponent<enignE::Scene::TransformComponent>(entity, transform);
	scene.AddComponent<enignE::Scene::HierarchyComponent>(entity);
	enignE::Scene::MeshRendererComponent renderer;
	renderer.ModelPtr = std::move(model);
	scene.AddComponent<enignE::Scene::MeshRendererComponent>(entity, renderer);
	scene.AddComponent<enignE::Scene::RenderSourceComponent>(entity, {
		.Type = enignE::Scene::RenderSourceType::ImportedModel,
		.AssetHandle = handle,
		.AssetPath = path
	});
	if (parentID != 0)
		scene.SetParent(entity, scene.FindEntityByID(parentID));
	return entity;
}

bool EditorLayer::UpdateAssetPreview(
	const enignE::Editor::AssetDragPayload& asset,
	const DirectX::XMFLOAT3& position)
{
	if (!m_context.ActiveScene) return false;
	auto& scene = *m_context.ActiveScene;
	if (m_assetPreviewRootID != 0
		&& (m_assetPreviewHandle != asset.Handle || m_assetPreviewType != asset.Type))
		CancelAssetPreview();

	if (m_assetPreviewRootID == 0)
	{
		entt::entity root = entt::null;
		if (asset.Type == enignE::Graphics::AssetType::Model)
		{
			root = CreateModelAssetEntity(asset.Handle, asset.Path.data(), 0, &position);
		}
		else if (asset.Type == enignE::Graphics::AssetType::Prefab)
		{
			enignE::Editor::PrefabAsset prefab;
			if (!LoadPrefab(asset.Path.data(), prefab)) return false;
			const auto preview = prefab.Instantiate(
				scene, asset.Handle, asset.Path.data(), 0);
			root = scene.FindEntityByID(preview.GetRootID());
		}
		if (root == entt::null) return false;
		const auto markTransient = [&scene](auto&& self, entt::entity entity) -> void
		{
			scene.AddComponent<enignE::Scene::TransientEditorComponent>(entity);
			for (const entt::entity child : scene.GetChildren(entity)) self(self, child);
		};
		markTransient(markTransient, root);
		m_assetPreviewRootID = scene.GetEntityID(root);
		m_assetPreviewHandle = asset.Handle;
		m_assetPreviewType = asset.Type;
	}

	const entt::entity root = scene.FindEntityByID(m_assetPreviewRootID);
	if (root == entt::null) { CancelAssetPreview(); return false; }
	scene.SetLocalPosition(root, position);
	m_assetPreviewTouchedThisFrame = true;
	return true;
}

void EditorLayer::CommitAssetPreview()
{
	if (!m_context.ActiveScene || m_assetPreviewRootID == 0) return;
	auto& scene = *m_context.ActiveScene;
	const entt::entity root = scene.FindEntityByID(m_assetPreviewRootID);
	if (root == entt::null) { CancelAssetPreview(); return; }
	const auto removeTransient = [&scene](auto&& self, entt::entity entity) -> void
	{
		scene.RemoveComponent<enignE::Scene::TransientEditorComponent>(entity);
		for (const entt::entity child : scene.GetChildren(entity)) self(self, child);
	};
	removeTransient(removeTransient, root);
	const auto snapshot = enignE::Editor::EntitySnapshot::CaptureSubtree(scene, root);
	m_commandStack.RecordExecuted(std::make_unique<enignE::Editor::AdoptEntityCommand>(scene, snapshot));
	m_context.SelectEntity(root);
	m_assetPreviewRootID = 0;
	m_assetPreviewHandle = 0;
	m_assetPreviewType = enignE::Graphics::AssetType::Unknown;
}

void EditorLayer::CancelAssetPreview()
{
	if (m_context.ActiveScene && m_assetPreviewRootID != 0)
	{
		const entt::entity root = m_context.ActiveScene->FindEntityByID(m_assetPreviewRootID);
		if (root != entt::null) m_context.ActiveScene->DestroyEntityRecursive(root);
	}
	m_assetPreviewRootID = 0;
	m_assetPreviewHandle = 0;
	m_assetPreviewType = enignE::Graphics::AssetType::Unknown;
}

void EditorLayer::UpdateSceneContextPopup()
{
	if (!m_sceneViewportVisible)
	{
		m_sceneContextPressStartedInViewport = false;
		m_sceneContextPressDragged = false;
		return;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		m_sceneContextPressStartedInViewport = m_sceneViewportHovered;
		m_sceneContextPressDragged = false;
	}
	if (m_sceneContextPressStartedInViewport
		&& ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f))
	{
		m_sceneContextPressDragged = true;
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		if (m_sceneContextPressStartedInViewport
			&& !m_sceneContextPressDragged
			&& m_sceneViewportHovered)
		{
			const ImVec2 mouse = ImGui::GetMousePos();
			m_sceneContextPosition = ScenePointFromViewport(
				{
					mouse.x - m_sceneViewportOrigin.x,
					mouse.y - m_sceneViewportOrigin.y
				},
				m_sceneViewportSize,
				m_context.ViewProvider(),
				m_context.ProjectionProvider());
			ImGui::OpenPopup("##SceneViewportContext");
		}
		m_sceneContextPressStartedInViewport = false;
		m_sceneContextPressDragged = false;
	}
}

void EditorLayer::DrawSceneContextPopup()
{
	if (!m_context.ActiveScene || !m_context.Commands)
		return;

	ImGui::SetNextWindowSizeConstraints(enignE::Editor::UI::Scale(190.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
	if (!ImGui::BeginPopup("##SceneViewportContext"))
		return;

	auto& scene = *m_context.ActiveScene;
	const auto selectCreated = [&scene, this](std::uint64_t id)
	{
		m_context.SelectEntity(scene.FindEntityByID(id));
	};
	const auto createEntity = [&](std::unique_ptr<enignE::Editor::CreateEntityCommand> command)
	{
		enignE::Editor::CreateEntityCommand* created = command.get();
		m_commandStack.Execute(std::move(command));
		selectCreated(created->GetEntityID());
		return created->GetEntityID();
	};
	const auto createPrimitive = [&](enignE::Scene::PrimitiveType type)
	{
		if (!m_context.CreatePrimitiveModel)
			return;

		enignE::Scene::PrimitiveDesc desc;
		desc.Type = type;
		desc.Name = PrimitiveName(type);
		desc.Position = m_sceneContextPosition;
		if (type == enignE::Scene::PrimitiveType::Rectangle)
		{
			desc.Width = 2.0f;
			desc.Height = 1.0f;
			desc.Depth = 1.0f;
		}
		else if (type == enignE::Scene::PrimitiveType::Plane)
		{
			desc.Width = 8.0f;
			desc.Depth = 8.0f;
		}

		auto command = std::make_unique<enignE::Editor::CreatePrimitiveCommand>(
			scene,
			desc,
			m_context.CreatePrimitiveModel);
		enignE::Editor::CreatePrimitiveCommand* created = command.get();
		m_commandStack.Execute(std::move(command));
		selectCreated(created->GetEntityID());
	};

	ImGui::TextDisabled("Scene");
	ImGui::Separator();
	if (ImGui::MenuItem("Empty Element"))
		createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(scene, "Element"));
	if (ImGui::MenuItem("Camera"))
	{
		const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
			scene,
			"Camera",
			enignE::Scene::CameraComponent{}));
		if (scene.GetSettings().ActiveCameraEntityID == 0)
			m_commandStack.Execute(std::make_unique<enignE::Editor::SetActiveCameraCommand>(
				scene,
				0,
				id));
	}
	if (ImGui::MenuItem("Light"))
	{
		const std::uint64_t id = createEntity(std::make_unique<enignE::Editor::CreateEntityCommand>(
			scene,
			"Light",
			enignE::Scene::LightComponent{}));
		if (scene.GetSettings().ActiveLightEntityID == 0)
			m_commandStack.Execute(std::make_unique<enignE::Editor::SetActiveLightCommand>(
				scene,
				0,
				id));
	}
	if (ImGui::BeginMenu("3D Object", m_context.CreatePrimitiveModel != nullptr))
	{
		if (ImGui::MenuItem("Cube"))
			createPrimitive(enignE::Scene::PrimitiveType::Cube);
		if (ImGui::MenuItem("Rectangle"))
			createPrimitive(enignE::Scene::PrimitiveType::Rectangle);
		if (ImGui::MenuItem("Pyramid"))
			createPrimitive(enignE::Scene::PrimitiveType::Pyramid);
		if (ImGui::MenuItem("Plane"))
			createPrimitive(enignE::Scene::PrimitiveType::Plane);
		if (ImGui::MenuItem("Sphere"))
			createPrimitive(enignE::Scene::PrimitiveType::Sphere);
		ImGui::EndMenu();
	}
	if (m_context.IsSelectedEntityValid())
	{
		ImGui::Separator();
		if (ImGui::MenuItem(
			m_context.GetSelectedEntities().size() > 1 ? "Duplicate Selected" : "Duplicate",
			"Ctrl+D"))
		{
			DuplicateSelection();
		}
		if (ImGui::MenuItem("Delete Selected"))
			DeleteSelection();
	}

	ImGui::EndPopup();
}

void EditorLayer::DrawLightWorkbench()
{
	using namespace enignE::Editor;
	using namespace enignE::Scene;

	if (!m_context.ActiveScene || !m_context.Commands)
		return;
	if (!ImGui::Begin("Light Workbench", nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		ImGui::End();
		return;
	}

	auto& scene = *m_context.ActiveScene;
	auto lights = scene.View<LightComponent, TransformComponent>();
	std::size_t total = 0;
	std::size_t enabled = 0;
	for (const entt::entity entity : lights)
	{
		if (scene.IsEntityPendingDestroy(entity))
			continue;
		++total;
		if (lights.get<LightComponent>(entity).bEnabled)
			++enabled;
	}
	ImGui::Text("Scene lights  %zu  |  GPU active  %zu / %u",
		total,
		std::min<std::size_t>(enabled, MaxGpuLights),
		static_cast<unsigned>(MaxGpuLights));
	ImGui::TextDisabled("All enabled lights illuminate; one directional or spot light supplies shadows.");
	if (enabled > MaxGpuLights)
	{
		ImGui::TextColored(
			ImVec4(1.0f, 0.48f, 0.25f, 1.0f),
			"%zu enabled lights exceed the frame budget and are not submitted.",
			enabled - MaxGpuLights);
	}

	const auto createLight = [this, &scene](LightComponent::Type type, const char* name)
	{
		LightComponent light;
		light.LightType = type;
		auto command = std::make_unique<CreateEntityCommand>(scene, name, light);
		auto* created = command.get();
		m_context.Commands->Execute(std::move(command));
		const std::uint64_t id = created->GetEntityID();
		const entt::entity entity = scene.FindEntityByID(id);
		if (entity != entt::null)
			m_context.SelectEntity(entity);
		if (scene.GetSettings().ActiveLightEntityID == 0
			&& type != LightComponent::Type::Point)
		{
			m_context.Commands->Execute(std::make_unique<SetActiveLightCommand>(scene, 0, id));
		}
	};
	if (ImGui::Button("+ Light"))
		createLight(LightComponent::Type::Directional, "Light");

	const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("##LightRows", 5, flags, enignE::Editor::UI::Scale(0.0f, 220.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, enignE::Editor::UI::Scale(34.0f));
		ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, enignE::Editor::UI::Scale(72.0f));
		ImGui::TableSetupColumn("Energy", ImGuiTableColumnFlags_WidthFixed, enignE::Editor::UI::Scale(62.0f));
		ImGui::TableSetupColumn("Shadow", ImGuiTableColumnFlags_WidthFixed, enignE::Editor::UI::Scale(62.0f));
		ImGui::TableHeadersRow();

		for (const entt::entity entity : lights)
		{
			if (scene.IsEntityPendingDestroy(entity))
				continue;
			const auto* light = scene.GetComponent<LightComponent>(entity);
			if (!light)
				continue;
			const std::uint64_t id = scene.GetEntityID(entity);
			const auto* tag = scene.GetComponent<TagComponent>(entity);
			const char* typeName = light->LightType == LightComponent::Type::Directional
				? "Directional"
				: light->LightType == LightComponent::Type::Point ? "Point" : "Spot";
			ImGui::PushID(static_cast<int>(id));
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			bool lightEnabled = light->bEnabled;
			if (ImGui::Checkbox("##Enabled", &lightEnabled))
			{
				LightComponent after = *light;
				after.bEnabled = lightEnabled;
				m_context.Commands->Execute(std::make_unique<SetLightCommand>(
					scene, id, *light, after));
				light = scene.GetComponent<LightComponent>(entity);
			}
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Selectable(
				tag ? tag->Tag : "Light",
				m_context.IsEntitySelected(entity),
				ImGuiSelectableFlags_AllowOverlap))
			{
				m_context.SelectEntity(entity);
			}
			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(typeName);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.2f", light->Intensity);
			ImGui::TableSetColumnIndex(4);
			if (light->LightType == LightComponent::Type::Point)
			{
				ImGui::TextDisabled("N/A");
			}
			else if (scene.GetSettings().ActiveLightEntityID == id)
			{
				ImGui::TextColored(ImVec4(0.96f, 0.70f, 0.30f, 1.0f), "SOURCE");
			}
			else if (ImGui::SmallButton("Set"))
			{
				m_context.Commands->Execute(std::make_unique<SetActiveLightCommand>(
					scene, scene.GetSettings().ActiveLightEntityID, id));
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void EditorLayer::DrawCameraGizmos()
{
	using namespace DirectX;
	using namespace enignE::Scene;

	if (!m_sceneViewportVisible)
		return;
	if (m_sceneViewportSize.x == 0 || m_sceneViewportSize.y == 0)
		return;

	const XMFLOAT4X4& view = m_context.ViewProvider();
	const XMFLOAT4X4& projection = m_context.ProjectionProvider();
	const XMUINT2 viewport = m_sceneViewportSize;
	const float gameAspect = static_cast<float>(std::max(1u, m_gameViewportSize.x))
		/ static_cast<float>(std::max(1u, m_gameViewportSize.y));
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(
		ImVec2(m_sceneViewportOrigin.x, m_sceneViewportOrigin.y),
		ImVec2(
			m_sceneViewportOrigin.x + static_cast<float>(viewport.x),
			m_sceneViewportOrigin.y + static_cast<float>(viewport.y)),
		true);

	auto cameras = m_context.ActiveScene->View<CameraComponent, TransformComponent>();
	for (const entt::entity entity : cameras)
	{
		const auto& camera = cameras.get<CameraComponent>(entity);
		const auto& transform = cameras.get<TransformComponent>(entity);
		const XMMATRIX world = transform.GetWorldMatrix();
		XMFLOAT3 worldPosition{};
		XMStoreFloat3(&worldPosition, world.r[3]);

		ImVec2 iconPosition{};
		const bool selected = m_context.IsEntitySelected(entity);
		const bool active = m_context.ActiveScene->GetEntityID(entity)
			== m_context.ActiveScene->GetSettings().ActiveCameraEntityID;
		if (ProjectPoint(worldPosition, view, projection, viewport, iconPosition))
		{
			iconPosition.x += m_sceneViewportOrigin.x;
			iconPosition.y += m_sceneViewportOrigin.y;
			const ImU32 iconColor = selected
				? IM_COL32(255, 194, 72, 255)
				: active ? IM_COL32(92, 218, 164, 255) : IM_COL32(112, 190, 255, 255);
			const float thickness = selected ? 2.5f : 2.0f;
			const ImVec2 bodyMin{iconPosition.x - 10.0f, iconPosition.y - 7.0f};
			const ImVec2 bodyMax{iconPosition.x + 5.0f, iconPosition.y + 7.0f};
			drawList->AddRect(bodyMin, bodyMax, iconColor, 2.0f, thickness);
			drawList->AddTriangle(
				{bodyMax.x, iconPosition.y - 5.0f},
				{iconPosition.x + 12.0f, iconPosition.y - 9.0f},
				{iconPosition.x + 12.0f, iconPosition.y + 9.0f},
				iconColor,
				thickness);
			drawList->AddCircleFilled(iconPosition, selected ? 2.5f : 2.0f, iconColor);
		}

		if (!selected)
			continue;

		XMVECTOR scale{};
		XMVECTOR rotation{};
		XMVECTOR translation{};
		XMMATRIX cameraWorld = world;
		if (XMMatrixDecompose(&scale, &rotation, &translation, world))
		{
			cameraWorld = XMMatrixRotationQuaternion(rotation)
				* XMMatrixTranslationFromVector(translation);
		}

		const float nearPlane = std::max(0.001f, camera.NearPlane);
		const float farPlane = std::max(nearPlane + 0.001f, camera.FarPlane);
		const float halfFov = XMConvertToRadians(std::clamp(camera.FOVDegrees, 1.0f, 179.0f)) * 0.5f;
		const float nearHalfHeight = std::tan(halfFov) * nearPlane;
		const float farHalfHeight = std::tan(halfFov) * farPlane;
		const std::array<XMFLOAT3, 8> localCorners{{
			{-nearHalfHeight * gameAspect, -nearHalfHeight, nearPlane},
			{ nearHalfHeight * gameAspect, -nearHalfHeight, nearPlane},
			{ nearHalfHeight * gameAspect,  nearHalfHeight, nearPlane},
			{-nearHalfHeight * gameAspect,  nearHalfHeight, nearPlane},
			{-farHalfHeight * gameAspect, -farHalfHeight, farPlane},
			{ farHalfHeight * gameAspect, -farHalfHeight, farPlane},
			{ farHalfHeight * gameAspect,  farHalfHeight, farPlane},
			{-farHalfHeight * gameAspect,  farHalfHeight, farPlane}
		}};
		std::array<XMFLOAT3, 8> worldCorners{};
		for (std::size_t i = 0; i < localCorners.size(); ++i)
		{
			XMStoreFloat3(
				&worldCorners[i],
				XMVector3TransformCoord(XMLoadFloat3(&localCorners[i]), cameraWorld));
		}

		constexpr std::array<std::array<int, 2>, 12> edges{{
			{{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
			{{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
			{{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
		}};
		const ImU32 frustumColor = IM_COL32(255, 194, 72, 210);
		for (const auto& edge : edges)
		{
			ImVec2 start{};
			ImVec2 end{};
			if (!ProjectClippedLine(
					worldCorners[edge[0]],
					worldCorners[edge[1]],
					view,
					projection,
					viewport,
					start,
					end))
			{
				continue;
			}
			start.x += m_sceneViewportOrigin.x;
			start.y += m_sceneViewportOrigin.y;
			end.x += m_sceneViewportOrigin.x;
			end.y += m_sceneViewportOrigin.y;
			drawList->AddLine(start, end, frustumColor, 1.5f);
		}
	}

	drawList->PopClipRect();
}

void EditorLayer::DrawLightGizmos()
{
	using namespace DirectX;
	using namespace enignE::Scene;

	if (!m_sceneViewportVisible)
		return;
	if (m_sceneViewportSize.x == 0 || m_sceneViewportSize.y == 0)
		return;

	const XMFLOAT4X4& view = m_context.ViewProvider();
	const XMFLOAT4X4& projection = m_context.ProjectionProvider();
	const XMUINT2 viewport = m_sceneViewportSize;
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(
		ImVec2(m_sceneViewportOrigin.x, m_sceneViewportOrigin.y),
		ImVec2(
			m_sceneViewportOrigin.x + static_cast<float>(viewport.x),
			m_sceneViewportOrigin.y + static_cast<float>(viewport.y)),
		true);

	const auto drawWorldLine = [&](const XMFLOAT3& start, const XMFLOAT3& end, ImU32 color, float thickness)
	{
		ImVec2 screenStart{};
		ImVec2 screenEnd{};
		if (!ProjectClippedLine(start, end, view, projection, viewport, screenStart, screenEnd))
			return;
		screenStart.x += m_sceneViewportOrigin.x;
		screenStart.y += m_sceneViewportOrigin.y;
		screenEnd.x += m_sceneViewportOrigin.x;
		screenEnd.y += m_sceneViewportOrigin.y;
		drawList->AddLine(screenStart, screenEnd, color, thickness);
	};

	auto lights = m_context.ActiveScene->View<LightComponent, TransformComponent>();
	for (const entt::entity entity : lights)
	{
		const auto& light = lights.get<LightComponent>(entity);
		const auto& transform = lights.get<TransformComponent>(entity);
		const XMMATRIX world = transform.GetWorldMatrix();
		XMVECTOR scale{};
		XMVECTOR rotation = XMQuaternionIdentity();
		XMVECTOR translation = world.r[3];
		XMMatrixDecompose(&scale, &rotation, &translation, world);
		const XMMATRIX lightWorld =
			XMMatrixRotationQuaternion(rotation) * XMMatrixTranslationFromVector(translation);
		XMFLOAT3 position{};
		XMStoreFloat3(&position, translation);

		const bool selected = m_context.IsEntitySelected(entity);
		const bool active = m_context.ActiveScene->GetEntityID(entity)
			== m_context.ActiveScene->GetSettings().ActiveLightEntityID;
		const ImU32 color = !light.bEnabled
			? IM_COL32(118, 116, 108, selected ? 230 : 150)
			: selected
			? IM_COL32(255, 194, 72, 255)
			: active ? IM_COL32(255, 238, 112, 255) : IM_COL32(255, 221, 92, 230);

		ImVec2 icon{};
		if (ProjectPoint(position, view, projection, viewport, icon))
		{
			icon.x += m_sceneViewportOrigin.x;
			icon.y += m_sceneViewportOrigin.y;
			if (light.LightType == LightComponent::Type::Directional)
			{
				drawList->AddCircle(icon, 6.0f, color, 16, 2.0f);
				for (int ray = 0; ray < 8; ++ray)
				{
					const float angle = XM_2PI * static_cast<float>(ray) / 8.0f;
					const ImVec2 direction{std::cos(angle), std::sin(angle)};
					drawList->AddLine(
						{icon.x + direction.x * 9.0f, icon.y + direction.y * 9.0f},
						{icon.x + direction.x * 13.0f, icon.y + direction.y * 13.0f},
						color,
						2.0f);
				}
			}
			else if (light.LightType == LightComponent::Type::Point)
			{
				drawList->AddCircle(icon, 7.0f, color, 16, 2.0f);
				drawList->AddLine({icon.x - 10.0f, icon.y}, {icon.x + 10.0f, icon.y}, color, 1.5f);
				drawList->AddLine({icon.x, icon.y - 10.0f}, {icon.x, icon.y + 10.0f}, color, 1.5f);
			}
			else
			{
				drawList->AddTriangle(
					{icon.x - 8.0f, icon.y - 6.0f},
					{icon.x - 8.0f, icon.y + 6.0f},
					{icon.x + 9.0f, icon.y},
					color,
					2.0f);
			}
		}

		if (!selected)
			continue;

		const float range = std::max(light.Range, 0.01f);
		constexpr int segments = 48;
		if (light.LightType == LightComponent::Type::Point)
		{
			for (int plane = 0; plane < 3; ++plane)
			{
				XMFLOAT3 previous{};
				for (int segment = 0; segment <= segments; ++segment)
				{
					const float angle = XM_2PI * static_cast<float>(segment) / segments;
					const float cosine = std::cos(angle) * range;
					const float sine = std::sin(angle) * range;
					const XMFLOAT3 local = plane == 0
						? XMFLOAT3{cosine, sine, 0.0f}
						: plane == 1 ? XMFLOAT3{cosine, 0.0f, sine}
						: XMFLOAT3{0.0f, cosine, sine};
					XMFLOAT3 current{};
					XMStoreFloat3(&current, XMVector3TransformCoord(XMLoadFloat3(&local), lightWorld));
					if (segment > 0)
						drawWorldLine(previous, current, color, 1.25f);
					previous = current;
				}
			}
			const XMMATRIX inverseView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
			const XMVECTOR cameraRight = XMVector3Normalize(inverseView.r[0]);
			XMFLOAT3 silhouetteEdge{};
			XMStoreFloat3(
				&silhouetteEdge,
				XMVectorAdd(translation, XMVectorScale(cameraRight, range)));
			ImVec2 screenCenter{};
			ImVec2 screenEdge{};
			if (ProjectPoint(position, view, projection, viewport, screenCenter)
				&& ProjectPoint(silhouetteEdge, view, projection, viewport, screenEdge))
			{
				screenCenter.x += m_sceneViewportOrigin.x;
				screenCenter.y += m_sceneViewportOrigin.y;
				screenEdge.x += m_sceneViewportOrigin.x;
				screenEdge.y += m_sceneViewportOrigin.y;
				const float silhouetteRadius = std::sqrt(
					(screenEdge.x - screenCenter.x) * (screenEdge.x - screenCenter.x)
					+ (screenEdge.y - screenCenter.y) * (screenEdge.y - screenCenter.y));
				drawList->AddCircle(screenCenter, silhouetteRadius, color, 72, 1.75f);
			}
		}
		else if (light.LightType == LightComponent::Type::Spot)
		{
			const float radius = std::tan(
				XMConvertToRadians(std::clamp(light.SpotAngle, 1.0f, 179.0f) * 0.5f)) * range;
			std::array<XMFLOAT3, 4> cardinal{};
			XMFLOAT3 previous{};
			for (int segment = 0; segment <= segments; ++segment)
			{
				const float angle = XM_2PI * static_cast<float>(segment) / segments;
				const XMFLOAT3 local{
					std::cos(angle) * radius,
					std::sin(angle) * radius,
					range
				};
				XMFLOAT3 current{};
				XMStoreFloat3(&current, XMVector3TransformCoord(XMLoadFloat3(&local), lightWorld));
				if (segment > 0)
					drawWorldLine(previous, current, color, 1.25f);
				if (segment % (segments / 4) == 0 && segment < segments)
					cardinal[segment / (segments / 4)] = current;
				previous = current;
			}
			for (const XMFLOAT3& corner : cardinal)
				drawWorldLine(position, corner, color, 1.25f);
		}
		else
		{
			const XMVECTOR forward = XMVector3Normalize(
				XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), lightWorld));
			const XMVECTOR right = XMVector3Normalize(
				XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), lightWorld));
			const XMVECTOR up = XMVector3Normalize(
				XMVector3TransformNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), lightWorld));
			for (int x = -1; x <= 1; ++x)
			{
				const XMVECTOR start = XMVectorAdd(
					translation,
					XMVectorScale(right, static_cast<float>(x) * 0.5f));
				XMFLOAT3 lineStart{};
				XMFLOAT3 lineEnd{};
				XMStoreFloat3(&lineStart, start);
				XMStoreFloat3(&lineEnd, XMVectorAdd(start, XMVectorScale(forward, 2.5f)));
				drawWorldLine(lineStart, lineEnd, color, 1.5f);
			}
			for (int y = -1; y <= 1; y += 2)
			{
				XMFLOAT3 lineStart{};
				XMFLOAT3 lineEnd{};
				const XMVECTOR start = XMVectorAdd(translation, XMVectorScale(up, static_cast<float>(y) * 0.5f));
				XMStoreFloat3(&lineStart, start);
				XMStoreFloat3(&lineEnd, XMVectorAdd(start, XMVectorScale(forward, 2.5f)));
				drawWorldLine(lineStart, lineEnd, color, 1.5f);
			}
		}
	}

	drawList->PopClipRect();
}

void EditorLayer::DrawGizmo()
{
	using namespace DirectX;
	using enignE::Editor::GizmoMode;

	const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	if (!m_sceneViewportVisible)
	{
		if (!mouseDown)
			CommitGizmoDrag();
		return;
	}
	if (!m_context.IsSelectedEntityValid())
	{
		if (!mouseDown)
			CommitGizmoDrag();
		return;
	}
	const auto* transform = m_context.ActiveScene->GetComponent<enignE::Scene::TransformComponent>(
		m_context.SelectedEntity);
	if (!transform)
	{
		if (!mouseDown)
			CommitGizmoDrag();
		return;
	}
	if (InputManager::Get().IsMouseButtonDown(2))
	{
		CommitGizmoDrag();
		m_activeAxis = -1;
		m_dragWorldUnitsPerPixel = 0.0f;
		return;
	}

	XMFLOAT3 origin{};
	XMStoreFloat3(&origin, transform->GetWorldMatrix().r[3]);
	const XMFLOAT4X4& view = m_context.ViewProvider();
	const XMFLOAT4X4& projection = m_context.ProjectionProvider();
	const XMUINT2 viewport = m_sceneViewportSize;
	ImVec2 screenOrigin{};
	if (!ProjectPoint(origin, view, projection, viewport, screenOrigin))
		return;
	screenOrigin.x += m_sceneViewportOrigin.x;
	screenOrigin.y += m_sceneViewportOrigin.y;

	const std::array<XMFLOAT3, 3> axes{{
		{1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 1.0f}
	}};
	const std::array<ImU32, 3> colors{{
		IM_COL32(232, 76, 86, 255),
		IM_COL32(90, 210, 120, 255),
		IM_COL32(72, 145, 235, 255)
	}};
	const ImVec2 mouse = ImGui::GetMousePos();
	const bool clicked = mouseDown && !m_mouseWasDown && m_sceneViewportHovered;
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(
		ImVec2(m_sceneViewportOrigin.x, m_sceneViewportOrigin.y),
		ImVec2(
			m_sceneViewportOrigin.x + static_cast<float>(m_sceneViewportSize.x),
			m_sceneViewportOrigin.y + static_cast<float>(m_sceneViewportSize.y)),
		true);

	if (m_context.CurrentGizmoMode == GizmoMode::Rotate)
	{
		XMVECTOR worldScale{};
		XMVECTOR worldRotation = XMQuaternionIdentity();
		XMVECTOR worldTranslation{};
		XMMatrixDecompose(
			&worldScale,
			&worldRotation,
			&worldTranslation,
			transform->GetWorldMatrix());
		const XMMATRIX orientation = XMMatrixRotationQuaternion(worldRotation);
		constexpr int ringSegments = 72;
		const float ringRadius = GizmoLength;
		const XMMATRIX viewMatrix = XMLoadFloat4x4(&view);
		const XMMATRIX inverseView = XMMatrixInverse(nullptr, viewMatrix);
		const XMVECTOR toCamera = XMVector3Normalize(
			XMVectorSubtract(inverseView.r[3], worldTranslation));
		float silhouetteRadius = 0.0f;
		int hoveredAxis = -1;
		float hoveredDistance = std::numeric_limits<float>::max();
		ImVec2 hoveredDirection{};

		for (int axis = 0; axis < 3; ++axis)
		{
			const int firstBasis = (axis + 1) % 3;
			const int secondBasis = (axis + 2) % 3;
			const XMVECTOR localAxes[3] = {
				XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
			};
			const XMVECTOR basisA = XMVector3Normalize(
				XMVector3TransformNormal(localAxes[firstBasis], orientation));
			const XMVECTOR basisB = XMVector3Normalize(
				XMVector3TransformNormal(localAxes[secondBasis], orientation));
			float closestDistance = std::numeric_limits<float>::max();
			ImVec2 closestDirection{};
			for (int segment = 0; segment < ringSegments; ++segment)
			{
				const float firstAngle = XM_2PI * static_cast<float>(segment) / ringSegments;
				const float secondAngle = XM_2PI * static_cast<float>(segment + 1) / ringSegments;
				const XMVECTOR firstOffset = XMVectorScale(
					XMVectorAdd(
						XMVectorScale(basisA, std::cos(firstAngle)),
						XMVectorScale(basisB, std::sin(firstAngle))),
					ringRadius);
				const XMVECTOR secondOffset = XMVectorScale(
					XMVectorAdd(
						XMVectorScale(basisA, std::cos(secondAngle)),
						XMVectorScale(basisB, std::sin(secondAngle))),
					ringRadius);
				XMFLOAT3 firstWorld{};
				XMFLOAT3 secondWorld{};
				XMStoreFloat3(&firstWorld, XMVectorAdd(worldTranslation, firstOffset));
				XMStoreFloat3(&secondWorld, XMVectorAdd(worldTranslation, secondOffset));
				ImVec2 firstScreen{};
				ImVec2 secondScreen{};
				if (!ProjectPoint(firstWorld, view, projection, viewport, firstScreen)
					|| !ProjectPoint(secondWorld, view, projection, viewport, secondScreen))
				{
					continue;
				}
				firstScreen.x += m_sceneViewportOrigin.x;
				firstScreen.y += m_sceneViewportOrigin.y;
				secondScreen.x += m_sceneViewportOrigin.x;
				secondScreen.y += m_sceneViewportOrigin.y;
				silhouetteRadius = std::max(
					silhouetteRadius,
					std::sqrt(
						(firstScreen.x - screenOrigin.x) * (firstScreen.x - screenOrigin.x)
						+ (firstScreen.y - screenOrigin.y) * (firstScreen.y - screenOrigin.y)));
				silhouetteRadius = std::max(
					silhouetteRadius,
					std::sqrt(
						(secondScreen.x - screenOrigin.x) * (secondScreen.x - screenOrigin.x)
						+ (secondScreen.y - screenOrigin.y) * (secondScreen.y - screenOrigin.y)));

				const XMVECTOR midpointOffset = XMVector3Normalize(
					XMVectorAdd(firstOffset, secondOffset));
				const float facing = XMVectorGetX(XMVector3Dot(midpointOffset, toCamera));
				const ImU32 segmentColor = facing > 0.0f
					? colors[axis]
					: (colors[axis] & IM_COL32(255, 255, 255, 0)) | IM_COL32(0, 0, 0, 85);
				drawList->AddLine(
					firstScreen,
					secondScreen,
					segmentColor,
					m_activeAxis == axis ? 4.0f : 2.5f);

				const float distance = DistanceToSegment(mouse, firstScreen, secondScreen);
				if (distance < closestDistance)
				{
					closestDistance = distance;
					closestDirection = {
						secondScreen.x - firstScreen.x,
						secondScreen.y - firstScreen.y
					};
				}
			}

			if (closestDistance < hoveredDistance)
			{
				hoveredAxis = axis;
				hoveredDistance = closestDistance;
				hoveredDirection = closestDirection;
			}
		}
		if (clicked && hoveredAxis >= 0 && hoveredDistance <= GizmoHitDistance)
		{
			const float directionLength = std::sqrt(
				hoveredDirection.x * hoveredDirection.x
				+ hoveredDirection.y * hoveredDirection.y);
			m_activeAxis = hoveredAxis;
			BeginGizmoDrag();
			m_dragStartMouse = {mouse.x, mouse.y};
			m_dragScreenDirection = directionLength > 0.001f
				? XMFLOAT2{
					hoveredDirection.x / directionLength,
					hoveredDirection.y / directionLength}
				: XMFLOAT2{1.0f, 0.0f};
			m_dragStartValue = transform->GetLocalRotation();
			m_dragWorldUnitsPerPixel = 0.0f;
			m_gizmoConsumedClick = true;
		}
		if (silhouetteRadius > 1.0f)
		{
			drawList->AddCircle(
				screenOrigin,
				silhouetteRadius,
				IM_COL32(218, 218, 218, 185),
				96,
				1.5f);
		}
		drawList->AddCircleFilled(screenOrigin, 3.5f, IM_COL32(225, 225, 225, 210));
		if (m_activeAxis >= 0 && mouseDown)
		{
			const float mouseDelta =
				(mouse.x - m_dragStartMouse.x) * m_dragScreenDirection.x
				+ (mouse.y - m_dragStartMouse.y) * m_dragScreenDirection.y;
			const XMVECTOR localAxes[3] = {
				XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
			};
			const float angle = mouseDelta * 0.01f;
			const XMVECTOR delta = XMQuaternionRotationAxis(localAxes[m_activeAxis], angle);
			for (const GizmoDragEntry& entry : m_gizmoDragEntries)
			{
				const XMVECTOR startQuaternion = XMLoadFloat4(&entry.Start.RotationQuaternion);
				XMFLOAT4 entityQuaternion{};
				XMStoreFloat4(
					&entityQuaternion,
					XMQuaternionNormalize(XMQuaternionMultiply(delta, startQuaternion)));
				XMFLOAT3 entityRotation = entry.Start.Rotation;
				(&entityRotation.x)[m_activeAxis] += angle;
				m_context.ActiveScene->SetLocalRotationQuaternion(
					m_context.ActiveScene->FindEntityByID(entry.EntityID),
					entityQuaternion,
					entityRotation);
			}
		}
	}
	else
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			XMFLOAT3 displayAxis = axes[axis];
			if (m_context.CurrentGizmoMode == GizmoMode::Translate && (axis == 0 || axis == 2))
			{
				displayAxis.x = -displayAxis.x;
				displayAxis.y = -displayAxis.y;
				displayAxis.z = -displayAxis.z;
			}
			const XMFLOAT3 endpoint{
				origin.x + displayAxis.x * GizmoLength,
				origin.y + displayAxis.y * GizmoLength,
				origin.z + displayAxis.z * GizmoLength
			};
			ImVec2 screenEndpoint{};
			if (!ProjectPoint(endpoint, view, projection, viewport, screenEndpoint))
				continue;
			screenEndpoint.x += m_sceneViewportOrigin.x;
			screenEndpoint.y += m_sceneViewportOrigin.y;
			drawList->AddLine(
				screenOrigin,
				screenEndpoint,
				colors[axis],
				m_activeAxis == axis ? 5.0f : 3.0f);
			if (m_context.CurrentGizmoMode == GizmoMode::Scale)
			{
				drawList->AddRectFilled(
					{screenEndpoint.x - 5.0f, screenEndpoint.y - 5.0f},
					{screenEndpoint.x + 5.0f, screenEndpoint.y + 5.0f},
					colors[axis]);
			}
			else
			{
				const ImVec2 direction{
					screenEndpoint.x - screenOrigin.x,
					screenEndpoint.y - screenOrigin.y
				};
				const float length = std::sqrt(
					direction.x * direction.x + direction.y * direction.y);
				if (length > 1.0f)
				{
					const ImVec2 forward{direction.x / length, direction.y / length};
					const ImVec2 side{-forward.y, forward.x};
					const float arrowLength = m_activeAxis == axis ? 14.0f : 12.0f;
					const float arrowWidth = m_activeAxis == axis ? 7.0f : 6.0f;
					drawList->AddTriangleFilled(
						screenEndpoint,
						{
							screenEndpoint.x - forward.x * arrowLength + side.x * arrowWidth,
							screenEndpoint.y - forward.y * arrowLength + side.y * arrowWidth
						},
						{
							screenEndpoint.x - forward.x * arrowLength - side.x * arrowWidth,
							screenEndpoint.y - forward.y * arrowLength - side.y * arrowWidth
						},
						colors[axis]);
				}
			}

			if (clicked && DistanceToSegment(mouse, screenOrigin, screenEndpoint) <= GizmoHitDistance)
			{
				const ImVec2 dragDirection{
					screenEndpoint.x - screenOrigin.x,
					screenEndpoint.y - screenOrigin.y};
				const float dragLength = std::sqrt(
					dragDirection.x * dragDirection.x + dragDirection.y * dragDirection.y);
				if (dragLength > 1.0f)
				{
					m_activeAxis = axis;
					BeginGizmoDrag();
					m_dragStartMouse = {mouse.x, mouse.y};
					m_dragScreenDirection = {
						dragDirection.x / dragLength,
						dragDirection.y / dragLength};
					m_dragWorldUnitsPerPixel = GizmoLength / dragLength;
					m_dragStartValue = m_context.CurrentGizmoMode == GizmoMode::Translate
						? transform->GetLocalPosition()
						: transform->GetLocalScale();
					m_gizmoConsumedClick = true;
				}
			}

			if (m_activeAxis == axis && mouseDown)
			{
				if (m_dragWorldUnitsPerPixel > 0.0f)
				{
					const float mouseDelta =
						(mouse.x - m_dragStartMouse.x) * m_dragScreenDirection.x
						+ (mouse.y - m_dragStartMouse.y) * m_dragScreenDirection.y;
					XMFLOAT3 value = m_dragStartValue;
					float& component = (&value.x)[axis];
					const float axisDirection =
						m_context.CurrentGizmoMode == GizmoMode::Translate && (axis == 0 || axis == 2)
							? -1.0f
							: 1.0f;
					component += axisDirection * mouseDelta * m_dragWorldUnitsPerPixel;
					if (m_context.CurrentGizmoMode == GizmoMode::Translate)
					{
						const float delta = component - (&m_dragStartValue.x)[axis];
						for (const GizmoDragEntry& entry : m_gizmoDragEntries)
						{
							XMFLOAT3 position = entry.Start.Position;
							(&position.x)[axis] += delta;
							m_context.ActiveScene->SetLocalPosition(
								m_context.ActiveScene->FindEntityByID(entry.EntityID),
								position);
						}
					}
					else
					{
						component = std::max(0.001f, component);
						const float delta = component - (&m_dragStartValue.x)[axis];
						for (const GizmoDragEntry& entry : m_gizmoDragEntries)
						{
							XMFLOAT3 scale = entry.Start.Scale;
							(&scale.x)[axis] = std::max(0.001f, (&scale.x)[axis] + delta);
							m_context.ActiveScene->SetLocalScale(
								m_context.ActiveScene->FindEntityByID(entry.EntityID),
								scale);
						}
					}
				}
			}
		}
	}

	if (!mouseDown)
	{
		CommitGizmoDrag();
		m_activeAxis = -1;
		m_dragWorldUnitsPerPixel = 0.0f;
	}
	drawList->PopClipRect();
}

void EditorLayer::BeginGizmoDrag()
{
	if (m_gizmoDragActive || !m_context.IsSelectedEntityValid())
		return;
	const auto* transform = m_context.ActiveScene->GetComponent<enignE::Scene::TransformComponent>(
		m_context.SelectedEntity);
	if (!transform)
		return;
	m_gizmoDragEntries.clear();
	for (const entt::entity entity : m_context.GetSelectedEntities())
	{
		bool nested = false;
		for (entt::entity parent = m_context.ActiveScene->GetParent(entity);
			parent != entt::null;
			parent = m_context.ActiveScene->GetParent(parent))
		{
			if (m_context.IsEntitySelected(parent))
			{
				nested = true;
				break;
			}
		}
		const auto* selectedTransform =
			m_context.ActiveScene->GetComponent<enignE::Scene::TransformComponent>(entity);
		if (nested || !selectedTransform)
			continue;
		m_gizmoDragEntries.push_back({
			m_context.ActiveScene->GetEntityID(entity),
			{
				selectedTransform->GetLocalPosition(),
				selectedTransform->GetLocalRotation(),
				selectedTransform->GetLocalScale(),
				selectedTransform->GetLocalRotationQuaternion(),
				true
			}
		});
	}
	if (m_gizmoDragEntries.empty())
		return;
	m_gizmoDragActive = true;
}

void EditorLayer::CommitGizmoDrag()
{
	if (!m_gizmoDragActive)
		return;
	auto command = std::make_unique<enignE::Editor::CompositeCommand>();
	for (const GizmoDragEntry& entry : m_gizmoDragEntries)
	{
		const entt::entity entity = m_context.ActiveScene->FindEntityByID(entry.EntityID);
		const auto* transform =
			m_context.ActiveScene->GetComponent<enignE::Scene::TransformComponent>(entity);
		if (!transform)
			continue;
		const enignE::Editor::TransformValue after{
			transform->GetLocalPosition(),
			transform->GetLocalRotation(),
			transform->GetLocalScale(),
			transform->GetLocalRotationQuaternion(),
			true
		};
		const auto& before = entry.Start;
		const bool changed =
			before.Position.x != after.Position.x
			|| before.Position.y != after.Position.y
			|| before.Position.z != after.Position.z
			|| before.Rotation.x != after.Rotation.x
			|| before.Rotation.y != after.Rotation.y
			|| before.Rotation.z != after.Rotation.z
			|| before.Scale.x != after.Scale.x
			|| before.Scale.y != after.Scale.y
			|| before.Scale.z != after.Scale.z;
		if (changed)
		{
			command->Add(std::make_unique<enignE::Editor::SetTransformCommand>(
				*m_context.ActiveScene,
				entry.EntityID,
				before,
				after));
		}
	}
	if (!command->Empty())
		m_commandStack.RecordExecuted(std::move(command));
	m_gizmoDragActive = false;
	m_gizmoDragEntries.clear();
}

void EditorLayer::SelectEntityAt(const DirectX::XMFLOAT2& mousePosition)
{
	using namespace DirectX;
	using namespace enignE::Scene;

	const XMUINT2 viewport = m_sceneViewportSize;
	if (viewport.x == 0 || viewport.y == 0)
		return;
	const XMMATRIX projection = XMLoadFloat4x4(&m_context.ProjectionProvider());
	const XMMATRIX view = XMLoadFloat4x4(&m_context.ViewProvider());
	const XMVECTOR nearPoint = XMVector3Unproject(
		XMVectorSet(mousePosition.x, mousePosition.y, 0.0f, 1.0f),
		0.0f, 0.0f, static_cast<float>(viewport.x), static_cast<float>(viewport.y), 0.0f, 1.0f,
		projection, view, XMMatrixIdentity());
	const XMVECTOR farPoint = XMVector3Unproject(
		XMVectorSet(mousePosition.x, mousePosition.y, 1.0f, 1.0f),
		0.0f, 0.0f, static_cast<float>(viewport.x), static_cast<float>(viewport.y), 0.0f, 1.0f,
		projection, view, XMMatrixIdentity());
	const XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));

	entt::entity nearestEntity = entt::null;
	float nearestDistance = std::numeric_limits<float>::max();
	auto cameras = m_context.ActiveScene->View<CameraComponent, TransformComponent>();
	for (const entt::entity entity : cameras)
	{
		const auto& transform = cameras.get<TransformComponent>(entity);
		XMFLOAT3 worldPosition{};
		XMStoreFloat3(&worldPosition, transform.GetWorldMatrix().r[3]);
		ImVec2 screenPosition{};
		if (!ProjectPoint(
				worldPosition,
				m_context.ViewProvider(),
				m_context.ProjectionProvider(),
				viewport,
				screenPosition))
		{
			continue;
		}
		const float dx = mousePosition.x - screenPosition.x;
		const float dy = mousePosition.y - screenPosition.y;
		if (dx * dx + dy * dy <= 16.0f * 16.0f)
		{
			if (ImGui::GetIO().KeyCtrl)
				m_context.ToggleEntitySelection(entity);
			else
				m_context.SelectEntity(entity);
			return;
		}
	}

	auto lights = m_context.ActiveScene->View<LightComponent, TransformComponent>();
	for (const entt::entity entity : lights)
	{
		const auto& transform = lights.get<TransformComponent>(entity);
		XMFLOAT3 worldPosition{};
		XMStoreFloat3(&worldPosition, transform.GetWorldMatrix().r[3]);
		ImVec2 screenPosition{};
		if (!ProjectPoint(
				worldPosition,
				m_context.ViewProvider(),
				m_context.ProjectionProvider(),
				viewport,
				screenPosition))
		{
			continue;
		}
		const float dx = mousePosition.x - screenPosition.x;
		const float dy = mousePosition.y - screenPosition.y;
		if (dx * dx + dy * dy <= 16.0f * 16.0f)
		{
			if (ImGui::GetIO().KeyCtrl)
				m_context.ToggleEntitySelection(entity);
			else
				m_context.SelectEntity(entity);
			return;
		}
	}

	auto entities = m_context.ActiveScene->View<MeshRendererComponent, TransformComponent>();
	for (const entt::entity entity : entities)
	{
		const auto& renderer = entities.get<MeshRendererComponent>(entity);
		const auto& transform = entities.get<TransformComponent>(entity);
		if (!renderer.bVisible)
			continue;
		XMFLOAT3 minimum{};
		XMFLOAT3 maximum{};
		float distance = 0.0f;
		if (GetWorldBounds(renderer, transform, minimum, maximum)
			&& RayIntersectsAabb(nearPoint, direction, minimum, maximum, distance)
			&& distance < nearestDistance)
		{
			nearestDistance = distance;
			nearestEntity = entity;
		}
	}
	if (ImGui::GetIO().KeyCtrl)
	{
		if (nearestEntity != entt::null)
			m_context.ToggleEntitySelection(nearestEntity);
	}
	else
		m_context.SelectEntity(nearestEntity);
}

void EditorLayer::DuplicateSelection()
{
	if (!m_context.ActiveScene || !m_context.Commands)
		return;
	std::vector<entt::entity> roots;
	for (const entt::entity candidate : m_context.GetSelectedEntities())
	{
		bool nested = false;
		for (entt::entity parent = m_context.ActiveScene->GetParent(candidate);
			parent != entt::null;
			parent = m_context.ActiveScene->GetParent(parent))
		{
			if (m_context.IsEntitySelected(parent))
			{
				nested = true;
				break;
			}
		}
		if (!nested)
			roots.push_back(candidate);
	}
	auto command = std::make_unique<enignE::Editor::CompositeCommand>();
	std::vector<enignE::Editor::DuplicateEntityCommand*> duplicates;
	for (const entt::entity entity : roots)
	{
		auto duplicate = std::make_unique<enignE::Editor::DuplicateEntityCommand>(
			*m_context.ActiveScene,
			entity);
		duplicates.push_back(duplicate.get());
		command->Add(std::move(duplicate));
	}
	if (command->Empty())
		return;
	m_commandStack.Execute(std::move(command));
	std::vector<entt::entity> selection;
	for (const auto* duplicate : duplicates)
		selection.push_back(m_context.ActiveScene->FindEntityByID(duplicate->GetEntityID()));
	m_context.SelectEntities(selection);
}

void EditorLayer::CopySelection()
{
	m_entityClipboard.clear();
	if (!m_context.ActiveScene)
		return;
	for (const entt::entity candidate : m_context.GetSelectedEntities())
	{
		bool nested = false;
		for (entt::entity parent = m_context.ActiveScene->GetParent(candidate);
			parent != entt::null; parent = m_context.ActiveScene->GetParent(parent))
		{
			if (m_context.IsEntitySelected(parent)) { nested = true; break; }
		}
		if (!nested)
			m_entityClipboard.push_back(
				enignE::Editor::EntitySnapshot::CaptureSubtree(*m_context.ActiveScene, candidate));
	}
}

void EditorLayer::PasteSelection()
{
	if (!m_context.ActiveScene || !m_context.Commands || m_entityClipboard.empty())
		return;
	auto command = std::make_unique<enignE::Editor::CompositeCommand>();
	std::vector<enignE::Editor::PasteEntityCommand*> pasted;
	for (const auto& snapshot : m_entityClipboard)
	{
		auto item = std::make_unique<enignE::Editor::PasteEntityCommand>(
			*m_context.ActiveScene, snapshot);
		pasted.push_back(item.get());
		command->Add(std::move(item));
	}
	m_commandStack.Execute(std::move(command));
	std::vector<entt::entity> selection;
	for (const auto* item : pasted)
		selection.push_back(m_context.ActiveScene->FindEntityByID(item->GetEntityID()));
	m_context.SelectEntities(selection);
}

void EditorLayer::DeleteSelection()
{
	if (!m_context.ActiveScene || !m_context.Commands)
		return;
	auto command = std::make_unique<enignE::Editor::CompositeCommand>();
	for (const entt::entity candidate : m_context.GetSelectedEntities())
	{
		bool nested = false;
		for (entt::entity parent = m_context.ActiveScene->GetParent(candidate);
			parent != entt::null;
			parent = m_context.ActiveScene->GetParent(parent))
		{
			if (m_context.IsEntitySelected(parent))
			{
				nested = true;
				break;
			}
		}
		if (!nested)
			command->Add(std::make_unique<enignE::Editor::DeleteEntityCommand>(
				*m_context.ActiveScene,
				candidate));
	}
	if (!command->Empty())
		m_commandStack.Execute(std::move(command));
	m_context.ClearSelection();
}
