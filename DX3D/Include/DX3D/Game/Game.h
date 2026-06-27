#pragma once
#include <DX3D/Core/Base.h>
#include <DX3D/Core/Core.h>
#include <chrono>

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
		void onInternalUpdate();
	private:
		UniquePtr<Logger> m_logger{};
		UniquePtr<InputSystem> m_inputSystem{};
		RefPtr<GraphicsDevice> m_graphicsDevice{};
		UniquePtr<Display> m_display{};
		UniquePtr<World> m_world{};	
		
		UniquePtr<WorldRenderer> m_worldRenderer{};

<<<<<<< Updated upstream
=======
		GameObject* m_selectedObject{};
		std::vector<GameObject*> m_selectedObjects{};

		ObjectCopyData m_objectClipboard{};

		TransformGizmo m_transformGizmo{};

		ui32 m_cubeCounter{ 0 };
		ui32 m_planeCounter{ 0 };

		// Current transform gizmo mode.
	/*	GizmoOperation m_gizmoOperation{
			GizmoOperation::Translate
		};*/

>>>>>>> Stashed changes
		bool m_isRunning{ true };

		std::chrono::steady_clock::time_point m_previousTime{};
	};
}
