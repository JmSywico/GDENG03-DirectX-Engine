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

	dx3d::GameObject* m_editorCamera{};

	dx3d::f32 m_cameraYaw{ 0.0f };
	dx3d::f32 m_cameraPitch{ 0.45f };

	dx3d::f32 m_cameraMoveSpeed{ 4.0f };
	dx3d::f32 m_cameraFastSpeed{ 12.0f };
	dx3d::f32 m_cameraLookSpeed{ 0.0025f };

	// True only when right mouse camera control was started
	// outside an ImGui window.
	bool m_isCameraControlActive{ false };

	struct CircleData
	{
		dx3d::GameObject* object{};
		dx3d::f32 velocityX{};
		dx3d::f32 velocityY{};
	};

	std::vector<CircleData> m_circles{};
	dx3d::f32 m_circleRadius{ 50.0f };
};