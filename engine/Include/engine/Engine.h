#pragma once

#include "pch.h"

#include "ECS/Systems/HierarchySystem.h"
#include "ECS/Systems/FlyControllerSystem.h"
#include "ECS/Systems/RotatorSystem.h"
#include "Core/SimulationClock.h"
#include "Core/JobSystem.h"
#include "ECS/Systems/TransformPropagationSystem.h"
#include "Editor/EditorCamera.h"
#include "Graphics/AssetRegistry.h"
#include "Graphics/BgfxCallback.h"
#include "Graphics/Instancing/InstanceRenderer.h"
#include "Graphics/ProcGen/ProcGen.h"
#include "Graphics/RendererDebug.h"
#include "Graphics/ShadowRenderer.h"
#include "Graphics/Texture2D.h"
#include "Layer/LayerStack.h"
#include "Input/InputActions.h"
#include "Platform/IWindow.h"
#include "Platform/Win32Window.h"
#include "Physics/PhysicsWorld.h"
#include "Project/ProjectConfig.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <cstdint>

/**
 * @brief Owns the application lifetime, active scene, layer stack, and renderer.
 * @ingroup core
 *
 * The editor and runtime modes share this class. Editor mode installs editor
 * and debug layers; runtime mode runs the same scene and rendering systems
 * without editor UI.
 *
 * @warning Engine is currently Win32-specific and must be initialized before
 * loading GPU-backed models or textures.
 */
class Engine
{
public:
	/** @brief Selects which optional application layers are installed. */
	enum class AppMode { Editor, Runtime };
	enum class SimulationState { Stopped, Playing, Paused };

	explicit Engine(HINSTANCE hInstance, AppMode mode = AppMode::Editor);
	/** @return true after the window, bgfx, renderer resources, and layers initialize. */
	bool Initialize();
	/** @brief Runs the Win32 message and frame loop until the window closes. */
	int Run();
	/** @brief Releases layers and renderer resources. Safe after a failed initialization. */
	void Cleanup();

	std::shared_ptr<Model> LoadModel(const std::string& filepath);
	std::shared_ptr<Texture2D> LoadTexture(const std::string& filepath);

	void SetView(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& up);
	void SetProjection(float fovY, float aspect, float zn, float zf);
	void EnableSceneCameraControls(bool enable);
	void SetSceneCameraMoveSpeed(float unitsPerSecond) { m_sceneCamera.SetMoveSpeed(unitsPerSecond); }
	void SetSceneCameraLookSensitivity(float radiansPerPixel) { m_sceneCamera.SetLookSensitivity(radiansPerPixel); }
	/** @brief Sets directional shadow cascade count before Initialize(). */
	bool SetDirectionalShadowCascadeCount(std::uint8_t cascadeCount)
	{
		return !m_bgfxInitialized && m_shadowRenderer.SetDirectionalCascadeCount(cascadeCount);
	}

	EventManager& GetEventManager() { return EventManager::Get(); }
	void PushLayer(std::unique_ptr<Layer> layer) { m_layerStack.PushLayer(std::move(layer)); }
	void PushOverlay(std::unique_ptr<Layer> overlay) { m_layerStack.PushOverlay(std::move(overlay)); }
	std::unique_ptr<Layer> PopLayer() { return m_layerStack.PopLayer(); }
	std::unique_ptr<Layer> PopOverlay() { return m_layerStack.PopOverlay(); }

	enignE::Scene::Scene& GetScene() { return m_scene; }
	const enignE::Scene::Scene& GetScene() const { return m_scene; }
	enignE::Scene::Scene& GetSimulationScene() { return ActiveScene(); }
	const enignE::Scene::Scene& GetSimulationScene() const { return ActiveScene(); }
	SimulationState GetSimulationState() const { return m_simulationState; }
	bool BeginPlay();
	bool StopPlay();
	bool TogglePause();
	bool StepSimulation();
	bool IsGameInputCaptured() const
	{
		return m_appMode == AppMode::Runtime || m_gameInputCaptured;
	}
	void SetGameInputCaptured(bool captured);
	double GetFixedDeltaSeconds() const { return m_simulationClock.GetFixedDeltaSeconds(); }
	void SetFixedDeltaSeconds(double value) { m_simulationClock.SetFixedDeltaSeconds(value); }
	enignE::Input::InputActionMap& GetInputActions() { return m_inputActions; }
	const enignE::Input::InputActionMap& GetInputActions() const { return m_inputActions; }
	const std::vector<enignE::Physics::PhysicsContactEvent>& GetPhysicsContactEvents() const
	{
		return m_physicsWorld.GetContactEvents();
	}
	bool RaycastPhysics(
		const DirectX::XMFLOAT3& origin,
		const DirectX::XMFLOAT3& direction,
		float maxDistance,
		enignE::Physics::PhysicsRaycastHit& hit) const
	{
		return m_physicsWorld.Raycast(origin, direction, maxDistance, hit);
	}
	const enignE::Project::ProjectConfig& GetProjectConfig() const { return m_project; }
	const DirectX::XMFLOAT4X4& GetViewMatrix() const { return m_sceneCamera.GetViewMatrix(); }
	const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return m_sceneCamera.GetProjectionMatrix(); }
	const DirectX::XMFLOAT4X4& GetGameViewMatrix() const { return m_gameView; }
	const DirectX::XMFLOAT4X4& GetGameProjectionMatrix() const { return m_gameProjection; }
	DirectX::XMUINT2 GetViewportSize() const
	{
		return {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)};
	}
	float GetViewportAspectRatio() const
	{
		return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
	}

	void SetDirectionalLight(const DirectX::XMFLOAT3& dir, const DirectX::XMFLOAT3& color, float intensity)
	{
		m_fallbackLightDir = dir;
		m_fallbackLightColor = color;
		m_fallbackLightIntensity = intensity;
	}

	std::shared_ptr<Model> CreateModel(const MeshData& data);
	/** @brief Serializes the active scene without transient GPU resources. */
	bool SaveScene(const std::filesystem::path& path);
	/**
	 * @brief Replaces the active scene from disk and resolves render sources.
	 * @return false when parsing or scene reconstruction fails.
	 */
	bool LoadScene(const std::filesystem::path& path);

