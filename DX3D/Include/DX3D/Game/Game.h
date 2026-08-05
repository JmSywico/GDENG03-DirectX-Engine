#pragma once

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec2.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Core.h>
#include <DX3D/Editor/TransformGizmo.h>

#include <string>
#include <chrono>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstddef>

#include <d3d11.h>
#include <wrl.h>

namespace dx3d
{
	class GameObject;
	class CameraComponent;

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

	private:
		enum class CopiedObjectType
		{
			None,
			Cube,
			Plane,
			Sphere,
			Capsule,
			CombinedMesh
		};

		enum class SnapshotObjectType
		{
			Cube,
			Plane,
			Sphere,
			Capsule,
			CombinedMesh,
			Model,
			DirectionalLight
		};

		struct RigidBodySnapshot
		{
			bool isPresent{ false };
			Vec3 velocity{};
			Vec3 angularVelocity{};
			f32 mass{ 1.0f };
			f32 restitution{ 0.45f };
			f32 friction{ 0.20f };
			bool useGravity{ true };
			bool isStatic{ false };
			Vec3 colliderSize{
				1.0f,
				1.0f,
				1.0f
			};
			Vec3 colliderOffset{};
			bool colliderUsesTransformScale{ true };
		};

		struct MaterialSnapshot
		{
			bool isPresent{ false };
			std::string texturePath{};
			Vec2 uvTiling{
				1.0f,
				1.0f
			};
			Vec2 uvOffset{};
		};

		struct ObjectSnapshot
		{
			SnapshotObjectType type{
				SnapshotObjectType::Cube
			};

			std::string name{};

			Vec3 position{};
			Vec3 rotation{};
			Vec3 scale{
				1.0f,
				1.0f,
				1.0f
			};

			MeshData meshData{};
			std::string modelPath{};
			std::string texturePath{};

			Vec3 lightColor{
				1.0f,
				1.0f,
				1.0f
			};
			f32 lightIntensity{ 1.0f };
			f32 ambientStrength{ 0.20f };
			f32 shadowArea{ 30.0f };
			bool castShadows{ true };

			RigidBodySnapshot rigidBody{};
			MaterialSnapshot material{};
		};

		struct EditorSnapshot
		{
			std::vector<ObjectSnapshot> objects{};
			std::vector<std::string> selectedNames{};
			ui32 cubeCounter{};
			ui32 planeCounter{};
			ui32 sphereCounter{};
			ui32 capsuleCounter{};
			std::string sceneStatusMessage{};
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

			bool hasRigidBody{ false };
			Vec3 rigidBodyVelocity{};
			Vec3 rigidBodyAngularVelocity{};
			f32 rigidBodyMass{ 1.0f };
			f32 rigidBodyRestitution{ 0.45f };
			f32 rigidBodyFriction{ 0.20f };
			bool rigidBodyUseGravity{ true };
			bool rigidBodyIsStatic{ false };
			Vec3 rigidBodyColliderSize{
				1.0f,
				1.0f,
				1.0f
			};
			Vec3 rigidBodyColliderOffset{};
			bool rigidBodyColliderUsesTransformScale{ true };

			MaterialSnapshot material{};

			ui32 pasteCount{};
		};

		enum class ProjectAssetKind
		{
			Folder,
			Image,
			Model,
			Shader,
			Scene,
			Level,
			Prefab,
			Script,
			Other
		};

		struct ProjectAssetEntry
		{
			std::string name{};
			std::string path{};
			ProjectAssetKind kind{
				ProjectAssetKind::Other
			};
			bool isDirectory{ false };
			size_t fileSize{};
		};

		struct ProjectThumbnail
		{
			Microsoft::WRL::ComPtr<
				ID3D11ShaderResourceView
			> resourceView{};

			f32 width{};
			f32 height{};
			bool failed{ false };
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

		bool canObjectAcceptTexture(
			GameObject* object
		) const noexcept;

