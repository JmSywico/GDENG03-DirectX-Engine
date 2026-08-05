#include "Engine.h"

#include "Graphics/BgfxContext.h"
#include "Graphics/ModelLoader.h"
#include "Editor/ViewportRenderPlan.h"
#ifndef ENIGNE_RUNTIME_BUILD
#include "Layer/EditorLayer.h"
#include "Layer/DebugLayer.h"
#include "Editor/UI/UIScale.h"
#endif
#include "Scene/Serialization/SceneSerializer.h"

#include <debugdraw/debugdraw.h>
#ifndef ENIGNE_RUNTIME_BUILD
#include <imgui/imgui.h>
#endif

#include <fstream>
#include <cstring>
#include <limits>
#include <commdlg.h>
#include <vector>

namespace
{
	bgfx::ShaderHandle LoadShader(const char* filename)
	{
		const std::string path = std::string(ENIGNE_SHADER_DIR) + "/" + filename;
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			LOG_ERRORF("Unable to open bgfx shader: {}", path);
			return BGFX_INVALID_HANDLE;
		}

		const std::streamsize size = file.tellg();
		if (size <= 0)
		{
			LOG_ERRORF("bgfx shader '{}' is empty or has an invalid size: {}", path, size);
			return BGFX_INVALID_HANDLE;
		}
		if (size >= std::numeric_limits<uint32_t>::max())
		{
			LOG_ERRORF("bgfx shader '{}' is too large: {} bytes", path, size);
			return BGFX_INVALID_HANDLE;
		}

		file.seekg(0, std::ios::beg);
		std::vector<uint8_t> shaderData(static_cast<size_t>(size) + 1);
		if (!file.read(reinterpret_cast<char*>(shaderData.data()), size))
		{
			LOG_ERRORF("Unable to read bgfx shader: {}", path);
			return BGFX_INVALID_HANDLE;
		}
		shaderData[static_cast<size_t>(size)] = 0;
		const bgfx::Memory* memory = bgfx::copy(shaderData.data(), static_cast<uint32_t>(shaderData.size()));
		const bgfx::ShaderHandle shader = bgfx::createShader(memory);
		if (!bgfx::isValid(shader))
			LOG_ERRORF("bgfx failed to create shader from '{}'", path);
		return shader;
	}

	std::filesystem::path ChooseScenePath(HWND owner, bool save, const std::filesystem::path& initial)
	{
		wchar_t path[MAX_PATH] = {};
		if (!initial.empty())
			wcsncpy_s(path, initial.c_str(), _TRUNCATE);
		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = owner;
		dialog.lpstrFilter = L"enignE Scene (*.escene)\0*.escene\0All Files (*.*)\0*.*\0";
		dialog.lpstrFile = path;
		dialog.nMaxFile = static_cast<DWORD>(std::size(path));
		dialog.lpstrDefExt = L"escene";
		dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR
			| (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
		if (!accepted)
		{
			const DWORD error = CommDlgExtendedError();
			if (error != 0)
				LOG_ERRORF("Scene file dialog failed with common-dialog error 0x{:08X}", error);
		}
		return accepted ? std::filesystem::path(path) : std::filesystem::path{};
	}

	bool GetPhysicsWorldTransform(
		const enignE::Scene::Scene& scene,
		entt::entity entity,
		DirectX::XMFLOAT3& position,
		DirectX::XMFLOAT4& rotation)
	{
		const auto* transform = scene.GetComponent<enignE::Scene::TransformComponent>(entity);
		if (!transform) return false;
		DirectX::XMMATRIX world = transform->GetLocalMatrix();
		for (entt::entity parent = scene.GetParent(entity); parent != entt::null;
			parent = scene.GetParent(parent))
		{
			const auto* parentTransform =
				scene.GetComponent<enignE::Scene::TransformComponent>(parent);
			if (!parentTransform) return false;
			world *= parentTransform->GetLocalMatrix();
		}
		DirectX::XMVECTOR scale;
		DirectX::XMVECTOR orientation;
		DirectX::XMVECTOR translation;
		if (!DirectX::XMMatrixDecompose(&scale, &orientation, &translation, world))
			return false;
		DirectX::XMStoreFloat3(&position, translation);
		DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(orientation));
		return true;
	}

}

Engine::Engine(HINSTANCE hInstance, AppMode mode)
	: m_hInstance(hInstance)
	, m_appMode(mode)
	, m_scene("MainScene")
	, m_project(enignE::Project::ProjectConfig::Discover(std::filesystem::current_path()))
	, m_assets(m_project.GetRoot())
{
	m_assets.SetJobSystem(&m_jobs);
	if (m_appMode == AppMode::Runtime)
		m_simulationState = SimulationState::Playing;
	m_inputActions = enignE::Input::InputActionMap::CreateDefaults();
	const std::filesystem::path inputActionsPath = m_project.Resolve(m_project.InputActions);
	if (!m_inputActions.Load(inputActionsPath) && m_appMode == AppMode::Editor)
	{
		if (!m_inputActions.Save(inputActionsPath))
			LOG_WARNF("Unable to write default input actions to '{}'", inputActionsPath.string());
	}
	m_assets.LoadManifest(m_project.Resolve(m_project.AssetDirectory / "asset-manifest.json"));
	m_assets.RebuildFromMetadata(m_project.AssetDirectory);
	DirectX::XMStoreFloat4x4(&m_gameView, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&m_gameProjection, DirectX::XMMatrixIdentity());
}

