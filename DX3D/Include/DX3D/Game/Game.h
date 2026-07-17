#pragma once

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Core.h>
#include <DX3D/Editor/TransformGizmo.h>

#include <string>
#include <chrono>
#include <vector>

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

		bool loadCreditsLogo();
		void createNewScene();

		void saveScene();

		void loadScene();

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

		TransformGizmo m_transformGizmo{};

		bool m_showCreditsWindow = false;

		bool m_showColorPickerWindow = false;

		Microsoft::WRL::ComPtr<
			ID3D11ShaderResourceView
		> m_creditsLogo{};

		f32 m_creditsLogoWidth{};
		f32 m_creditsLogoHeight{};

		ui32 m_cubeCounter{ 0 };
		ui32 m_planeCounter{ 0 };

		std::string m_sceneStatusMessage
		{
			"Scene file: Scene.dx3dscene"
		};

		bool m_isRunning{ true };

		std::chrono::steady_clock::time_point m_previousTime{};
	};
}
