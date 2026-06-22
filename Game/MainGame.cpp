#include "MainGame.h"

#include <DX3D/Component/CircleComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>

#include <imgui.h>

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

	m_editorCamera =
		world.createGameObject<dx3d::GameObject>();

	m_editorCamera->setName("Editor Camera");

	auto camera = m_editorCamera;

	auto cameraComponent =
		camera->createOrGetComponent<
		dx3d::CameraComponent>();

	cameraComponent->setNearPlane(0.1f);
	cameraComponent->setFarPlane(100.0f);
	cameraComponent->setFieldOfView(1.0f);

	camera->getTransform().setPosition(
		{ 0.0f, 3.0f, -6.0f }
	);

	// Positive X rotation looks downward in this engine.
	camera->getTransform().setRotation(
		{ 0.45f, 0.0f, 0.0f }
	);

	// Cube
	auto cube =
		world.createGameObject<dx3d::GameObject>();

	cube->setName("Cube");

	cube->createOrGetComponent<
		dx3d::CubeComponent>();

	// Cube has a height of one unit, so Y = 0.5
	// places its bottom directly on the plane.
	cube->getTransform().setPosition(
		{ 0.0f, 0.5f, 0.0f }
	);

	cube->getTransform().setRotation(
		{ 0.0f, 0.55f, 0.0f }
	);

	cube->getTransform().setScale(
		{ 1.0f, 1.0f, 1.0f }
	);

	// Ground plane
	auto plane =
		world.createGameObject<dx3d::GameObject>();

	plane->setName("Plane");

	plane->createOrGetComponent<
		dx3d::PlaneComponent>();

	plane->getTransform().setPosition(
		{ 0.0f, 0.0f, 0.0f }
	);

	plane->getTransform().setScale(
		{ 10.0f, 1.0f, 10.0f }
	);

	getInputSystem().setCursorLocked(false);
	getInputSystem().setCursorVisible(true);
}

void MainGame::onUpdate(dx3d::f32 deltaTime)
{
	Game::onUpdate(deltaTime);

	auto& input = getInputSystem();

	if (input.isKeyPressed(dx3d::KeyCode::Escape))
	{
		requestExit();
		return;
	}

	if (!m_editorCamera)
		return;

	bool imguiWantsMouse = false;
	bool imguiWantsKeyboard = false;

	// Check that ImGui has already been initialized before accessing its IO.
	if (ImGui::GetCurrentContext() != nullptr)
	{
		const ImGuiIO& io = ImGui::GetIO();

		imguiWantsMouse = io.WantCaptureMouse;

		// WantTextInput is included so movement also stops while
		// typing into an Inspector input field.
		imguiWantsKeyboard =
			io.WantCaptureKeyboard ||
			io.WantTextInput;
	}

	const bool rightMousePressed =
		input.isKeyPressed(dx3d::KeyCode::MouseRight);

	const bool rightMouseReleased =
		input.isKeyReleased(dx3d::KeyCode::MouseRight);

	const bool rightMouseDown =
		input.isKeyDown(dx3d::KeyCode::MouseRight);

	// Begin camera control only when the right-click did not
	// begin over an ImGui window or control.
	if (rightMousePressed &&
		!imguiWantsMouse &&
		!m_isCameraControlActive)
	{
		m_isCameraControlActive = true;

		input.setCursorVisible(false);
		input.setCursorLocked(true);
	}

	// End camera control when right mouse is released.
	//
	// The !rightMouseDown check also protects against cases where
	// the release event is missed, such as losing window focus.
	if (m_isCameraControlActive &&
		(rightMouseReleased || !rightMouseDown))
	{
		m_isCameraControlActive = false;

		input.setCursorLocked(false);
		input.setCursorVisible(true);
	}

	// Right-clicking an ImGui window will never activate this.
	if (!m_isCameraControlActive)
		return;

	auto& transform = m_editorCamera->getTransform();

	// Ignore the first mouse delta when camera control begins.
	if (!rightMousePressed)
	{
		const auto mouseDelta = input.getMouseDelta();

		m_cameraYaw +=
			mouseDelta.x * m_cameraLookSpeed;

		m_cameraPitch +=
			mouseDelta.y * m_cameraLookSpeed;

		// Prevent the camera from turning upside down.
		constexpr dx3d::f32 minimumPitch = -1.5f;
		constexpr dx3d::f32 maximumPitch = 1.5f;

		if (m_cameraPitch < minimumPitch)
			m_cameraPitch = minimumPitch;

		if (m_cameraPitch > maximumPitch)
			m_cameraPitch = maximumPitch;

		transform.setRotation(
			{
				m_cameraPitch,
				m_cameraYaw,
				0.0f
			}
		);
	}

	// Do not use keyboard movement while ImGui is accepting
	// keyboard input, such as while editing an Inspector value.
	if (imguiWantsKeyboard)
		return;

	const dx3d::f32 currentSpeed =
		input.isKeyDown(dx3d::KeyCode::Shift)
		? m_cameraFastSpeed
		: m_cameraMoveSpeed;

	dx3d::Vec3 movement
	{
		0.0f,
		0.0f,
		0.0f
	};

	const dx3d::Vec3 forward = transform.forward();
	const dx3d::Vec3 right = transform.right();

	// Forward and backward.
	if (input.isKeyDown(dx3d::KeyCode::W))
	{
		movement.x += forward.x;
		movement.y += forward.y;
		movement.z += forward.z;
	}

	if (input.isKeyDown(dx3d::KeyCode::S))
	{
		movement.x -= forward.x;
		movement.y -= forward.y;
		movement.z -= forward.z;
	}

	// Left and right.
	if (input.isKeyDown(dx3d::KeyCode::D))
	{
		movement.x += right.x;
		movement.y += right.y;
		movement.z += right.z;
	}

	if (input.isKeyDown(dx3d::KeyCode::A))
	{
		movement.x -= right.x;
		movement.y -= right.y;
		movement.z -= right.z;
	}

	// World-space vertical movement.
	if (input.isKeyDown(dx3d::KeyCode::E))
	{
		movement.y += 1.0f;
	}

	if (input.isKeyDown(dx3d::KeyCode::Q))
	{
		movement.y -= 1.0f;
	}

	const dx3d::f32 movementLength =
		std::sqrt(
			movement.x * movement.x +
			movement.y * movement.y +
			movement.z * movement.z
		);

	// Normalize so diagonal movement is not faster.
	if (movementLength > 0.0001f)
	{
		movement.x /= movementLength;
		movement.y /= movementLength;
		movement.z /= movementLength;

		const dx3d::f32 movementAmount =
			currentSpeed * deltaTime;

		auto position = transform.getPosition();

		position.x += movement.x * movementAmount;
		position.y += movement.y * movementAmount;
		position.z += movement.z * movementAmount;

		transform.setPosition(position);
	}
}