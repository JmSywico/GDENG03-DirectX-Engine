#pragma once

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Core.h>
#include <DX3D/Editor/TransformGizmo.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/ColliderComponent.h>

#include <string>
#include <chrono>
#include <vector>
#include <deque>
#include <array>

namespace dx3d
{
	class GameObject;
	class CameraComponent;
	class PhysicsWorld;

	class Game
	{
		dx3d_disable_copy_and_move(Game)

	public:
		explicit Game(const GameDesc& desc);
		virtual ~Game();

		virtual World& getWorld() noexcept final;
		virtual Logger& getLogger() noexcept final;
		virtual InputSystem& getInputSystem() noexcept final;
		virtual void run() final;

	protected:
		virtual void onCreate() {}
		virtual void onUpdate(f32 deltaTime) {}

		void requestExit() noexcept;
		bool isSceneViewportHovered() const noexcept { return m_sceneViewportHovered; }
		bool isSceneViewportFocused() const noexcept { return m_sceneViewportFocused; }
		GameObject* getEditorCamera() noexcept { return m_editorCamera; }
		GameObject* getPrimarySelectedObject() noexcept { return m_selectedObject; }

	private:
		enum class EditorMode
		{
			Editing,
			Playing,
			Paused
		};

		enum class CopiedObjectType
		{
			None,
			Cube,
			Plane,
			CombinedMesh
		};

		struct ObjectCopyData
		{
			bool isValid{ false };

			CopiedObjectType type{
				CopiedObjectType::None
			};

			std::string sourceName{};

			Vec3 position{};
			Vec3 rotation{};

			Vec3 scale{
				1.0f,
				1.0f,
				1.0f
			};

			MeshData meshData{};
			ui64 parentEntityId{};
			bool hasMaterial{};
			MaterialMode materialMode{ MaterialMode::LitTint };
			Vec4 materialAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
			Vec3 materialEmissive{};
			f32 materialEmissionStrength{};
			bool hasRigidBody{};
			RigidBodyType rigidBodyType{ RigidBodyType::Static };
			f32 rigidBodyFriction{ 0.5f };
			f32 rigidBodyRestitution{};
			f32 rigidBodyLinearDamping{ 0.05f };
			f32 rigidBodyAngularDamping{ 0.05f };
			f32 rigidBodyGravityFactor{ 1.0f };
			bool rigidBodyEnabled{ true };
			bool hasCollider{};
			ColliderShape colliderShape{ ColliderShape::Box };
			Vec3 colliderHalfExtents{ 0.5f, 0.5f, 0.5f };
			f32 colliderRadius{ 0.5f };

			ui32 pasteCount{};
		};

	private:
		void onInternalUpdate();

		bool isObjectSelected(
			const GameObject* object
		) const noexcept;

		void selectOnly(
			GameObject* object
		);

		void toggleObjectSelection(
			GameObject* object
		);

		void removeObjectFromSelection(
			GameObject* object
		);

		void clearSelection() noexcept;

		void handleViewportPicking(
			CameraComponent* camera,
			const TransformGizmo::ViewportArea& viewportArea
		);

		const MeshData* getObjectMeshData(
			GameObject* object
		) const noexcept;

		bool canMergeSelectedObjects() const noexcept;

		void mergeSelectedObjects();

		bool canCopySelectedObject() const noexcept;

		void copySelectedObject();

		void pasteCopiedObject();
		void duplicateSelectedObject();

		void createNewScene();
		void ensureEditorCamera();
		void ensureGameCamera();

		void saveScene();

		void loadScene();
		void openSceneDialog();
		void loadScene(const std::string& filePath);

		void pushUndoSnapshot();
		void pushUndoSnapshot(const std::string& snapshot);
		void undo();
		void redo();
		void startPlayMode();
		void stopPlayMode();
		void refreshAssetLens();

	private:
		UniquePtr<Logger> m_logger{};
		UniquePtr<InputSystem> m_inputSystem{};
		RefPtr<GraphicsDevice> m_graphicsDevice{};
		UniquePtr<Display> m_display{};
		UniquePtr<World> m_world{};

		UniquePtr<WorldRenderer> m_worldRenderer{};
		UniquePtr<PhysicsWorld> m_physicsWorld{};
		GameObject* m_editorCamera{};

		GameObject* m_selectedObject{};
		std::vector<GameObject*> m_selectedObjects{};

		ObjectCopyData m_objectClipboard{};

		TransformGizmo m_transformGizmo{};

		ui32 m_cubeCounter{ 0 };
		ui32 m_planeCounter{ 0 };

		std::string m_sceneStatusMessage
		{
			"Scene file: Scene.dx3dscene"
		};
		std::string m_sceneFilePath{ "Scene.dx3dscene" };
		std::string m_startupScenePath{};

		EditorMode m_editorMode{ EditorMode::Editing };
		std::string m_editorSceneSnapshot{};
		std::deque<std::string> m_undoSnapshots{};
		std::deque<std::string> m_redoSnapshots{};
		std::vector<std::string> m_assetPaths{};
		std::array<f32, 120> m_frameTimes{};
		size_t m_frameTimeCursor{};
		f32 m_fixedStepAccumulator{};
		bool m_singleStepRequested{};
		bool m_sceneDirty{};
		bool m_requestSceneLoad{};
		bool m_requestEditorClose{};
		bool m_showStats{ true };
		bool m_showAssetLens{ true };
		bool m_defaultDockLayoutBuilt{};
		bool m_focusSceneViewRequested{ true };
		bool m_focusGameViewRequested{};
		bool m_sceneViewportHovered{};
		bool m_sceneViewportFocused{};
		f32 m_uiScale{ 1.0f };
		char m_assetFilter[128]{};
		char m_registryFilter[128]{};

		bool m_isRunning{ true };

		std::chrono::steady_clock::time_point m_previousTime{};
	};
}
