#pragma once

#include <DX3D/All.h>
#include <vector>

class MainGame : public dx3d::Game
{
public:
	explicit MainGame(const dx3d::GameDesc& desc);

protected:
	void onCreate() override;
	void onUpdate(dx3d::f32 deltaTime) override;

private:
	void spawnCircle();

	/*dx3d::GameObject* m_warpCube{};

	dx3d::f32 m_warpElapsedTime{ 0.0f };
	bool m_warpFinished{ false };*/

	dx3d::GameObject* m_animatedCube{};

	dx3d::f32 m_animationProgress{ 0.0f };
	dx3d::f32 m_animationDirection{ 1.0f };

	dx3d::f32 m_cameraYaw{ -0.633f };
	dx3d::f32 m_cameraPitch{ 0.322f };

	dx3d::f32 m_cameraMoveSpeed{ 10.0f };
	dx3d::f32 m_cameraFastSpeed{ 40.0f };
	dx3d::f32 m_cameraLookSpeed{ 0.003f };
	dx3d::f32 m_cameraPanSpeed{ 0.01f };
	dx3d::f32 m_cameraZoomSpeed{ 6.0f };

	bool m_isCameraControlActive{ false };
	bool m_isOrbitActive{ false };
	bool m_isDollyActive{ false };
	bool m_orbitPivotValid{ false };
	dx3d::Vec3 m_orbitPivot{};
	dx3d::f32 m_orbitDistance{ 8.5f };
	dx3d::InputActionMap m_sceneInputActions{ dx3d::InputActionMap::createDefaults() };

	struct CircleData
	{
		dx3d::GameObject* object{};
		dx3d::f32 velocityX{};
		dx3d::f32 velocityY{};
	};

	std::vector<CircleData> m_circles{};
	dx3d::f32 m_circleRadius{ 50.0f };
};