private:
	struct FrameCpuTimings
	{
		double UpdateMs = 0.0;
		double InputLayersMs = 0.0;
		double SimulationMs = 0.0;
		double TransformMs = 0.0;
		double CameraCleanupMs = 0.0;
		double RenderWorldMs = 0.0;
		double EditorUiMs = 0.0;
		double BgfxFrameMs = 0.0;
	};
	enignE::Scene::Scene& ActiveScene() { return *m_activeScene; }
	const enignE::Scene::Scene& ActiveScene() const { return *m_activeScene; }
	bool InitializeBgfx();
	bool CreateRendererResources();
	void DestroyRendererResources();
	void Tick();
	void Update(float deltaTime);
	void Render();
	void RegisterSceneCameraInput();
	void UpdateSceneCameraControls(float deltaTime);
	void UpdateWindowSize();
	void UpdateSceneCameraAndLight();
	void ResizeSceneViewport(std::uint16_t width, std::uint16_t height);
	void ResizeGameViewport(std::uint16_t width, std::uint16_t height);
	void ApplyViewportResizes();
	void ResizeRenderTarget(
		bgfx::FrameBufferHandle& frameBuffer,
		std::uint16_t& currentWidth,
		std::uint16_t& currentHeight,
		std::uint16_t width,
		std::uint16_t height);
	void DestroyViewportTargets();
	bgfx::TextureHandle GetSceneViewportTexture() const;
	bgfx::TextureHandle GetGameViewportTexture() const;
	void RenderWorld(
		bgfx::ViewId viewId,
		bgfx::FrameBufferHandle frameBuffer,
		std::uint16_t width,
		std::uint16_t height,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection,
		const DirectX::XMFLOAT3& cameraPosition,
		float cameraNear,
		float cameraFar,
		bgfx::ViewId shadowViewBase,
		bool renderShadowMaps,
		bool drawDebug);
	void ClearWorldView(
		bgfx::ViewId viewId,
		bgfx::FrameBufferHandle frameBuffer,
		std::uint16_t width,
		std::uint16_t height);
	bool SaveSceneWithDialog();
	bool LoadSceneWithDialog();
	void ResolveSceneResources(enignE::Scene::Scene& scene);
	bool RebuildPhysicsScene(enignE::Scene::Scene& scene);
	bool StepPhysicsScene(enignE::Scene::Scene& scene, float fixedDeltaTime);
	void DrawPhysicsDebug(bgfx::ViewId viewId);

	HINSTANCE m_hInstance;
	AppMode m_appMode;
	std::unique_ptr<IWindow> m_window;
	HWND m_hWnd = nullptr;
	int m_width = 1280;
	int m_height = 720;
	bool m_bgfxInitialized = false;
	bool m_imguiInitialized = false;
	bool m_debugDrawInitialized = false;
	std::unique_ptr<enignE::Graphics::BgfxCallback> m_bgfxCallback;

	static constexpr bgfx::ViewId SceneShadowViewBase = 0;
	static constexpr bgfx::ViewId SceneView = 4;
	static constexpr bgfx::ViewId GameShadowViewBase = 5;
	static constexpr bgfx::ViewId GameView = 9;
	static constexpr bgfx::ViewId EditorClearView = 30;
	static constexpr bgfx::ViewId ImGuiView = 31;
	bgfx::ProgramHandle m_sceneProgram = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_albedoUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_albedoSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_metallicRoughnessSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_normalSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightDirIntensityUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightColorMaterialUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightPositionRangeUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightTypeSpotUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_materialSurfaceUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_materialEmissiveUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_cameraPositionUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_debugViewUniform = BGFX_INVALID_HANDLE;
	std::array<bgfx::UniformHandle, enignE::Graphics::ShadowFrameData::MaxCascades>
		m_shadowSamplerUniforms{
			bgfx::UniformHandle{bgfx::kInvalidHandle}, bgfx::UniformHandle{bgfx::kInvalidHandle},
			bgfx::UniformHandle{bgfx::kInvalidHandle}, bgfx::UniformHandle{bgfx::kInvalidHandle}};
	std::array<bgfx::UniformHandle, enignE::Graphics::ShadowFrameData::MaxCascades>
		m_shadowMatrixUniforms{
			bgfx::UniformHandle{bgfx::kInvalidHandle}, bgfx::UniformHandle{bgfx::kInvalidHandle},
			bgfx::UniformHandle{bgfx::kInvalidHandle}, bgfx::UniformHandle{bgfx::kInvalidHandle}};
	bgfx::UniformHandle m_shadowParametersUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_cascadeSplitsUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_shadowMapInfoUniform = BGFX_INVALID_HANDLE;
	std::shared_ptr<Texture2D> m_defaultAlbedoTexture;
	RendererDebugSettings m_rendererDebugSettings;

	enignE::Editor::EditorCamera m_sceneCamera;
	DirectX::XMFLOAT4X4 m_gameView;
	DirectX::XMFLOAT4X4 m_gameProjection;
	DirectX::XMFLOAT3 m_gameCameraPosition = {0.0f, 0.0f, -3.0f};
	bool m_hasActiveGameCamera = false;
	float m_gameCameraNear = 0.1f;
	float m_gameCameraFar = 1000.0f;

	bgfx::FrameBufferHandle m_sceneViewportFrameBuffer = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle m_gameViewportFrameBuffer = BGFX_INVALID_HANDLE;
	std::uint16_t m_sceneViewportWidth = 640;
	std::uint16_t m_sceneViewportHeight = 360;
	std::uint16_t m_gameViewportWidth = 640;
	std::uint16_t m_gameViewportHeight = 360;
	std::uint16_t m_requestedSceneViewportWidth = 640;
	std::uint16_t m_requestedSceneViewportHeight = 360;
	std::uint16_t m_requestedGameViewportWidth = 640;
	std::uint16_t m_requestedGameViewportHeight = 360;
	bool m_sceneViewportHovered = false;
	bool m_editorShadowsForGameViewport = false;
	bool m_sceneViewportVisible = true;
	bool m_gameViewportVisible = false;

	bool m_sceneCameraControlsEnabled = false;

	DirectX::XMFLOAT3 m_lightDir = {0.577f, 0.577f, -0.577f};
	DirectX::XMFLOAT3 m_lightPosition = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 m_lightColor = {1.0f, 1.0f, 1.0f};
	float m_lightIntensity = 0.8f;
	float m_lightRange = 100.0f;
	float m_lightType = 0.0f;
	float m_lightSpotCosine = 0.0f;
	bool m_lightCastsShadows = true;
	float m_shadowStrength = 1.0f;
	float m_shadowBias = 0.0012f;
	float m_shadowNormalBias = 0.02f;
	float m_shadowDistance = 100.0f;
	DirectX::XMFLOAT3 m_fallbackLightDir = {0.577f, 0.577f, -0.577f};
	DirectX::XMFLOAT3 m_fallbackLightColor = {1.0f, 1.0f, 1.0f};
	float m_fallbackLightIntensity = 0.8f;

	enignE::Core::JobSystem m_jobs;
	LayerStack m_layerStack;
	enignE::Scene::Scene m_scene;
	enignE::Scene::Scene m_playScene{"PlayScene"};
	enignE::Scene::Scene* m_activeScene = &m_scene;
	enignE::Core::SimulationClock m_simulationClock;
	enignE::Input::InputActionMap m_inputActions;
	enignE::Physics::PhysicsWorld m_physicsWorld;
	std::uint64_t m_physicsSceneStructureVersion = 0;
	SimulationState m_simulationState = SimulationState::Stopped;
	bool m_gameInputCaptured = false;
	enignE::Project::ProjectConfig m_project;
	enignE::Graphics::AssetRegistry m_assets;
	std::filesystem::path m_currentScenePath;
	enignE::ECS::TransformPropagationSystem m_transformPropagationSystem;
	enignE::ECS::RotatorSystem m_rotatorSystem;
	enignE::ECS::FlyControllerSystem m_flyControllerSystem;
	float m_gameMouseDeltaX = 0.0f;
	float m_gameMouseDeltaY = 0.0f;
	enignE::Graphics::InstanceRenderer m_instanceRenderer;
	enignE::Graphics::ShadowRenderer m_shadowRenderer;
	FrameCpuTimings m_frameCpuTimings;
	const enignE::Scene::Scene* m_lastTransformScene = nullptr;
	std::uint64_t m_lastTransformVersion = 0;
	std::chrono::steady_clock::time_point m_lastFrameTime;
};