		void assignTextureToObject(
			GameObject* object,
			const std::string& texturePath
		);

		void drawPhysicsDebugOverlay(
			CameraComponent* camera,
			const TransformGizmo::ViewportArea& viewportArea
		);

		void drawProjectWindow(
			f32 workX,
			f32 workY,
			f32 workWidth,
			f32 workHeight,
			f32 rightPanelWidth
		);

		void refreshProjectAssets();

		ProjectAssetKind getProjectAssetKind(
			const std::string& path,
			bool isDirectory
		) const;

		const char* getProjectAssetIcon(
			ProjectAssetKind kind
		) const noexcept;

		ProjectThumbnail* getProjectThumbnail(
			const std::string& path
		);

		bool loadProjectThumbnail(
			const std::string& path,
			ProjectThumbnail& thumbnail
		);

		EditorSnapshot captureEditorSnapshot() const;

		void restoreEditorSnapshot(
			const EditorSnapshot& snapshot
		);

		bool areEditorSnapshotsEqual(
			const EditorSnapshot& lhs,
			const EditorSnapshot& rhs
		) const noexcept;

		void pushUndoSnapshot(
			const EditorSnapshot& snapshot
		);

		void beginPendingUndoSnapshot(
			const EditorSnapshot& snapshot
		);

		void commitPendingUndoSnapshot(
			const EditorSnapshot& currentSnapshot,
			bool editorStillActive
		);

		bool canUndo() const noexcept;
		bool canRedo() const noexcept;

		void undoEditorOperation();
		void redoEditorOperation();

		bool loadCreditsLogo();
		void createNewScene();

		void saveScene();

		void loadScene();

		void saveLevel();

		void loadLevel();

	private:
		UniquePtr<Logger> m_logger{};
		UniquePtr<InputSystem> m_inputSystem{};
		RefPtr<GraphicsDevice> m_graphicsDevice{};
		UniquePtr<Display> m_display{};
		UniquePtr<World> m_world{};

		UniquePtr<WorldRenderer> m_worldRenderer{};

		GameObject* m_selectedObject{};
		std::vector<GameObject*> m_selectedObjects{};

		ObjectCopyData m_objectClipboard{};

		std::vector<EditorSnapshot> m_undoStack{};
		std::vector<EditorSnapshot> m_redoStack{};

		EditorSnapshot m_pendingUndoSnapshot{};
		bool m_hasPendingUndoSnapshot{ false };
		bool m_isRestoringEditorSnapshot{ false };
		bool m_skipUndoTrackingThisFrame{ false };

		TransformGizmo m_transformGizmo{};

		bool m_showCreditsWindow = false;

		bool m_showColorPickerWindow = false;

		bool m_showPhysicsDebugOverlay = false;
		bool m_showOnlySelectedPhysicsDebug = true;
		bool m_showProjectWindow = true;

		Microsoft::WRL::ComPtr<
			ID3D11ShaderResourceView
		> m_creditsLogo{};

		f32 m_creditsLogoWidth{};
		f32 m_creditsLogoHeight{};

		std::vector<ProjectAssetEntry>
			m_projectAssets{};

		std::unordered_map<
			std::string,
			ProjectThumbnail
		> m_projectThumbnailCache{};

		std::string m_projectRootPath{
			"DX3D/Assets"
		};

		std::string m_projectCurrentPath{
			"DX3D/Assets"
		};

		std::string m_selectedProjectAssetPath{};

		std::array<char, 128>
			m_projectSearchBuffer{};

		bool m_projectAssetsDirty{ true };

		ui32 m_cubeCounter{ 0 };
		ui32 m_planeCounter{ 0 };
		ui32 m_sphereCounter{ 0 };
		ui32 m_capsuleCounter{ 0 };

		std::string m_sceneStatusMessage
		{
			"Scene file: Scene.dx3dscene"
		};

		bool m_isRunning{ true };

		std::chrono::steady_clock::time_point m_previousTime{};
	};
}
