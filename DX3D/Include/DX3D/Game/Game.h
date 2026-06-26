#pragma once

#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Math/Vec3.h>
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Core.h>
#include <DX3D/Editor/TransformGizmo.h>

#include <string>
#include <chrono>
#include <vector>

namespace dx3d
{
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
		/*enum class GizmoOperation
		{
			Translate,
			Rotate,
			Scale
		};*/

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

			// Used only when the copied object contains
			// a CombinedMeshComponent.
			MeshData meshData{};

			// Used to name repeated pasted copies.
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

		ui32 m_cubeCounter{ 1 };
		ui32 m_planeCounter{ 1 };

		// Current transform gizmo mode.
	/*	GizmoOperation m_gizmoOperation{
			GizmoOperation::Translate
		};*/

		bool m_isRunning{ true };

		std::chrono::steady_clock::time_point m_previousTime{};
	};
}