bool Engine::Initialize()
{
	LOG_INFO("Engine::Initialize - creating window");
	m_window = std::make_unique<Win32Window>();
	if (!m_window->Create(m_hInstance, m_width, m_height, L"enignE"))
		return false;
	m_window->Show(SW_SHOW);
	m_hWnd = m_window->GetNativeHandle();
	m_width = m_window->GetWidth();
	m_height = m_window->GetHeight();
	if (m_appMode == AppMode::Runtime)
	{
		m_gameViewportWidth = static_cast<std::uint16_t>(std::clamp(m_width, 1, 65535));
		m_gameViewportHeight = static_cast<std::uint16_t>(std::clamp(m_height, 1, 65535));
	}

	InputManager::Get().Initialize(m_hWnd, true);
	for (std::uint32_t type = 0; type < static_cast<std::uint32_t>(EventType::COUNT); ++type)
	{
		EventManager::Get().AddListener(static_cast<EventType>(type), [this](Event& event)
		{
			m_layerStack.OnEvent(event);
		});
	}
	RegisterSceneCameraInput();
	if (m_appMode == AppMode::Runtime && !m_physicsWorld.Initialize())
	{
		LOG_ERROR("Engine::Initialize - Jolt physics world initialization failed");
		Cleanup();
		return false;
	}

	if (!InitializeBgfx() || !CreateRendererResources())
	{
		LOG_ERROR("Engine::Initialize - bgfx Direct3D 12 initialization failed");
		Cleanup();
		return false;
	}

	#ifndef ENIGNE_RUNTIME_BUILD
	if (m_appMode == AppMode::Editor)
	{
		ResizeSceneViewport(m_sceneViewportWidth, m_sceneViewportHeight);
		ResizeGameViewport(m_gameViewportWidth, m_gameViewportHeight);
		const float dpiScale = static_cast<float>(GetDpiForWindow(m_hWnd)) / 96.0f;
		enignE::Editor::UI::SetScaleFactor(dpiScale);
		imguiCreate(18.0f * dpiScale);
		m_imguiInitialized = true;
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		if (dpiScale != 1.0f)
			ImGui::GetStyle().ScaleAllSizes(dpiScale);
		PushOverlay(std::make_unique<EditorLayer>(
		m_scene,
		[this]() -> enignE::Scene::Scene& { return GetSimulationScene(); },
		[this]() -> const DirectX::XMFLOAT4X4& { return m_sceneCamera.GetViewMatrix(); },
		[this]() -> const DirectX::XMFLOAT4X4& { return m_sceneCamera.GetProjectionMatrix(); },
		[this]() { return GetViewportSize(); },
		[this]() { m_scene.MarkRenderDataChanged(); },
		[this]() { return GetSceneViewportTexture(); },
		[this]() { return GetGameViewportTexture(); },
		[this](std::uint16_t width, std::uint16_t height) { ResizeSceneViewport(width, height); },
		[this](std::uint16_t width, std::uint16_t height) { ResizeGameViewport(width, height); },
		[this](bool hovered) { m_sceneViewportHovered = hovered; },
		[this](bool sceneVisible, bool gameVisible, bool sceneFocused, bool gameFocused)
		{
			m_sceneViewportVisible = sceneVisible;
			m_gameViewportVisible = gameVisible;
			if (sceneVisible != gameVisible)
				m_editorShadowsForGameViewport = gameVisible;
			else if (gameFocused)
				m_editorShadowsForGameViewport = true;
			else if (sceneFocused)
				m_editorShadowsForGameViewport = false;
		},
		[this]() { return SaveSceneWithDialog(); },
		[this]() { return LoadSceneWithDialog(); },
		[this](const MeshData& data) { return CreateModel(data); },
		[this](const std::string& path) { return LoadTexture(path); },
		[this](const std::string& path) { return LoadModel(path); },
		&m_assets,
		m_project.Resolve(m_project.AssetDirectory),
		[this]() { return static_cast<enignE::Editor::SimulationState>(m_simulationState); },
		[this]() { return BeginPlay(); },
		[this]() { return StopPlay(); },
		[this]() { return TogglePause(); },
		[this]() { return StepSimulation(); },
		[this](bool captured) { SetGameInputCaptured(captured); },
		[this]() { return IsGameInputCaptured(); },
		[this]() { m_window->Minimize(); },
		[this]() { m_window->ToggleMaximizeRestore(); },
		[this]() { m_window->Close(); },
		[this]() { m_window->BeginTitleBarDrag(); },
		[this]() { return m_window->IsMaximized(); }));
		PushOverlay(std::make_unique<DebugLayer>(
		[this]
		{
			const enignE::Graphics::InstanceRendererProfile& profile = m_instanceRenderer.GetProfile();
			const enignE::Core::JobSystemStats jobs = m_jobs.GetStats();
			const enignE::Physics::PhysicsWorldStats physics = m_physicsWorld.GetStats();
			const std::size_t sceneTargetBytes = bgfx::isValid(m_sceneViewportFrameBuffer)
				? static_cast<std::size_t>(m_sceneViewportWidth) * m_sceneViewportHeight * 8u : 0u;
			const std::size_t gameTargetBytes = bgfx::isValid(m_gameViewportFrameBuffer)
				? static_cast<std::size_t>(m_gameViewportWidth) * m_gameViewportHeight * 8u : 0u;
			const std::size_t shadowTargetBytes = m_shadowRenderer.IsAvailable()
				? static_cast<std::size_t>(enignE::Graphics::ShadowRenderer::MapSize)
					* enignE::Graphics::ShadowRenderer::MapSize * 2u
					* m_shadowRenderer.GetDirectionalCascadeCount()
				: 0u;
			return ProfilerMetrics{
				.EntityCount = ActiveScene().GetEntityCount(),
				.VisibleChunkCount = m_instanceRenderer.GetVisibleChunkCount(),
				.SubmittedInstanceCount = m_instanceRenderer.GetSubmittedInstanceCount(),
				.BuildBatchesCpuMs = profile.BuildBatchesCpuMs,
				.DrawSubmitCpuMs = profile.DrawSubmitCpuMs,
				.UpdateCpuMs = m_frameCpuTimings.UpdateMs,
				.InputLayersCpuMs = m_frameCpuTimings.InputLayersMs,
				.SimulationCpuMs = m_frameCpuTimings.SimulationMs,
				.PhysicsStepCpuMs = physics.LastStepMilliseconds,
				.PhysicsBodyCount = physics.BodyCount,
				.PhysicsContactCount = physics.ContactCount,
				.PhysicsContactEventCount = physics.ContactEventCount,
				.TransformCpuMs = m_frameCpuTimings.TransformMs,
				.TransformVisitedCount = m_transformPropagationSystem.GetVisitedTransformCount(),
				.CameraCleanupCpuMs = m_frameCpuTimings.CameraCleanupMs,
				.RenderWorldCpuMs = m_frameCpuTimings.RenderWorldMs,
				.EditorUiCpuMs = m_frameCpuTimings.EditorUiMs,
				.BgfxFrameCpuMs = m_frameCpuTimings.BgfxFrameMs,
				.SceneViewportRendered = m_appMode == AppMode::Editor && m_sceneViewportVisible,
				.GameViewportRendered = m_appMode != AppMode::Editor || m_gameViewportVisible,
				.JobWorkerCount = jobs.WorkerCount,
				.QueuedJobCount = jobs.QueuedJobs,
				.ActiveJobCount = jobs.ActiveJobs,
				.PeakQueuedJobCount = jobs.PeakQueuedJobs,
				.CompletedJobCount = jobs.CompletedJobs,
				.AverageJobExecutionMs = jobs.AverageExecutionMs,
				.VisibleBatchCount = profile.VisibleBatchCount,
				.SubmittedDrawCount = profile.SubmittedDrawCount,
				.UploadedInstanceCount = profile.UploadedInstanceCount,
				.UploadedInstanceBytes = profile.UploadedInstanceBytes,
				.LargestBatchInstanceCount = profile.LargestBatchInstanceCount,
				.SplitDrawCount = profile.SplitDrawCount,
				.ExhaustedBatchCount = profile.ExhaustedBatchCount,
				.SkippedInstanceCount = profile.SkippedInstanceCount,
				.ShadowPassCount = profile.ShadowPassCount,
				.ShadowDrawCount = profile.ShadowDrawCount,
				.ShadowInstanceCount = profile.ShadowInstanceCount,
				.ShadowSplitDrawCount = profile.ShadowSplitDrawCount,
				.ShadowExhaustedBatchCount = profile.ShadowExhaustedBatchCount,
				.ShadowSkippedInstanceCount = profile.ShadowSkippedInstanceCount,
				.ShadowCascadeCount = m_shadowRenderer.GetDirectionalCascadeCount(),
				.ViewportTargetBytes = sceneTargetBytes + gameTargetBytes,
				.ShadowTargetBytes = shadowTargetBytes
			};
		},
			&m_rendererDebugSettings));
	}
	#endif

	m_lastFrameTime = std::chrono::steady_clock::now();
	LOG_INFO("Engine::Initialize - bgfx Direct3D 12 renderer ready");
	return true;
}

bool Engine::InitializeBgfx()
{
	bgfx::renderFrame();

	m_bgfxCallback = std::make_unique<enignE::Graphics::BgfxCallback>(
		enignE::Graphics::GetDefaultBgfxCacheDirectory());
	bgfx::Init init;
	init.type = bgfx::RendererType::Direct3D12;
	init.vendorId = BGFX_PCI_ID_NONE;
	init.platformData.nwh = m_hWnd;
	init.platformData.ndt = nullptr;
	init.callback = m_bgfxCallback.get();
	init.limits.maxEncoders = 2;
	// Shared by editor Scene/Game color passes, shadow cascades, debug drawing,
	// and ImGui. The stress fixture alone submits 4.27 MiB of instance data per pass.
	init.limits.maxTransientVbSize = 64u << 20;
	init.limits.maxTransientIbSize = 1u << 20;
	init.limits.minUniformBufferSize = 256u << 10;
	init.resolution.width = static_cast<uint32_t>(m_width);
	init.resolution.height = static_cast<uint32_t>(m_height);
	init.resolution.reset = BGFX_RESET_VSYNC;
	if (!bgfx::init(init))
	{
		m_bgfxCallback.reset();
		return false;
	}

	m_bgfxInitialized = true;
	enignE::Graphics::SetBgfxInitialized(true);
	if (bgfx::getRendererType() != bgfx::RendererType::Direct3D12)
	{
		LOG_ERROR("bgfx initialized a renderer other than Direct3D 12");
		return false;
	}

	bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_PROFILER);
	bgfx::setViewName(SceneView, "Scene");
	bgfx::setViewName(GameView, "Game");
	for (std::uint8_t index = 0; index < enignE::Graphics::ShadowFrameData::MaxCascades; ++index)
	{
		bgfx::setViewName(static_cast<bgfx::ViewId>(SceneShadowViewBase + index), "Scene Shadow");
		bgfx::setViewName(static_cast<bgfx::ViewId>(GameShadowViewBase + index), "Game Shadow");
	}
	bgfx::setViewName(EditorClearView, "Editor Backbuffer Clear");
	bgfx::setViewClear(SceneView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x334c66ff, 1.0f, 0);
	bgfx::setViewClear(GameView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x18212bff, 1.0f, 0);
	bgfx::setViewClear(EditorClearView, BGFX_CLEAR_COLOR, 0x11151bff, 1.0f, 0);
	ddInit();
	m_debugDrawInitialized = true;
	return true;
}

