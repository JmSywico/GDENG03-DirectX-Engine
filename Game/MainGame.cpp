#include "MainGame.h"

#include <DX3D/Component/CircleComponent.h>
#include <cmath>
#include <random>

MainGame::MainGame(const dx3d::GameDesc& desc)
	: dx3d::Game(desc)
{}

void MainGame::spawnCircle()
{
	auto& world = getWorld();

	auto circle = world.createGameObject<dx3d::GameObject>();
	circle->createOrGetComponent<dx3d::CircleComponent>();

	circle->getTransform().setPosition(
		{ 0.0f, 0.0f, 0.0f }
	);

	circle->getTransform().setScale(
		{ 100.0f, 100.0f, 1.0f }
	);

	static std::random_device randomDevice;
	static std::mt19937 randomGenerator(randomDevice());

	static std::uniform_real_distribution<dx3d::f32> angleDistribution(
		0.0f,
		2.0f * dx3d::MathUtils::PI
	);

	const dx3d::f32 angle = angleDistribution(randomGenerator);
	const dx3d::f32 speed = 250.0f;

	CircleData circleData{};

	circleData.object = circle;
	circleData.velocityX = std::cos(angle) * speed;
	circleData.velocityY = std::sin(angle) * speed;

	m_circles.push_back(circleData);
}

void MainGame::onCreate()
{
	Game::onCreate();

	auto& world = getWorld();

	auto camera = world.createGameObject<dx3d::GameObject>();
	camera->createOrGetComponent<dx3d::CameraComponent>();
	camera->getTransform().setPosition({ 0.0f, 0.0f, -10.0f });

	spawnCircle();
	getInputSystem().setCursorLocked(false);
	getInputSystem().setCursorVisible(true);
}

void MainGame::onUpdate(dx3d::f32 deltaTime)
{
	Game::onUpdate(deltaTime);

	if (getInputSystem().isKeyPressed(dx3d::KeyCode::Escape))
	{
		requestExit();
		return;
	}

	auto& input = getInputSystem();
	auto& world = getWorld();

	// Space creates a new circle.
	if (input.isKeyPressed(dx3d::KeyCode::Space))
	{
		spawnCircle();
	}

	// Backspace removes the most recently created circle.
	if (input.isKeyPressed(dx3d::KeyCode::Backspace) &&
		!m_circles.empty())
	{
		world.destroyGameObject(m_circles.back().object);
		m_circles.pop_back();
	}

	// Delete removes every circle.
	if (input.isKeyPressed(dx3d::KeyCode::Delete))
	{
		for (auto& circle : m_circles)
		{
			if (circle.object)
			{
				world.destroyGameObject(circle.object);
			}
		}

		m_circles.clear();
	}

	constexpr dx3d::f32 halfWindowWidth = 512.0f;
	constexpr dx3d::f32 halfWindowHeight = 384.0f;

	const dx3d::f32 leftEdge =
		-halfWindowWidth + m_circleRadius;

	const dx3d::f32 rightEdge =
		halfWindowWidth - m_circleRadius;

	const dx3d::f32 bottomEdge =
		-halfWindowHeight + m_circleRadius;

	const dx3d::f32 topEdge =
		halfWindowHeight - m_circleRadius;

	// Move every circle independently.
	for (auto& circle : m_circles)
	{
		if (!circle.object)
			continue;

		auto& transform = circle.object->getTransform();
		auto position = transform.getPosition();

		position.x += circle.velocityX * deltaTime;
		position.y += circle.velocityY * deltaTime;

		if (position.x <= leftEdge)
		{
			position.x = leftEdge;
			circle.velocityX = std::abs(circle.velocityX);
		}
		else if (position.x >= rightEdge)
		{
			position.x = rightEdge;
			circle.velocityX = -std::abs(circle.velocityX);
		}

		if (position.y <= bottomEdge)
		{
			position.y = bottomEdge;
			circle.velocityY = std::abs(circle.velocityY);
		}
		else if (position.y >= topEdge)
		{
			position.y = topEdge;
			circle.velocityY = -std::abs(circle.velocityY);
		}

		transform.setPosition(position);
	}
}