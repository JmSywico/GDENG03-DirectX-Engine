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

	struct CircleData
	{
		dx3d::GameObject* object{};
		dx3d::f32 velocityX{};
		dx3d::f32 velocityY{};
	};

	std::vector<CircleData> m_circles{};
	dx3d::f32 m_circleRadius{ 50.0f };
};