bool Engine::CreateRendererResources()
{
	const bgfx::ShaderHandle vertexShader = LoadShader("vs_scene.bin");
	const bgfx::ShaderHandle fragmentShader = LoadShader("fs_scene.bin");
	if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader))
	{
		if (bgfx::isValid(vertexShader))
			bgfx::destroy(vertexShader);
		if (bgfx::isValid(fragmentShader))
			bgfx::destroy(fragmentShader);
		return false;
	}

	m_sceneProgram = bgfx::createProgram(vertexShader, fragmentShader, true);
	if (!bgfx::isValid(m_sceneProgram))
		LOG_ERROR("bgfx failed to create scene shader program");

	m_albedoUniform = bgfx::createUniform("u_albedo", bgfx::UniformType::Vec4);
	if (!bgfx::isValid(m_albedoUniform))
		LOG_ERROR("bgfx failed to create uniform 'u_albedo'");

	m_albedoSampler = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
	if (!bgfx::isValid(m_albedoSampler))
		LOG_ERROR("bgfx failed to create sampler 's_albedo'");
	m_metallicRoughnessSampler =
		bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
	if (!bgfx::isValid(m_metallicRoughnessSampler))
		LOG_ERROR("bgfx failed to create sampler 's_metallicRoughness'");
	m_normalSampler = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);

	m_lightDirIntensityUniform = bgfx::createUniform("u_lightDirIntensity", bgfx::UniformType::Vec4);
	if (!bgfx::isValid(m_lightDirIntensityUniform))
		LOG_ERROR("bgfx failed to create uniform 'u_lightDirIntensity'");

	m_lightColorMaterialUniform = bgfx::createUniform("u_lightColorMaterial", bgfx::UniformType::Vec4);
	if (!bgfx::isValid(m_lightColorMaterialUniform))
		LOG_ERROR("bgfx failed to create uniform 'u_lightColorMaterial'");
	m_lightPositionRangeUniform = bgfx::createUniform("u_lightPositionRange", bgfx::UniformType::Vec4);
	m_lightTypeSpotUniform = bgfx::createUniform("u_lightTypeSpot", bgfx::UniformType::Vec4);
	m_materialSurfaceUniform = bgfx::createUniform("u_materialSurface", bgfx::UniformType::Vec4);
	m_materialEmissiveUniform = bgfx::createUniform("u_materialEmissive", bgfx::UniformType::Vec4);
	m_cameraPositionUniform = bgfx::createUniform("u_cameraPosition", bgfx::UniformType::Vec4);

	m_debugViewUniform = bgfx::createUniform("u_debugView", bgfx::UniformType::Vec4);
	if (!bgfx::isValid(m_debugViewUniform))
		LOG_ERROR("bgfx failed to create uniform 'u_debugView'");
	for (std::uint8_t index = 0; index < enignE::Graphics::ShadowFrameData::MaxCascades; ++index)
	{
		const std::string samplerName = "s_shadowMap" + std::to_string(index);
		const std::string matrixName = "u_shadowMtx" + std::to_string(index);
		m_shadowSamplerUniforms[index] = bgfx::createUniform(samplerName.c_str(), bgfx::UniformType::Sampler);
		m_shadowMatrixUniforms[index] = bgfx::createUniform(matrixName.c_str(), bgfx::UniformType::Mat4);
	}
	m_shadowParametersUniform = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
	m_cascadeSplitsUniform = bgfx::createUniform("u_cascadeSplits", bgfx::UniformType::Vec4);
	m_shadowMapInfoUniform = bgfx::createUniform("u_shadowMapInfo", bgfx::UniformType::Vec4);
	if (!m_shadowRenderer.Initialize())
		LOG_WARN("Shadow renderer initialization failed; continuing without real-time shadows");

	m_defaultAlbedoTexture = Texture2D::CreateSolidColor(0xffffffff, "DefaultWhiteAlbedo");
	if (!m_defaultAlbedoTexture)
		LOG_ERROR("bgfx failed to create the default white albedo texture");

	const bool resourcesValid = bgfx::isValid(m_sceneProgram)
		&& bgfx::isValid(m_albedoUniform)
		&& bgfx::isValid(m_albedoSampler)
		&& bgfx::isValid(m_metallicRoughnessSampler)
		&& bgfx::isValid(m_normalSampler)
		&& bgfx::isValid(m_lightDirIntensityUniform)
		&& bgfx::isValid(m_lightColorMaterialUniform)
		&& bgfx::isValid(m_lightPositionRangeUniform)
		&& bgfx::isValid(m_lightTypeSpotUniform)
		&& bgfx::isValid(m_materialSurfaceUniform)
		&& bgfx::isValid(m_materialEmissiveUniform)
		&& bgfx::isValid(m_cameraPositionUniform)
		&& bgfx::isValid(m_debugViewUniform)
		&& bgfx::isValid(m_shadowParametersUniform)
		&& bgfx::isValid(m_cascadeSplitsUniform)
		&& bgfx::isValid(m_shadowMapInfoUniform)
		&& std::all_of(
			m_shadowSamplerUniforms.begin(), m_shadowSamplerUniforms.end(),
			[](bgfx::UniformHandle handle) { return bgfx::isValid(handle); })
		&& std::all_of(
			m_shadowMatrixUniforms.begin(), m_shadowMatrixUniforms.end(),
			[](bgfx::UniformHandle handle) { return bgfx::isValid(handle); })
		&& m_defaultAlbedoTexture;
	if (!resourcesValid)
		DestroyRendererResources();
	return resourcesValid;
}

void Engine::DestroyRendererResources()
{
	if (!m_bgfxInitialized)
		return;
	m_shadowRenderer.Shutdown();
	m_defaultAlbedoTexture.reset();
	for (bgfx::UniformHandle& handle : m_shadowSamplerUniforms)
	{
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		handle = BGFX_INVALID_HANDLE;
	}
	for (bgfx::UniformHandle& handle : m_shadowMatrixUniforms)
	{
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		handle = BGFX_INVALID_HANDLE;
	}
	if (bgfx::isValid(m_shadowParametersUniform)) bgfx::destroy(m_shadowParametersUniform);
	if (bgfx::isValid(m_cascadeSplitsUniform)) bgfx::destroy(m_cascadeSplitsUniform);
	if (bgfx::isValid(m_shadowMapInfoUniform)) bgfx::destroy(m_shadowMapInfoUniform);
	if (bgfx::isValid(m_debugViewUniform)) bgfx::destroy(m_debugViewUniform);
	if (bgfx::isValid(m_lightColorMaterialUniform)) bgfx::destroy(m_lightColorMaterialUniform);
	if (bgfx::isValid(m_lightPositionRangeUniform)) bgfx::destroy(m_lightPositionRangeUniform);
	if (bgfx::isValid(m_lightTypeSpotUniform)) bgfx::destroy(m_lightTypeSpotUniform);
	if (bgfx::isValid(m_materialSurfaceUniform)) bgfx::destroy(m_materialSurfaceUniform);
	if (bgfx::isValid(m_materialEmissiveUniform)) bgfx::destroy(m_materialEmissiveUniform);
	if (bgfx::isValid(m_cameraPositionUniform)) bgfx::destroy(m_cameraPositionUniform);
	if (bgfx::isValid(m_lightDirIntensityUniform)) bgfx::destroy(m_lightDirIntensityUniform);
	if (bgfx::isValid(m_albedoSampler)) bgfx::destroy(m_albedoSampler);
	if (bgfx::isValid(m_metallicRoughnessSampler)) bgfx::destroy(m_metallicRoughnessSampler);
	if (bgfx::isValid(m_normalSampler)) bgfx::destroy(m_normalSampler);
	if (bgfx::isValid(m_albedoUniform)) bgfx::destroy(m_albedoUniform);
	if (bgfx::isValid(m_sceneProgram)) bgfx::destroy(m_sceneProgram);
	m_debugViewUniform = BGFX_INVALID_HANDLE;
	m_shadowParametersUniform = BGFX_INVALID_HANDLE;
	m_cascadeSplitsUniform = BGFX_INVALID_HANDLE;
	m_shadowMapInfoUniform = BGFX_INVALID_HANDLE;
	m_lightColorMaterialUniform = BGFX_INVALID_HANDLE;
	m_lightPositionRangeUniform = BGFX_INVALID_HANDLE;
	m_lightTypeSpotUniform = BGFX_INVALID_HANDLE;
	m_materialSurfaceUniform = BGFX_INVALID_HANDLE;
	m_materialEmissiveUniform = BGFX_INVALID_HANDLE;
	m_cameraPositionUniform = BGFX_INVALID_HANDLE;
	m_lightDirIntensityUniform = BGFX_INVALID_HANDLE;
	m_albedoSampler = BGFX_INVALID_HANDLE;
	m_metallicRoughnessSampler = BGFX_INVALID_HANDLE;
	m_normalSampler = BGFX_INVALID_HANDLE;
	m_albedoUniform = BGFX_INVALID_HANDLE;
	m_sceneProgram = BGFX_INVALID_HANDLE;
}

void Engine::Tick()
{
	const auto now = std::chrono::steady_clock::now();
	float delta = std::chrono::duration<float>(now - m_lastFrameTime).count();
	if (delta <= 0.0f)
		delta = 0.000001f;
	m_lastFrameTime = now;

	FrameBeginEvent beginEvent(delta);
	EventManager::Get().FireEvent(beginEvent);
	Update(delta);
	Render();
	FrameEndEvent endEvent(delta);
	EventManager::Get().FireEvent(endEvent);
}

