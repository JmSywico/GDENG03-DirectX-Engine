#pragma once

#include "Layer.h"
#include "../Editor/EditorContext.h"
#include "../Editor/Commands/CommandStack.h"
#include "../Editor/UI/EditorLoadingWindow.h"
#include "../Editor/UI/Chrome/TitleBar.h"
#include "../Editor/UI/Panels/InspectorPanel.h"
#include "../Editor/UI/Panels/AssetBrowserPanel.h"
#include "../Editor/UI/Panels/SceneHierarchyPanel.h"
#include "../Editor/UI/Widgets/GizmoToolbar.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

class EditorLayer : public Layer
{
public:
	using MatrixProvider = jnpf::Editor::EditorContext::MatrixProvider;
	using ViewportProvider = jnpf::Editor::EditorContext::ViewportProvider;
	using SceneChangedCallback = jnpf::Editor::EditorContext::SceneChangedCallback;
	using WindowAction = jnpf::Editor::EditorContext::WindowAction;
	using SceneAction = jnpf::Editor::EditorContext::SceneAction;
	using WindowStateProvider = jnpf::Editor::EditorContext::WindowStateProvider;
	using PrimitiveModelFactory = jnpf::Editor::EditorContext::PrimitiveModelFactory;
	using TextureLoader = jnpf::Editor::EditorContext::TextureLoader;
	using ModelLoader = jnpf::Editor::EditorContext::ModelLoader;
	using SimulationStateProvider = jnpf::Editor::EditorContext::SimulationStateProvider;
	using SimulationAction = jnpf::Editor::EditorContext::SimulationAction;
	using GameInputCaptureAction = jnpf::Editor::EditorContext::GameInputCaptureAction;
	using SceneProvider = std::function<jnpf::Scene::Scene&()>;
	using TextureProvider = std::function<ID3D11ShaderResourceView*()>;
	using ViewportResizeCallback = std::function<void(std::uint16_t, std::uint16_t)>;
	using ViewportInteractionCallback = std::function<void(bool)>;
	using ViewportStateCallback = std::function<void(bool, bool, bool, bool)>;

	EditorLayer(
		jnpf::Scene::Scene& scene,
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
		jnpf::Graphics::AssetRegistry* assets,
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
		WindowStateProvider isWindowMaximized);

	void OnEvent(Event& event) override;
	void OnUpdate(float deltaTime) override;
	void OnImGuiRender() override;

private:
	struct GizmoDragEntry
	{
		std::uint64_t EntityID = 0;
		jnpf::Editor::TransformValue Start;
	};

	void ApplyWorkbenchStyle();
	void DrawDockspace();
	void BuildDefaultDockLayout(ImGuiID dockspaceID, const ImVec2& dockspaceSize);
	void DrawViewportWindows();
	void UpdateSceneContextPopup();
	void DrawSceneContextPopup();
	void DrawLightWorkbench();
	void DrawCameraGizmos();
	void DrawLightGizmos();
	void DrawGizmo();
	void SelectEntityAt(const DirectX::XMFLOAT2& mousePosition);
	void DuplicateSelection();
	void CopySelection();
	void PasteSelection();
	void DeleteSelection();
	void BeginGizmoDrag();
	void CommitGizmoDrag();
	void SynchronizeActiveScene(const std::vector<std::uint64_t>* preservedSelection = nullptr);
	bool LoadPrefab(const std::string& path, jnpf::Editor::PrefabAsset& result) const;
	entt::entity CreateModelAssetEntity(
		std::uint64_t handle,
		const std::string& path,
		std::uint64_t parentID,
		const DirectX::XMFLOAT3* position);
	bool UpdateAssetPreview(
		const jnpf::Editor::AssetDragPayload& asset,
		const DirectX::XMFLOAT3& position);
	void CommitAssetPreview();
	void CancelAssetPreview();
	bool QueueEditorOperation(
		const char* title,
		const char* detail,
		std::function<bool()> action,
		std::function<void(bool)> completion = {});

	jnpf::Editor::EditorContext m_context;
	jnpf::Editor::CommandStack m_commandStack;
	jnpf::Editor::CommandStack m_suspendedEditorCommandStack;
	jnpf::Scene::Scene* m_editorScene = nullptr;
	SceneProvider m_activeSceneProvider;
	bool m_usingPlayScene = false;
	jnpf::Editor::TitleBar m_titleBar;
	jnpf::Editor::SceneHierarchyPanel m_sceneHierarchyPanel;
	jnpf::Editor::InspectorPanel m_inspectorPanel;
	jnpf::Editor::AssetBrowserPanel m_assetBrowserPanel;
	jnpf::Editor::GizmoToolbar m_gizmoToolbar;
	TextureProvider m_sceneTextureProvider;
	TextureProvider m_gameTextureProvider;
	ViewportResizeCallback m_resizeSceneViewport;
	ViewportResizeCallback m_resizeGameViewport;
	ViewportInteractionCallback m_sceneViewportInteraction;
	ViewportStateCallback m_viewportStateCallback;
	DirectX::XMUINT2 m_sceneViewportSize{640, 360};
	DirectX::XMUINT2 m_gameViewportSize{640, 360};
	DirectX::XMFLOAT2 m_sceneViewportOrigin{};
	DirectX::XMFLOAT3 m_sceneContextPosition{0.0f, 0.0f, 0.0f};
	bool m_sceneViewportHovered = false;
	bool m_sceneViewportVisible = false;
	bool m_gameViewportVisible = false;
	bool m_sceneViewportFocused = false;
	bool m_gameViewportFocused = false;
	bool m_gameViewportHovered = false;
	bool m_focusGameViewportRequested = false;
	bool m_focusSceneViewportRequested = false;
	bool m_sceneContextPressStartedInViewport = false;
	bool m_sceneContextPressDragged = false;
	int m_activeAxis = -1;
	DirectX::XMFLOAT2 m_dragStartMouse{};
	DirectX::XMFLOAT2 m_dragScreenDirection{};
	float m_dragWorldUnitsPerPixel = 0.0f;
	DirectX::XMFLOAT3 m_dragStartValue{};
	std::vector<GizmoDragEntry> m_gizmoDragEntries;
	std::vector<jnpf::Editor::EntitySnapshot> m_entityClipboard;
	bool m_workbenchStyleApplied = false;
	std::function<bool()> m_pendingEditorAction;
	std::function<void(bool)> m_pendingEditorCompletion;
	std::string m_pendingEditorTitle;
	std::string m_pendingEditorDetail;
	jnpf::Editor::UI::EditorLoadingWindow m_loadingWindow;
	bool m_gizmoDragActive = false;
	bool m_mouseWasDown = false;
	bool m_gizmoConsumedClick = false;
	bool m_defaultDockLayoutBuilt = false;
	std::uint64_t m_assetPreviewRootID = 0;
	std::uint64_t m_assetPreviewHandle = 0;
	jnpf::Graphics::AssetType m_assetPreviewType = jnpf::Graphics::AssetType::Unknown;
	bool m_assetPreviewTouchedThisFrame = false;
};
