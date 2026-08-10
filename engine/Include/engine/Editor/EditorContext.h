#pragma once

#include <DirectXMath.h>
#include <entt/entity/entity.hpp>
#include <functional>
#include <filesystem>
#include <memory>
#include <vector>

class Model;
class Texture2D;
namespace jnpf::Graphics { class AssetRegistry; }
struct MeshData;

namespace jnpf::Scene
{
	class Scene;
}

namespace jnpf::Editor
{
	class CommandStack;
	enum class GizmoMode
	{
		Translate,
		Rotate,
		Scale
	};
	enum class SimulationState { Stopped, Playing, Paused };

	struct EditorContext
	{
		using MatrixProvider = std::function<const DirectX::XMFLOAT4X4&()>;
		using ViewportProvider = std::function<DirectX::XMUINT2()>;
		using SceneChangedCallback = std::function<void()>;
		using WindowAction = std::function<void()>;
		using SceneAction = std::function<bool()>;
		using WindowStateProvider = std::function<bool()>;
		using PrimitiveModelFactory = std::function<std::shared_ptr<Model>(const MeshData&)>;
		using TextureLoader = std::function<std::shared_ptr<Texture2D>(const std::string&)>;
		using ModelLoader = std::function<std::shared_ptr<Model>(const std::string&)>;
		using SimulationStateProvider = std::function<SimulationState()>;
		using SimulationAction = std::function<bool()>;
		using GameInputCaptureAction = std::function<void(bool)>;
		using PrefabAction = std::function<bool(
			std::uint64_t,
			const std::string&,
			std::uint64_t,
			const DirectX::XMFLOAT3*)>;
		using EntityAction = std::function<bool(entt::entity)>;
		using AssetAction = std::function<bool(std::uint64_t, const std::string&)>;
		using AssetReferenceCount = std::function<std::size_t(std::uint64_t)>;

		EditorContext(
			Scene::Scene& scene,
			MatrixProvider viewProvider,
			MatrixProvider projectionProvider,
			ViewportProvider viewportProvider,
			SceneChangedCallback sceneChangedCallback,
			WindowAction minimizeWindow,
			WindowAction toggleMaximizeWindow,
			WindowAction closeWindow,
			WindowAction beginTitleBarDrag,
			WindowStateProvider isWindowMaximized);

		bool IsSelectedEntityValid() const;
		bool IsEntitySelected(entt::entity entity) const;
		std::vector<entt::entity> GetSelectedEntities() const;
		void SelectEntity(entt::entity entity, bool additive = false);
		void ToggleEntitySelection(entt::entity entity);
		void SelectEntities(const std::vector<entt::entity>& entities);
		void ClearSelection();
		void ValidateSelection();

		Scene::Scene* ActiveScene = nullptr;
		CommandStack* Commands = nullptr;
		MatrixProvider ViewProvider;
		MatrixProvider ProjectionProvider;
		ViewportProvider ViewportProviderCallback;
		SceneChangedCallback SceneChanged;
		WindowAction MinimizeWindow;
		WindowAction ToggleMaximizeWindow;
		WindowAction CloseWindow;
		WindowAction BeginTitleBarDrag;
		WindowAction CopySelection;
		WindowAction PasteSelection;
		SceneAction SaveScene;
		SceneAction LoadScene;
		WindowStateProvider IsWindowMaximized;
		PrimitiveModelFactory CreatePrimitiveModel;
		TextureLoader LoadTexture;
		ModelLoader LoadModel;
		SimulationStateProvider GetSimulationState;
		SimulationAction BeginPlay;
		SimulationAction StopPlay;
		SimulationAction TogglePause;
		SimulationAction StepSimulation;
		GameInputCaptureAction SetGameInputCaptured;
		WindowStateProvider IsGameInputCaptured;
		PrefabAction InstantiatePrefab;
		PrefabAction InstantiateModel;
		EntityAction SaveAsPrefab;
		AssetAction DeleteAsset;
		AssetReferenceCount CountPrefabReferences;
		Graphics::AssetRegistry* Assets = nullptr;
		std::filesystem::path AssetDirectory = "assets";
		entt::entity SelectedEntity = entt::null;
		std::vector<entt::entity> SelectedEntities;
		GizmoMode CurrentGizmoMode = GizmoMode::Translate;
		bool RequestSceneLoad = false;
		bool RequestCloseEditor = false;
	};
}