void Engine::Update(float deltaTime)
{
	m_jobs.DrainMainThread();
	using Clock = std::chrono::steady_clock;
	const auto updateStart = Clock::now();
	const auto millisecondsSince = [](Clock::time_point start)
	{
		return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	};
	const auto inputLayersStart = Clock::now();
	UpdateWindowSize();
	InputManager::Get().PollKeyboard();
	EventManager::Get().DispatchQueuedEvents();
	if (m_gameInputCaptured && !InputManager::Get().HasFocus())
		m_gameInputCaptured = false;
	m_inputActions.Update(InputManager::Get());
	if (IsGameInputCaptured())
		m_flyControllerSystem.ApplyLook(ActiveScene(), m_gameMouseDeltaX, m_gameMouseDeltaY);
	m_gameMouseDeltaX = 0.0f;
	m_gameMouseDeltaY = 0.0f;
	UpdateSceneCameraControls(deltaTime);
	m_layerStack.OnUpdate(deltaTime);
	m_frameCpuTimings.InputLayersMs = millisecondsSince(inputLayersStart);
	const auto simulationStart = Clock::now();
	const bool simulationRunning = m_appMode == AppMode::Runtime
		|| m_simulationState == SimulationState::Playing;
	const std::uint32_t fixedSteps = m_simulationClock.Advance(deltaTime, simulationRunning);
	for (std::uint32_t step = 0; step < fixedSteps; ++step)
	{
		m_layerStack.OnFixedUpdate(static_cast<float>(m_simulationClock.GetFixedDeltaSeconds()));
		m_rotatorSystem.FixedUpdate(ActiveScene(), static_cast<float>(m_simulationClock.GetFixedDeltaSeconds()));
		m_flyControllerSystem.FixedUpdate(ActiveScene(), m_inputActions,
			static_cast<float>(m_simulationClock.GetFixedDeltaSeconds()), IsGameInputCaptured());
		if (m_physicsWorld.IsInitialized()
			&& !StepPhysicsScene(
				ActiveScene(), static_cast<float>(m_simulationClock.GetFixedDeltaSeconds())))
		{
			LOG_ERROR("Fixed-step physics update failed");
		}
	}
	m_frameCpuTimings.SimulationMs = millisecondsSince(simulationStart);
	const auto transformStart = Clock::now();
	enignE::Scene::Scene& activeScene = ActiveScene();
	const std::uint64_t transformVersion = activeScene.GetTransformVersion();
	if (m_lastTransformScene != &activeScene || m_lastTransformVersion != transformVersion)
	{
		enignE::Scene::DirtyTransformSet dirtyTransforms = activeScene.TakeDirtyTransforms();
		if (dirtyTransforms.All || m_lastTransformScene != &activeScene)
			m_transformPropagationSystem.Update(activeScene.GetRegistry(), deltaTime, &m_jobs);
		else
			m_transformPropagationSystem.UpdateDirty(
				activeScene.GetRegistry(), dirtyTransforms.Roots, deltaTime, &m_jobs);
		if (m_transformPropagationSystem.DidUpdateRenderableTransform())
			activeScene.MarkRenderDataChanged();
		m_lastTransformScene = &activeScene;
		m_lastTransformVersion = transformVersion;
	}
	else
	{
		m_transformPropagationSystem.UpdateDirty(activeScene.GetRegistry(), {}, deltaTime);
	}
	m_frameCpuTimings.TransformMs = millisecondsSince(transformStart);
	const auto cameraCleanupStart = Clock::now();
	ApplyViewportResizes();
	UpdateSceneCameraAndLight();

	activeScene.FlushDestroyQueue();
	m_frameCpuTimings.CameraCleanupMs = millisecondsSince(cameraCleanupStart);
	m_frameCpuTimings.UpdateMs = millisecondsSince(updateStart);
}

void Engine::UpdateSceneCameraAndLight()
{
	using namespace DirectX;
	auto& scene = ActiveScene();
	const auto& settings = scene.GetSettings();
	m_hasActiveGameCamera = false;
	if (const entt::entity cameraEntity = scene.FindEntityByID(settings.ActiveCameraEntityID);
		cameraEntity != entt::null)
	{
		const auto* camera = scene.GetComponent<enignE::Scene::CameraComponent>(cameraEntity);
		const auto* transform = scene.GetComponent<enignE::Scene::TransformComponent>(cameraEntity);
		if (camera && transform)
		{
			const XMMATRIX world = transform->GetWorldMatrix();
			XMVECTOR scale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
			XMVECTOR rotation = XMQuaternionIdentity();
			XMVECTOR translation = world.r[3];
			XMMATRIX cameraWorld = world;
			if (XMMatrixDecompose(&scale, &rotation, &translation, world))
			{
				cameraWorld =
					XMMatrixRotationQuaternion(rotation)
					* XMMatrixTranslationFromVector(translation);
			}
			XMStoreFloat4x4(&m_gameView, XMMatrixInverse(nullptr, cameraWorld));
			XMStoreFloat4x4(
				&m_gameProjection,
				camera->ToCamera().BuildPerspective(
					static_cast<float>(m_gameViewportWidth)
						/ static_cast<float>(std::max<std::uint16_t>(1, m_gameViewportHeight))));
			XMStoreFloat3(&m_gameCameraPosition, translation);
			m_gameCameraNear = std::max(0.001f, camera->NearPlane);
			m_gameCameraFar = std::max(m_gameCameraNear + 0.001f, camera->FarPlane);
			m_hasActiveGameCamera = true;
		}
	}
	if (!m_hasActiveGameCamera)
	{
		XMStoreFloat4x4(&m_gameView, XMMatrixIdentity());
		XMStoreFloat4x4(&m_gameProjection, XMMatrixIdentity());
		m_gameCameraPosition = {0.0f, 0.0f, 0.0f};
		m_gameCameraNear = 0.1f;
		m_gameCameraFar = 1000.0f;
	}

	m_lightDir = m_fallbackLightDir;
	m_lightPosition = {0.0f, 0.0f, 0.0f};
	m_lightColor = m_fallbackLightColor;
	m_lightIntensity = m_fallbackLightIntensity;
	m_lightRange = 100.0f;
	m_lightType = 0.0f;
	m_lightSpotCosine = 0.0f;
	m_lightCastsShadows = true;
	m_shadowStrength = 1.0f;
	m_shadowBias = 0.0012f;
	m_shadowNormalBias = 0.02f;
	m_shadowDistance = 100.0f;
	if (const entt::entity lightEntity = scene.FindEntityByID(settings.ActiveLightEntityID);
		lightEntity != entt::null)
	{
		const auto* light = scene.GetComponent<enignE::Scene::LightComponent>(lightEntity);
		const auto* transform = scene.GetComponent<enignE::Scene::TransformComponent>(lightEntity);
		if (light && transform)
		{
			const XMMATRIX world = transform->GetWorldMatrix();
			XMVECTOR scale{};
			XMVECTOR rotation = XMQuaternionIdentity();
			XMVECTOR translation = world.r[3];
			XMMatrixDecompose(&scale, &rotation, &translation, world);
			XMStoreFloat3(
				&m_lightDir,
				XMVector3Normalize(XMVector3TransformNormal(
					XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
					XMMatrixRotationQuaternion(rotation))));
			XMStoreFloat3(&m_lightPosition, translation);
			m_lightColor = light->Color;
			m_lightIntensity = light->Intensity;
			m_lightRange = std::max(light->Range, 0.0001f);
			m_lightType = static_cast<float>(light->LightType);
			m_lightSpotCosine = std::cos(
				XMConvertToRadians(std::clamp(light->SpotAngle, 1.0f, 179.0f) * 0.5f));
			m_lightCastsShadows = light->bCastShadows;
			m_shadowStrength = light->ShadowStrength;
			m_shadowBias = light->ShadowBias;
			m_shadowNormalBias = light->ShadowNormalBias;
			m_shadowDistance = light->ShadowDistance;
		}
	}
}

void Engine::UpdateWindowSize()
{
	const int width = m_window ? m_window->GetWidth() : m_width;
	const int height = m_window ? m_window->GetHeight() : m_height;
	if (width == m_width && height == m_height)
		return;

	m_width = width;
	m_height = height;
	if (m_bgfxInitialized)
		bgfx::reset(static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), BGFX_RESET_VSYNC);
	if (m_appMode == AppMode::Editor)
	{
		m_sceneCamera.SetAspect(
			static_cast<float>(m_sceneViewportWidth) / static_cast<float>(m_sceneViewportHeight));
	}
	else
	{
		m_gameViewportWidth = static_cast<std::uint16_t>(std::clamp(m_width, 1, 65535));
		m_gameViewportHeight = static_cast<std::uint16_t>(std::clamp(m_height, 1, 65535));
	}
}

