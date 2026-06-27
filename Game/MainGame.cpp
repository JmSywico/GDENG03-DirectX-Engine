#include "MainGame.h"

<<<<<<< Updated upstream
#include <DX3D/Component/CircleComponent.h>
=======
#include <DX3D/Component/CameraComponent.h>

#include <imgui.h>

>>>>>>> Stashed changes
#include <cmath>

MainGame::MainGame(
	const dx3d::GameDesc& desc
)
	: dx3d::Game(desc)
{}

void MainGame::onCreate()
{
	dx3d::Game::onCreate();

	auto& world = getWorld();

<<<<<<< Updated upstream
	auto camera = world.createGameObject<dx3d::GameObject>();
	camera->createOrGetComponent<dx3d::CameraComponent>();
	camera->getTransform().setPosition({ 0.0f, 0.0f, -10.0f });

	spawnCircle();
	getInputSystem().setCursorLocked(false);
	getInputSystem().setCursorVisible(true);
=======
	m_editorCamera =
		world.createGameObject<
		dx3d::GameObject>();

	m_editorCamera->setName(
		"Editor Camera"
	);

	auto* cameraComponent =
		m_editorCamera->createOrGetComponent<
		dx3d::CameraComponent>();

	cameraComponent->setNearPlane(
		0.1f
	);

	cameraComponent->setFarPlane(
		100.0f
	);

	cameraComponent->setFieldOfView(
		1.0f
	);

	m_editorCamera->getTransform().setPosition(
		{ 0.0f, 3.0f, -6.0f }
	);

	m_editorCamera->getTransform().setRotation(
		{
			m_cameraPitch,
			m_cameraYaw,
			0.0f
		}
	);

	getInputSystem().setCursorLocked(
		false
	);

	getInputSystem().setCursorVisible(
		true
	);
>>>>>>> Stashed changes
}

void MainGame::onUpdate(
	dx3d::f32 deltaTime
)
{
	dx3d::Game::onUpdate(
		deltaTime
	);

<<<<<<< Updated upstream
	if (getInputSystem().isKeyPressed(dx3d::KeyCode::Escape))
=======
	auto& input =
		getInputSystem();

	if (input.isKeyPressed(
		dx3d::KeyCode::Escape
	))
>>>>>>> Stashed changes
	{
		requestExit();
		return;
	}

<<<<<<< Updated upstream
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
=======
	if (!m_editorCamera)
		return;

	bool imguiWantsMouse = false;
	bool imguiWantsKeyboard = false;

	if (ImGui::GetCurrentContext() != nullptr)
	{
		const ImGuiIO& io =
			ImGui::GetIO();

		imguiWantsMouse =
			io.WantCaptureMouse;

		imguiWantsKeyboard =
			io.WantCaptureKeyboard ||
			io.WantTextInput;
	}

	const bool rightMousePressed =
		input.isKeyPressed(
			dx3d::KeyCode::MouseRight
		);

	const bool rightMouseReleased =
		input.isKeyReleased(
			dx3d::KeyCode::MouseRight
		);

	const bool rightMouseDown =
		input.isKeyDown(
			dx3d::KeyCode::MouseRight
		);

	if (rightMousePressed &&
		!imguiWantsMouse &&
		!m_isCameraControlActive)
	{
		m_isCameraControlActive = true;

		input.setCursorVisible(
			false
		);

		input.setCursorLocked(
			true
		);
	}

	if (m_isCameraControlActive &&
		(rightMouseReleased ||
			!rightMouseDown))
	{
		m_isCameraControlActive = false;

		input.setCursorLocked(
			false
		);

		input.setCursorVisible(
			true
		);
	}

	if (!m_isCameraControlActive)
		return;

	auto& transform =
		m_editorCamera->getTransform();

	if (!rightMousePressed)
	{
		const auto mouseDelta =
			input.getMouseDelta();

		m_cameraYaw +=
			mouseDelta.x *
			m_cameraLookSpeed;

		m_cameraPitch +=
			mouseDelta.y *
			m_cameraLookSpeed;

		constexpr dx3d::f32 minimumPitch =
			-1.5f;

		constexpr dx3d::f32 maximumPitch =
			1.5f;

		if (m_cameraPitch < minimumPitch)
		{
			m_cameraPitch =
				minimumPitch;
		}

		if (m_cameraPitch > maximumPitch)
		{
			m_cameraPitch =
				maximumPitch;
		}

		transform.setRotation(
			{
				m_cameraPitch,
				m_cameraYaw,
				0.0f
			}
		);
	}

	if (imguiWantsKeyboard)
		return;

	const dx3d::f32 currentSpeed =
		input.isKeyDown(
			dx3d::KeyCode::Shift
		)
		? m_cameraFastSpeed
		: m_cameraMoveSpeed;

	dx3d::Vec3 movement
	{
		0.0f,
		0.0f,
		0.0f
	};

	const dx3d::Vec3 forward =
		transform.forward();

	const dx3d::Vec3 right =
		transform.right();

	if (input.isKeyDown(
		dx3d::KeyCode::W
	))
	{
		movement.x += forward.x;
		movement.y += forward.y;
		movement.z += forward.z;
	}

	if (input.isKeyDown(
		dx3d::KeyCode::S
	))
	{
		movement.x -= forward.x;
		movement.y -= forward.y;
		movement.z -= forward.z;
	}

	if (input.isKeyDown(
		dx3d::KeyCode::D
	))
	{
		movement.x += right.x;
		movement.y += right.y;
		movement.z += right.z;
	}

	if (input.isKeyDown(
		dx3d::KeyCode::A
	))
	{
		movement.x -= right.x;
		movement.y -= right.y;
		movement.z -= right.z;
	}

	if (input.isKeyDown(
		dx3d::KeyCode::E
	))
	{
		movement.y += 1.0f;
	}

	if (input.isKeyDown(
		dx3d::KeyCode::Q
	))
	{
		movement.y -= 1.0f;
	}

	const dx3d::f32 movementLength =
		std::sqrt(
			movement.x * movement.x +
			movement.y * movement.y +
			movement.z * movement.z
		);

	if (movementLength <= 0.0001f)
		return;

	movement.x /= movementLength;
	movement.y /= movementLength;
	movement.z /= movementLength;

	const dx3d::f32 movementAmount =
		currentSpeed * deltaTime;

	auto position =
		transform.getPosition();
>>>>>>> Stashed changes

	position.x +=
		movement.x * movementAmount;

	position.y +=
		movement.y * movementAmount;

	position.z +=
		movement.z * movementAmount;

	transform.setPosition(
		position
	);
}