void Engine::Render()
{
	using Clock = std::chrono::steady_clock;
	const auto millisecondsSince = [](Clock::time_point start)
	{
		return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	};
	const auto worldStart = Clock::now();
	m_instanceRenderer.ResetShadowProfile();
	if (m_appMode == AppMode::Editor)
	{
		const enignE::Editor::ViewportRenderPlan renderPlan =
			enignE::Editor::ViewportRenderPlan::Build(
				m_sceneViewportVisible,
				m_gameViewportVisible,
				m_hasActiveGameCamera,
				m_editorShadowsForGameViewport);
		if (renderPlan.RenderScene)
		{
			RenderWorld(
				SceneView,
				m_sceneViewportFrameBuffer,
				m_sceneViewportWidth,
				m_sceneViewportHeight,
				m_sceneCamera.GetViewMatrix(),
				m_sceneCamera.GetProjectionMatrix(),
				m_sceneCamera.GetPosition(),
				m_sceneCamera.GetNearPlane(),
				m_sceneCamera.GetFarPlane(),
				SceneShadowViewBase,
				renderPlan.SceneOwnsShadows,
				true);
		}
		if (renderPlan.RenderGame)
		{
			RenderWorld(
				GameView,
				m_gameViewportFrameBuffer,
				m_gameViewportWidth,
				m_gameViewportHeight,
				m_gameView,
				m_gameProjection,
				m_gameCameraPosition,
				m_gameCameraNear,
				m_gameCameraFar,
				GameShadowViewBase,
				renderPlan.GameOwnsShadows,
				false);
		}
		else if (renderPlan.ClearGame)
		{
			ClearWorldView(
				GameView,
				m_gameViewportFrameBuffer,
				m_gameViewportWidth,
				m_gameViewportHeight);
		}
	}
	else
	{
		if (m_hasActiveGameCamera)
		{
			RenderWorld(
				GameView,
				BGFX_INVALID_HANDLE,
				static_cast<std::uint16_t>(std::max(1, m_width)),
				static_cast<std::uint16_t>(std::max(1, m_height)),
				m_gameView,
				m_gameProjection,
				m_gameCameraPosition,
				m_gameCameraNear,
				m_gameCameraFar,
				GameShadowViewBase,
				true,
				false);
		}
		else
		{
			ClearWorldView(
				GameView,
				BGFX_INVALID_HANDLE,
				static_cast<std::uint16_t>(std::max(1, m_width)),
				static_cast<std::uint16_t>(std::max(1, m_height)));
		}
	}
	m_frameCpuTimings.RenderWorldMs = millisecondsSince(worldStart);

	#ifndef ENIGNE_RUNTIME_BUILD
	const auto editorUiStart = Clock::now();
	if (m_imguiInitialized)
	{
		RECT clientRect{};
		GetClientRect(m_hWnd, &clientRect);
		const uint16_t width = static_cast<uint16_t>(std::max<LONG>(1, clientRect.right - clientRect.left));
		const uint16_t height = static_cast<uint16_t>(std::max<LONG>(1, clientRect.bottom - clientRect.top));
		bgfx::setViewFrameBuffer(EditorClearView, BGFX_INVALID_HANDLE);
		bgfx::setViewRect(EditorClearView, 0, 0, width, height);
		bgfx::touch(EditorClearView);

		const bool gameOwnsMouse = m_appMode == AppMode::Editor && m_gameInputCaptured;
		const POINT mousePosition = gameOwnsMouse
			? POINT{std::numeric_limits<LONG>::lowest(), std::numeric_limits<LONG>::lowest()}
			: InputManager::Get().GetMousePosition();
		uint8_t mouseButtons = 0;
		if (!gameOwnsMouse)
		{
			if (InputManager::Get().IsMouseButtonDown(1)) mouseButtons |= IMGUI_MBUT_LEFT;
			if (InputManager::Get().IsMouseButtonDown(2)) mouseButtons |= IMGUI_MBUT_RIGHT;
			if (InputManager::Get().IsMouseButtonDown(3)) mouseButtons |= IMGUI_MBUT_MIDDLE;
		}

		// bgfx's ImGui backend only supplies keyboard modifiers when USE_ENTRY is
		// enabled. enignE owns Win32 input directly, so forward modifier state here.
		ImGuiIO& imguiIO = ImGui::GetIO();
		if (gameOwnsMouse)
			imguiIO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		else
			imguiIO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		imguiIO.AddKeyEvent(
			ImGuiMod_Ctrl,
			InputManager::Get().IsKeyDown(VK_CONTROL));
		imguiIO.AddKeyEvent(
			ImGuiMod_Shift,
			InputManager::Get().IsKeyDown(VK_SHIFT));
		imguiIO.AddKeyEvent(
			ImGuiMod_Alt,
			InputManager::Get().IsKeyDown(VK_MENU));
		imguiIO.AddKeyEvent(
			ImGuiMod_Super,
			InputManager::Get().IsKeyDown(VK_LWIN)
				|| InputManager::Get().IsKeyDown(VK_RWIN));
		const auto forwardKey = [&imguiIO](ImGuiKey key, int virtualKey)
		{
			imguiIO.AddKeyEvent(key, InputManager::Get().IsKeyDown(virtualKey));
			imguiIO.SetKeyEventNativeData(key, virtualKey, 0);
		};
		forwardKey(ImGuiKey_Tab, VK_TAB);
		forwardKey(ImGuiKey_LeftArrow, VK_LEFT);
		forwardKey(ImGuiKey_RightArrow, VK_RIGHT);
		forwardKey(ImGuiKey_UpArrow, VK_UP);
		forwardKey(ImGuiKey_DownArrow, VK_DOWN);
		forwardKey(ImGuiKey_PageUp, VK_PRIOR);
		forwardKey(ImGuiKey_PageDown, VK_NEXT);
		forwardKey(ImGuiKey_Home, VK_HOME);
		forwardKey(ImGuiKey_End, VK_END);
		forwardKey(ImGuiKey_Insert, VK_INSERT);
		forwardKey(ImGuiKey_Delete, VK_DELETE);
		forwardKey(ImGuiKey_Backspace, VK_BACK);
		forwardKey(ImGuiKey_Space, VK_SPACE);
		forwardKey(ImGuiKey_Enter, VK_RETURN);
		forwardKey(ImGuiKey_Escape, VK_ESCAPE);
		forwardKey(ImGuiKey_A, 'A');
		forwardKey(ImGuiKey_C, 'C');
		forwardKey(ImGuiKey_V, 'V');
		forwardKey(ImGuiKey_X, 'X');
		forwardKey(ImGuiKey_Y, 'Y');
		forwardKey(ImGuiKey_Z, 'Z');

		imguiBeginFrame(
			mousePosition.x,
			mousePosition.y,
			mouseButtons,
			InputManager::Get().GetMouseWheelPosition(),
			width,
			height,
			InputManager::Get().ConsumeInputCharacter(),
			ImGuiView);
		m_layerStack.OnImGuiRender();
		imguiEndFrame();
	}
	m_frameCpuTimings.EditorUiMs = millisecondsSince(editorUiStart);
	#endif

	const auto bgfxFrameStart = Clock::now();
	bgfx::frame();
	m_frameCpuTimings.BgfxFrameMs = millisecondsSince(bgfxFrameStart);
}

void Engine::RenderWorld(
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
	bool drawDebug)
{
	if (width == 0 || height == 0)
		return;

	m_instanceRenderer.BuildBatches(
		ActiveScene().GetRegistry(),
		ActiveScene().GetRenderVersion(),
		cameraPosition,
		view,
		projection,
		&m_jobs);

	enignE::Graphics::ShadowFrameData shadowFrame;
	if (renderShadowMaps && m_lightCastsShadows && m_shadowRenderer.IsAvailable())
	{
		if (m_lightType < 0.5f)
		{
			shadowFrame = m_shadowRenderer.BuildDirectional(
				view,
				projection,
				cameraNear,
				cameraFar,
				m_lightDir,
				m_shadowDistance);
		}
		else if (m_lightType > 1.5f)
		{
			const float spotAngle = DirectX::XMConvertToDegrees(
				2.0f * std::acos(std::clamp(m_lightSpotCosine, -1.0f, 1.0f)));
			shadowFrame = m_shadowRenderer.BuildSpot(
				m_lightPosition, m_lightDir, spotAngle, m_lightRange);
		}
	}
	if (shadowFrame.IsValid())
	{
		for (std::uint8_t index = 0; index < shadowFrame.ViewCount; ++index)
		{
			const bgfx::ViewId shadowView = static_cast<bgfx::ViewId>(shadowViewBase + index);
			m_shadowRenderer.ConfigurePass(shadowView, index);
			bgfx::setViewTransform(
				shadowView,
				&shadowFrame.Views[index].View,
				&shadowFrame.Views[index].Projection);
			bgfx::touch(shadowView);
			m_instanceRenderer.DrawShadow(
				shadowView,
				m_shadowRenderer.GetProgram(),
				shadowFrame.Views[index].View,
				shadowFrame.Views[index].Projection);
		}
	}

	bgfx::setViewFrameBuffer(viewId, frameBuffer);
	bgfx::setViewRect(viewId, 0, 0, width, height);
	bgfx::setViewTransform(viewId, &view, &projection);
	bgfx::touch(viewId);

	const float lightDirIntensity[4] = {m_lightDir.x, m_lightDir.y, m_lightDir.z, m_lightIntensity};
	const float lightPositionRange[4] = {
		m_lightPosition.x, m_lightPosition.y, m_lightPosition.z, m_lightRange
	};
	const float lightTypeSpot[4] = {m_lightType, m_lightSpotCosine, 0.0f, 0.0f};
	const float debugView[4] = {
		drawDebug && m_rendererDebugSettings.ShowNormals ? 1.0f : 0.0f,
		0.0f,
		0.0f,
		0.0f
	};
	bgfx::setUniform(m_debugViewUniform, debugView);
	const float cameraPositionValue[4] = {
		cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f
	};
	bgfx::setUniform(m_cameraPositionUniform, cameraPositionValue);
	enignE::Graphics::ShadowSamplingBindings shadowBindings;
	shadowBindings.Samplers = m_shadowSamplerUniforms;
	shadowBindings.Matrices = m_shadowMatrixUniforms;
	shadowBindings.Parameters = m_shadowParametersUniform;
	shadowBindings.CascadeSplits = m_cascadeSplitsUniform;
	shadowBindings.MapInfo = m_shadowMapInfoUniform;
	shadowBindings.Frame = &shadowFrame;
	shadowBindings.Strength = m_shadowStrength;
	shadowBindings.Bias = m_shadowBias;
	shadowBindings.NormalBias = m_shadowNormalBias;
	shadowBindings.Enabled = shadowFrame.IsValid();
	for (std::uint8_t index = 0; index < enignE::Graphics::ShadowFrameData::MaxCascades; ++index)
		shadowBindings.Textures[index] = m_shadowRenderer.GetTexture(index);
	m_instanceRenderer.Draw(
		viewId,
		m_sceneProgram,
		m_albedoUniform,
		m_albedoSampler,
		m_metallicRoughnessSampler,
		m_normalSampler,
		m_lightDirIntensityUniform,
		m_lightColorMaterialUniform,
		m_lightPositionRangeUniform,
		m_lightTypeSpotUniform,
		m_materialSurfaceUniform,
		m_materialEmissiveUniform,
		shadowBindings,
		m_defaultAlbedoTexture->GetHandle(),
		lightDirIntensity,
		lightPositionRange,
		lightTypeSpot,
		m_lightColor);
	if (drawDebug)
	{
		m_instanceRenderer.DrawDebug(viewId, ActiveScene().GetRegistry(), m_rendererDebugSettings);
		DrawPhysicsDebug(viewId);
	}
}

void Engine::DrawPhysicsDebug(bgfx::ViewId viewId)
{
	if (!m_rendererDebugSettings.ShowPhysicsColliders
		&& !m_rendererDebugSettings.ShowPhysicsContacts)
		return;

	DebugDrawEncoder debugDraw;
	debugDraw.begin(viewId, true);
	debugDraw.setWireframe(true);
	if (m_rendererDebugSettings.ShowPhysicsColliders)
	{
		debugDraw.setColor(0xffffff40);
		auto colliders = ActiveScene().View<
			enignE::Scene::TransformComponent,
			enignE::Scene::RigidBodyComponent,
			enignE::Scene::ColliderComponent>();
		for (const entt::entity entity : colliders)
		{
			const auto& rigidBody = colliders.get<enignE::Scene::RigidBodyComponent>(entity);
			if (!rigidBody.Enabled || ActiveScene().IsEntityPendingDestroy(entity))
				continue;
			const auto& collider = colliders.get<enignE::Scene::ColliderComponent>(entity);
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 rotation;
			if (!GetPhysicsWorldTransform(ActiveScene(), entity, position, rotation))
				continue;
			if (collider.Shape == enignE::Scene::ColliderShape::Sphere)
			{
				bx::Sphere sphere;
				sphere.center = {position.x, position.y, position.z};
				sphere.radius = collider.Radius;
				debugDraw.draw(sphere);
			}
			else
			{
				const DirectX::XMMATRIX transform = DirectX::XMMatrixScaling(
					collider.HalfExtents.x,
					collider.HalfExtents.y,
					collider.HalfExtents.z)
					* DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation))
					* DirectX::XMMatrixTranslation(position.x, position.y, position.z);
				DirectX::XMFLOAT4X4 stored;
				DirectX::XMStoreFloat4x4(&stored, transform);
				bx::Obb box;
				std::memcpy(box.mtx, &stored, sizeof(box.mtx));
				debugDraw.draw(box);
			}
		}
	}

	if (m_rendererDebugSettings.ShowPhysicsContacts && m_physicsWorld.IsInitialized())
	{
		debugDraw.setColor(0xffff40ff);
		debugDraw.setWireframe(false);
		for (const enignE::Physics::PhysicsContact& contact : m_physicsWorld.GetActiveContacts())
		{
			bx::Sphere point;
			point.center = {contact.Position.x, contact.Position.y, contact.Position.z};
			point.radius = 0.06f;
			debugDraw.draw(point);
			debugDraw.moveTo(contact.Position.x, contact.Position.y, contact.Position.z);
			debugDraw.lineTo(
				contact.Position.x + contact.Normal.x * 0.5f,
				contact.Position.y + contact.Normal.y * 0.5f,
				contact.Position.z + contact.Normal.z * 0.5f);
		}
	}
	debugDraw.end();
}

void Engine::ClearWorldView(
	bgfx::ViewId viewId,
	bgfx::FrameBufferHandle frameBuffer,
	std::uint16_t width,
	std::uint16_t height)
{
	if (width == 0 || height == 0)
		return;

	bgfx::setViewFrameBuffer(viewId, frameBuffer);
	bgfx::setViewRect(viewId, 0, 0, width, height);
	bgfx::touch(viewId);
}

void Engine::ResizeRenderTarget(
	bgfx::FrameBufferHandle& frameBuffer,
	std::uint16_t& currentWidth,
	std::uint16_t& currentHeight,
	std::uint16_t width,
	std::uint16_t height)
{
	width = std::max<std::uint16_t>(1, width);
	height = std::max<std::uint16_t>(1, height);
	if (bgfx::isValid(frameBuffer) && width == currentWidth && height == currentHeight)
		return;
	if (bgfx::isValid(frameBuffer))
		bgfx::destroy(frameBuffer);

	const bgfx::TextureHandle attachments[] = {
		bgfx::createTexture2D(
			width,
			height,
			false,
			1,
			bgfx::TextureFormat::BGRA8,
			BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP),
		bgfx::createTexture2D(
			width,
			height,
			false,
			1,
			bgfx::TextureFormat::D24S8,
			BGFX_TEXTURE_RT_WRITE_ONLY)
	};
	if (!bgfx::isValid(attachments[0]) || !bgfx::isValid(attachments[1]))
	{
		if (bgfx::isValid(attachments[0])) bgfx::destroy(attachments[0]);
		if (bgfx::isValid(attachments[1])) bgfx::destroy(attachments[1]);
		frameBuffer = BGFX_INVALID_HANDLE;
		LOG_ERRORF("Failed to create {}x{} viewport render target", width, height);
		return;
	}
	frameBuffer = bgfx::createFrameBuffer(static_cast<std::uint8_t>(std::size(attachments)), attachments, true);
	if (!bgfx::isValid(frameBuffer))
	{
		bgfx::destroy(attachments[0]);
		bgfx::destroy(attachments[1]);
		LOG_ERRORF("Failed to create {}x{} viewport framebuffer", width, height);
		return;
	}
	currentWidth = width;
	currentHeight = height;
}

void Engine::ResizeSceneViewport(std::uint16_t width, std::uint16_t height)
{
	m_requestedSceneViewportWidth = std::max<std::uint16_t>(1, width);
	m_requestedSceneViewportHeight = std::max<std::uint16_t>(1, height);
}

void Engine::ResizeGameViewport(std::uint16_t width, std::uint16_t height)
{
	m_requestedGameViewportWidth = std::max<std::uint16_t>(1, width);
	m_requestedGameViewportHeight = std::max<std::uint16_t>(1, height);
}

void Engine::ApplyViewportResizes()
{
	if (m_appMode != AppMode::Editor)
		return;

	const bool sceneSizeChanged =
		!bgfx::isValid(m_sceneViewportFrameBuffer)
		|| m_requestedSceneViewportWidth != m_sceneViewportWidth
		|| m_requestedSceneViewportHeight != m_sceneViewportHeight;
	ResizeRenderTarget(
		m_sceneViewportFrameBuffer,
		m_sceneViewportWidth,
		m_sceneViewportHeight,
		m_requestedSceneViewportWidth,
		m_requestedSceneViewportHeight);
	if (sceneSizeChanged && bgfx::isValid(m_sceneViewportFrameBuffer))
	{
		m_sceneCamera.SetAspect(
			static_cast<float>(m_sceneViewportWidth) / static_cast<float>(m_sceneViewportHeight));
	}

	ResizeRenderTarget(
		m_gameViewportFrameBuffer,
		m_gameViewportWidth,
		m_gameViewportHeight,
		m_requestedGameViewportWidth,
		m_requestedGameViewportHeight);
}

bgfx::TextureHandle Engine::GetSceneViewportTexture() const
{
	if (bgfx::isValid(m_sceneViewportFrameBuffer))
		return bgfx::getTexture(m_sceneViewportFrameBuffer, 0);
	return BGFX_INVALID_HANDLE;
}

bgfx::TextureHandle Engine::GetGameViewportTexture() const
{
	if (bgfx::isValid(m_gameViewportFrameBuffer))
		return bgfx::getTexture(m_gameViewportFrameBuffer, 0);
	return BGFX_INVALID_HANDLE;
}

void Engine::DestroyViewportTargets()
{
	if (bgfx::isValid(m_sceneViewportFrameBuffer))
		bgfx::destroy(m_sceneViewportFrameBuffer);
	if (bgfx::isValid(m_gameViewportFrameBuffer))
		bgfx::destroy(m_gameViewportFrameBuffer);
	m_sceneViewportFrameBuffer = BGFX_INVALID_HANDLE;
	m_gameViewportFrameBuffer = BGFX_INVALID_HANDLE;
}

int Engine::Run()
{
	MSG message = {};
	while (message.message != WM_QUIT)
	{
		if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		else
		{
			Tick();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	return static_cast<int>(message.wParam);
}

void Engine::Cleanup()
{
	InputManager::Get().CaptureMouse(false);
	m_physicsWorld.Shutdown();
	if (!m_bgfxInitialized)
		return;

	m_layerStack.Clear();
	#ifndef ENIGNE_RUNTIME_BUILD
	if (m_imguiInitialized)
	{
		imguiDestroy();
		m_imguiInitialized = false;
	}
	#endif

	// Release all engine-owned bgfx resources before bgfx validates shutdown.
	m_instanceRenderer.Clear();
	m_scene.Clear();
	m_playScene.Clear();
	DestroyViewportTargets();
	if (m_debugDrawInitialized)
	{
		ddShutdown();
		m_debugDrawInitialized = false;
	}
	DestroyRendererResources();
	bgfx::shutdown();
	enignE::Graphics::SetBgfxInitialized(false);
	m_bgfxInitialized = false;
	m_bgfxCallback.reset();
}

std::shared_ptr<Model> Engine::LoadModel(const std::string& filepath)
{
	return m_assets.LoadModel(filepath);
}

bool Engine::SaveScene(const std::filesystem::path& path)
{
	const std::filesystem::path resolvedPath = m_project.Resolve(path);
	auto sources = m_scene.View<enignE::Scene::RenderSourceComponent>();
	for (const entt::entity entity : sources)
	{
		auto& source = sources.get<enignE::Scene::RenderSourceComponent>(entity);
		if (source.Type == enignE::Scene::RenderSourceType::ImportedModel
			&& !source.AssetPath.empty())
		{
			const auto reference = m_assets.Register(
				source.AssetPath,
				enignE::Graphics::AssetType::Model);
			source.AssetHandle = reference.Handle;
			source.AssetPath = reference.Path;
		}
	}
	auto renderers = m_scene.View<enignE::Scene::MeshRendererComponent>();
	for (const entt::entity entity : renderers)
	{
		auto& renderer = renderers.get<enignE::Scene::MeshRendererComponent>(entity);
		if (!renderer.MaterialResourcePtr)
			continue;
		MaterialResource& material = *renderer.MaterialResourcePtr;
		const auto registerTexture = [this](
			std::uint64_t& handle,
			std::string& assetPath)
		{
			if (assetPath.empty())
				return;
			const auto reference = m_assets.Register(
				assetPath,
				enignE::Graphics::AssetType::Texture);
			handle = reference.Handle;
			assetPath = reference.Path;
		};
		registerTexture(material.AlbedoTextureHandle, material.AlbedoTexturePath);
		registerTexture(material.NormalTextureHandle, material.NormalTexturePath);
		registerTexture(
			material.MetallicRoughnessTextureHandle,
			material.MetallicRoughnessTexturePath);
	}
	auto save = m_jobs.Submit([this, resolvedPath]
	{
		return enignE::Scene::SceneSerializer::Save(
			m_scene,
			resolvedPath,
			[this](const std::string& assetPath)
			{
				return m_assets.MakeProjectRelative(assetPath);
			});
	});
	if (!save.get())
		return false;
	m_currentScenePath = resolvedPath;
	m_assets.SaveManifest(m_project.Resolve(m_project.AssetDirectory / "asset-manifest.json"));
	return true;
}

bool Engine::LoadScene(const std::filesystem::path& path)
{
	const std::filesystem::path resolvedPath = m_project.Resolve(path);
	auto load = m_jobs.Submit([resolvedPath]
	{
		auto scene = std::make_unique<enignE::Scene::Scene>();
		if (!enignE::Scene::SceneSerializer::Load(*scene, resolvedPath))
			return std::unique_ptr<enignE::Scene::Scene>{};
		return scene;
	});
	std::unique_ptr<enignE::Scene::Scene> loaded = load.get();
	if (!loaded)
		return false;
	ResolveSceneResources(*loaded);
	m_scene = std::move(*loaded);
	if (m_appMode == AppMode::Runtime && !RebuildPhysicsScene(m_scene))
	{
		LOG_ERRORF("Unable to build runtime physics bodies for '{}'", resolvedPath.string());
		return false;
	}
	m_currentScenePath = resolvedPath;
	m_assets.SaveManifest(m_project.Resolve(m_project.AssetDirectory / "asset-manifest.json"));
	return true;
}

void Engine::ResolveSceneResources(enignE::Scene::Scene& scene)
	{
	struct PrimitiveCacheEntry
	{
		enignE::Scene::PrimitiveDesc Desc;
		std::shared_ptr<Model> ModelPtr;
	};
	std::vector<PrimitiveCacheEntry> primitiveModels;
	auto renderables = scene.View<
		enignE::Scene::RenderSourceComponent,
		enignE::Scene::MeshRendererComponent>();
	for (const entt::entity entity : renderables)
	{
		auto& source = renderables.get<enignE::Scene::RenderSourceComponent>(entity);
		auto& renderer = renderables.get<enignE::Scene::MeshRendererComponent>(entity);
		if (source.Type == enignE::Scene::RenderSourceType::Primitive)
		{
			enignE::Scene::PrimitiveDesc desc;
			desc.Type = source.Primitive;
			desc.Size = source.Size;
			desc.Width = source.Width;
			desc.Height = source.Height;
			desc.Depth = source.Depth;
			desc.Radius = source.Radius;
			desc.Slices = source.Slices;
			desc.Stacks = source.Stacks;
			const auto matches = [&desc](const PrimitiveCacheEntry& entry)
			{
				const auto& value = entry.Desc;
				return value.Type == desc.Type && value.Size == desc.Size
					&& value.Width == desc.Width && value.Height == desc.Height
					&& value.Depth == desc.Depth && value.Radius == desc.Radius
					&& value.Slices == desc.Slices && value.Stacks == desc.Stacks;
			};
			const auto cached = std::find_if(primitiveModels.begin(), primitiveModels.end(), matches);
			if (cached != primitiveModels.end())
				renderer.ModelPtr = cached->ModelPtr;
			else
			{
				MeshData data;
				switch (desc.Type)
				{
				case enignE::Scene::PrimitiveType::Cube: data = ProcGen::CreateCube(desc.Size); break;
				case enignE::Scene::PrimitiveType::Rectangle: data = ProcGen::CreateRectangle(desc.Width, desc.Height, desc.Depth); break;
				case enignE::Scene::PrimitiveType::Pyramid: data = ProcGen::CreatePyramid(desc.Size); break;
				case enignE::Scene::PrimitiveType::Plane: data = ProcGen::CreatePlane(desc.Width, desc.Depth); break;
				case enignE::Scene::PrimitiveType::Sphere: data = ProcGen::CreateSphere(desc.Radius, desc.Slices, desc.Stacks); break;
				}
				renderer.ModelPtr = CreateModel(data);
				primitiveModels.push_back({desc, renderer.ModelPtr});
			}
		}
		else if (source.Type == enignE::Scene::RenderSourceType::ImportedModel
			&& (!source.AssetPath.empty() || source.AssetHandle != 0))
		{
			const auto reference = m_assets.RegisterReference({
				source.AssetHandle,
				enignE::Graphics::AssetType::Model,
				source.AssetPath});
			source.AssetHandle = reference.Handle;
			source.AssetPath = reference.Path;
			renderer.ModelPtr = m_assets.LoadModel(reference.Handle);
		}
		if (!renderer.MaterialResourcePtr) continue;
		MaterialResource& material = *renderer.MaterialResourcePtr;
		const auto resolveTexture = [this](
			std::uint64_t& handle,
			std::string& assetPath,
			std::shared_ptr<Texture2D>& texture)
		{
			if (assetPath.empty() && handle == 0) return;
			const auto reference = m_assets.RegisterReference({
				handle,
				enignE::Graphics::AssetType::Texture,
				assetPath});
			handle = reference.Handle;
			assetPath = reference.Path;
			texture = m_assets.LoadTexture(reference.Handle);
		};
		resolveTexture(material.AlbedoTextureHandle, material.AlbedoTexturePath, material.AlbedoTexture);
		resolveTexture(material.NormalTextureHandle, material.NormalTexturePath, material.NormalTexture);
		resolveTexture(material.MetallicRoughnessTextureHandle,
			material.MetallicRoughnessTexturePath, material.MetallicRoughnessTexture);
	}
}

bool Engine::RebuildPhysicsScene(enignE::Scene::Scene& scene)
{
	m_physicsWorld.Shutdown();
	m_physicsSceneStructureVersion = 0;
	if (!m_physicsWorld.Initialize())
		return false;
	auto configured = scene.View<enignE::Scene::IDComponent, enignE::Scene::RigidBodyComponent>();
	for (const entt::entity entity : configured)
	{
		const auto& body = configured.get<enignE::Scene::RigidBodyComponent>(entity);
		if (!body.Enabled || scene.IsEntityPendingDestroy(entity)) continue;
		if (!scene.HasComponent<enignE::Scene::TransformComponent>(entity)
			|| !scene.HasComponent<enignE::Scene::ColliderComponent>(entity))
		{
			LOG_ERRORF("Physics entity {} requires both Transform and Collider components",
				scene.GetEntityID(entity));
			m_physicsWorld.Shutdown();
			return false;
		}
	}

	std::vector<entt::entity> bodies;
	auto view = scene.View<
		enignE::Scene::IDComponent,
		enignE::Scene::TransformComponent,
		enignE::Scene::RigidBodyComponent,
		enignE::Scene::ColliderComponent>();
	for (const entt::entity entity : view)
	{
		if (view.get<enignE::Scene::RigidBodyComponent>(entity).Enabled
			&& !scene.IsEntityPendingDestroy(entity))
			bodies.push_back(entity);
	}
	std::sort(bodies.begin(), bodies.end(), [&scene](entt::entity lhs, entt::entity rhs)
	{
		return scene.GetEntityID(lhs) < scene.GetEntityID(rhs);
	});

	for (const entt::entity entity : bodies)
	{
		const auto& rigidBody = view.get<enignE::Scene::RigidBodyComponent>(entity);
		const auto& collider = view.get<enignE::Scene::ColliderComponent>(entity);
		const std::uint64_t id = scene.GetEntityID(entity);
		if (rigidBody.Motion == enignE::Scene::RigidBodyMotion::Dynamic
			&& scene.GetParent(entity) != entt::null)
		{
			LOG_ERRORF("Physics entity {} is dynamic but is not a hierarchy root", id);
			m_physicsWorld.Shutdown();
			return false;
		}

		enignE::Physics::PhysicsBodyConfig config;
		config.EntityID = id;
		if (!GetPhysicsWorldTransform(scene, entity, config.Position, config.Rotation))
		{
			LOG_ERRORF("Unable to resolve physics transform for entity {}", id);
			m_physicsWorld.Shutdown();
			return false;
		}
		switch (rigidBody.Motion)
		{
		case enignE::Scene::RigidBodyMotion::Static: config.Motion = enignE::Physics::MotionType::Static; break;
		case enignE::Scene::RigidBodyMotion::Dynamic: config.Motion = enignE::Physics::MotionType::Dynamic; break;
		case enignE::Scene::RigidBodyMotion::Kinematic: config.Motion = enignE::Physics::MotionType::Kinematic; break;
		}
		config.Shape = collider.Shape == enignE::Scene::ColliderShape::Sphere
			? enignE::Physics::ShapeType::Sphere : enignE::Physics::ShapeType::Box;
		config.HalfExtents = collider.HalfExtents;
		config.Radius = collider.Radius;
		config.Friction = rigidBody.Friction;
		config.Restitution = rigidBody.Restitution;
		config.LinearDamping = rigidBody.LinearDamping;
		config.AngularDamping = rigidBody.AngularDamping;
		config.GravityFactor = rigidBody.GravityFactor;
		if (!m_physicsWorld.AddBody(config))
		{
			LOG_ERRORF("Unable to create physics body for entity {}", id);
			m_physicsWorld.Shutdown();
			return false;
		}
	}
	m_physicsSceneStructureVersion = scene.GetStructureVersion();
	LOG_INFOF("Built physics world with {} bodies", bodies.size());
	return true;
}

bool Engine::StepPhysicsScene(enignE::Scene::Scene& scene, float fixedDeltaTime)
{
	if (scene.GetStructureVersion() != m_physicsSceneStructureVersion
		&& !RebuildPhysicsScene(scene))
		return false;

	auto view = scene.View<
		enignE::Scene::IDComponent,
		enignE::Scene::TransformComponent,
		enignE::Scene::RigidBodyComponent,
		enignE::Scene::ColliderComponent>();
	for (const entt::entity entity : view)
	{
		const auto& body = view.get<enignE::Scene::RigidBodyComponent>(entity);
		if (!body.Enabled || body.Motion == enignE::Scene::RigidBodyMotion::Dynamic
			|| scene.IsEntityPendingDestroy(entity))
			continue;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 rotation;
		if (!GetPhysicsWorldTransform(scene, entity, position, rotation))
			return false;
		if (!m_physicsWorld.SetBodyTransform(
				scene.GetEntityID(entity), position, rotation, fixedDeltaTime))
			return false;
	}

	if (!m_physicsWorld.Step(fixedDeltaTime))
		return false;

	for (const entt::entity entity : view)
	{
		const auto& body = view.get<enignE::Scene::RigidBodyComponent>(entity);
		if (!body.Enabled || body.Motion != enignE::Scene::RigidBodyMotion::Dynamic
			|| scene.IsEntityPendingDestroy(entity))
			continue;
		enignE::Physics::PhysicsBodyState state;
		if (!m_physicsWorld.GetBodyState(scene.GetEntityID(entity), state))
			return false;
		const auto& transform = view.get<enignE::Scene::TransformComponent>(entity);
		scene.SetTransformQuaternion(
			entity, state.Position, state.Rotation,
			transform.GetLocalRotation(), transform.GetLocalScale());
	}
	return true;
}

std::shared_ptr<Texture2D> Engine::LoadTexture(const std::string& filepath)
{
	return m_assets.LoadTexture(filepath);
}

bool Engine::BeginPlay()
{
	if (m_appMode != AppMode::Editor || m_simulationState != SimulationState::Stopped)
		return false;
	m_scene.FlushDestroyQueue();
	auto clone = m_jobs.Submit([this]
	{
		auto scene = std::make_unique<enignE::Scene::Scene>("PlayScene");
		scene->CopyFrom(m_scene);
		return scene;
	});
	m_playScene = std::move(*clone.get());
	if (!RebuildPhysicsScene(m_playScene))
	{
		LOG_ERROR("Unable to initialize the Play Mode physics world");
		m_playScene.Clear();
		return false;
	}
	m_activeScene = &m_playScene;
	m_simulationClock.Reset();
	m_simulationState = SimulationState::Playing;
	m_instanceRenderer.Clear();
	return true;
}

bool Engine::StopPlay()
{
	if (m_appMode != AppMode::Editor || m_simulationState == SimulationState::Stopped)
		return false;
	SetGameInputCaptured(false);
	m_physicsWorld.Shutdown();
	m_physicsSceneStructureVersion = 0;
	m_activeScene = &m_scene;
	m_playScene.Clear();
	m_simulationClock.Reset();
	m_simulationState = SimulationState::Stopped;
	m_instanceRenderer.Clear();
	return true;
}

bool Engine::TogglePause()
{
	if (m_appMode != AppMode::Editor || m_simulationState == SimulationState::Stopped)
		return false;
	m_simulationState = m_simulationState == SimulationState::Playing
		? SimulationState::Paused : SimulationState::Playing;
	return true;
}

bool Engine::StepSimulation()
{
	if (m_appMode != AppMode::Editor || m_simulationState != SimulationState::Paused)
		return false;
	m_simulationClock.RequestStep();
	return true;
}

void Engine::SetGameInputCaptured(bool captured)
{
	const bool allowed = captured
		&& m_appMode == AppMode::Editor
		&& m_simulationState != SimulationState::Stopped
		&& InputManager::Get().HasFocus();
	if (m_gameInputCaptured == allowed) return;
	m_gameInputCaptured = allowed;
	InputManager::Get().CaptureMouse(allowed);
	if (allowed) m_sceneCamera.ResetInteraction();
}

bool Engine::SaveSceneWithDialog()
{
	std::filesystem::path path = ChooseScenePath(
		m_hWnd,
		true,
		m_currentScenePath.empty()
			? m_project.Resolve(std::filesystem::path("scenes/scene.escene"))
			: m_currentScenePath);
	return !path.empty() && SaveScene(path);
}

bool Engine::LoadSceneWithDialog()
{
	const std::filesystem::path path = ChooseScenePath(m_hWnd, false, m_currentScenePath);
	return !path.empty() && LoadScene(path);
}

void Engine::SetView(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& up)
{
	m_sceneCamera.SetView(eye, at, up);
}

void Engine::SetProjection(float fovY, float aspect, float zn, float zf)
{
	m_sceneCamera.SetProjection(fovY, aspect, zn, zf);
}

void Engine::EnableSceneCameraControls(bool enable)
{
	m_sceneCameraControlsEnabled = enable;
	if (!enable)
	{
		m_sceneCamera.ResetInteraction();
		InputManager::Get().CaptureMouse(false);
	}
}

void Engine::RegisterSceneCameraInput()
{
	EventManager::Get().AddListener(EventType::MouseMove, [this](Event& event)
	{
		if (auto* mouseMove = dynamic_cast<MouseMoveEvent*>(&event); mouseMove && IsGameInputCaptured())
		{
			m_gameMouseDeltaX += static_cast<float>(mouseMove->X);
			m_gameMouseDeltaY += static_cast<float>(mouseMove->Y);
		}
		else if (m_sceneCameraControlsEnabled && !m_gameInputCaptured)
		{
			if (mouseMove)
			{
				m_sceneCamera.AddMouseDelta(
					static_cast<float>(mouseMove->X),
					static_cast<float>(mouseMove->Y));
			}
		}
	});
	EventManager::Get().AddListener(EventType::MouseButtonDown, [this](Event& event)
	{
		if (!m_sceneCameraControlsEnabled || m_gameInputCaptured)
			return;
		if (auto* mouseButton = dynamic_cast<MouseButtonEvent*>(&event))
		{
			if (mouseButton->Button == 2)
			{
				m_sceneCamera.BeginLook();
				InputManager::Get().CaptureMouse(true);
			}
			else if (mouseButton->Button == 3)
				m_sceneCamera.BeginPan();
		}
	});
	EventManager::Get().AddListener(EventType::MouseButtonUp, [this](Event& event)
	{
		if (!m_sceneCameraControlsEnabled || m_gameInputCaptured)
			return;
		if (auto* mouseButton = dynamic_cast<MouseButtonEvent*>(&event))
		{
			if (mouseButton->Button == 2)
			{
				m_sceneCamera.EndLook();
				InputManager::Get().CaptureMouse(false);
			}
			else if (mouseButton->Button == 3)
				m_sceneCamera.EndPan();
		}
	});
	EventManager::Get().AddListener(EventType::MouseWheel, [this](Event& event)
	{
		if (!m_sceneCameraControlsEnabled || m_gameInputCaptured)
			return;
		if (auto* mouseWheel = dynamic_cast<MouseWheelEvent*>(&event); mouseWheel && !mouseWheel->Horizontal)
			m_sceneCamera.AddWheelDelta(static_cast<float>(mouseWheel->Delta) / WHEEL_DELTA);
	});
}

void Engine::UpdateSceneCameraControls(float deltaTime)
{
	if (!m_sceneCameraControlsEnabled || m_gameInputCaptured || !InputManager::Get().HasFocus())
	{
		m_sceneCamera.ResetInteraction();
		return;
	}
	#ifndef ENIGNE_RUNTIME_BUILD
	if (m_imguiInitialized
		&& ImGui::GetCurrentContext()
		&& ImGui::GetIO().WantCaptureMouse
		&& !m_sceneViewportHovered)
	{
		InputManager::Get().CaptureMouse(false);
		m_sceneCamera.ResetInteraction();
		return;
	}
	#endif

	m_sceneCamera.Update(deltaTime, InputManager::Get());
}

std::shared_ptr<Model> Engine::CreateModel(const MeshData& data)
{
	return ProcGen::BuildModel(data);
}
