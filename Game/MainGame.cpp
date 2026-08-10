#include "MainGame.h"

#include <DX3D/Component/CircleComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>

#include <imgui.h>

#include <string>
#include <algorithm>
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

/*	// Create one uniformly scaled cube at the center.
	auto& world = getWorld();
	m_warpCube =
		world.createGameObject<dx3d::GameObject>();

	m_warpCube->setName(
		"Warping Cube"
	);

	m_warpCube->createOrGetComponent<
		dx3d::CubeComponent>();

	m_warpCube->getTransform().setPosition(
		{ 0.0f, 0.0f, 0.0f }
	);

	m_warpCube->getTransform().setRotation(
		{ 0.0f, 0.0f, 0.0f }
	);

	// Initial uniform cube scale.
	m_warpCube->getTransform().setScale(
		{ 1.0f, 1.0f, 1.0f }
	);*/

/*	// Create 50 cubes at random positions.
	std::random_device randomDevice;
	std::mt19937 randomGenerator(
		randomDevice()
	);

	std::uniform_real_distribution<dx3d::f32>
		positionXDistribution(
			-5.0f,
			5.0f
		);

	std::uniform_real_distribution<dx3d::f32>
		positionYDistribution(
			-2.5f,
			2.5f
		);

	std::uniform_real_distribution<dx3d::f32>
		positionZDistribution(
			0.0f,
			10.0f
		);

	std::uniform_real_distribution<dx3d::f32>
		rotationDistribution(
			0.0f,
			2.0f * dx3d::MathUtils::PI
		);

	constexpr dx3d::ui32 cubeCount = 50;

	for (
		dx3d::ui32 cubeIndex = 0;
		cubeIndex < cubeCount;
		++cubeIndex
		)
	{
		auto* cube =
			world.createGameObject<
			dx3d::GameObject>();

		cube->setName(
			"Cube " +
			std::to_string(cubeIndex + 1)
		);

		cube->createOrGetComponent<
			dx3d::CubeComponent>();

		cube->getTransform().setPosition(
			{
				positionXDistribution(
					randomGenerator
				),
				positionYDistribution(
					randomGenerator
				),
				positionZDistribution(
					randomGenerator
				)
			}
		);

		// Give every cube a random starting orientation.
		cube->getTransform().setRotation(
			{
				rotationDistribution(
					randomGenerator
				),
				rotationDistribution(
					randomGenerator
				),
				rotationDistribution(
					randomGenerator
				)
			}
		);

		cube->getTransform().setScale(
			{ 0.6f, 0.6f, 0.6f }
		);
	}*/

	// Cube
	//auto cube =
/*		world.createGameObject<dx3d::GameObject>();

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
	);*/

	// Single rotating white cube
		// Single animated rainbow cube
/*		m_animatedCube =
		world.createGameObject<dx3d::GameObject>();

	m_animatedCube->setName(
		"Animated Rainbow Cube"
	);

	m_animatedCube->createOrGetComponent<
		dx3d::CubeComponent>();

	// Starting point of the animation.
	m_animatedCube->getTransform().setPosition(
		{ -1.5f, 0.0f, 0.0f }
	);

	m_animatedCube->getTransform().setRotation(
		{ 0.0f, 0.55f, 0.0f }
	);

	m_animatedCube->getTransform().setScale(
		{ 1.0f, 1.0f, 1.0f }
	);*/

	// Ground plane
	/*auto plane =
		world.createGameObject<dx3d::GameObject>();

	plane->setName("Plane");

	plane->createOrGetComponent<
		dx3d::PlaneComponent>();

	plane->getTransform().setPosition(
		{ 0.0f, 0.0f, 0.0f }
	);

	plane->getTransform().setScale(
		{ 10.0f, 1.0f, 10.0f }
	);*/

	getInputSystem().setCursorLocked(false);
	getInputSystem().setCursorVisible(true);
}

void MainGame::onUpdate(dx3d::f32 deltaTime)
{
	Game::onUpdate(deltaTime);

	auto& input = getInputSystem();
	m_sceneInputActions.update(input);

/*	// Continuously rotate every cube around X, Y, and Z.
	dx3d::ui32 currentCubeCount = 0;

	auto cubeComponents =
		getWorld().getComponents<
		dx3d::CubeComponent>(
			currentCubeCount
		);

	for (
		dx3d::ui32 cubeIndex = 0;
		cubeIndex < currentCubeCount;
		++cubeIndex
		)
	{
		auto* cubeComponent =
			cubeComponents[cubeIndex];

		if (!cubeComponent)
			continue;

		auto& cubeTransform =
			cubeComponent
			->getGameObject()
			.getTransform();

		auto rotation =
			cubeTransform.getRotation();

		const dx3d::f32 speedOffset =
			static_cast<dx3d::f32>(
				cubeIndex
				) * 0.01f;

		rotation.x +=
			(0.6f + speedOffset) *
			deltaTime;

		rotation.y +=
			(0.9f + speedOffset * 1.5f) *
			deltaTime;

		rotation.z +=
			(1.2f + speedOffset * 2.0f) *
			deltaTime;

		cubeTransform.setRotation(
			rotation
		);
	}*/

	// Continuously rotate the cube on the X, Y, and Z axes.
/*	if (m_rotatingCube)
	{
		auto rotation =
			m_rotatingCube->getTransform().getRotation();

		rotation.x += 0.7f * deltaTime;
		rotation.y += 1.0f * deltaTime;
		rotation.z += 1.3f * deltaTime;

		m_rotatingCube->getTransform().setRotation(
			rotation
		);
	}*/

	// Move the cube along X and Y while uniformly scaling it.
/*	if (m_animatedCube)
	{
		// Controls how quickly interpolation moves from 0 to 1.
		constexpr dx3d::f32 animationSpeed = 0.4f;

		m_animationProgress +=
			m_animationDirection *
			animationSpeed *
			deltaTime;

		// Reverse the animation at both endpoints.
		if (m_animationProgress >= 1.0f)
		{
			m_animationProgress = 1.0f;
			m_animationDirection = -1.0f;
		}
		else if (m_animationProgress <= 0.0f)
		{
			m_animationProgress = 0.0f;
			m_animationDirection = 1.0f;
		}

		const dx3d::f32 t =
			m_animationProgress;

		// Starting and ending positions.
		const dx3d::Vec3 startPosition
		{
			-1.5f,
			0.0f,
			0.0f
		};

		const dx3d::Vec3 endPosition
		{
			1.5f,
			2.0f,
			0.0f
		};

		// Linear interpolation:
		// result = start + (end - start) * t
		const dx3d::Vec3 currentPosition
		{
			startPosition.x +
				(endPosition.x - startPosition.x) * t,

			startPosition.y +
				(endPosition.y - startPosition.y) * t,

			startPosition.z +
				(endPosition.z - startPosition.z) * t
		};

		// Uniformly interpolate from 1.0 to 0.25.
		constexpr dx3d::f32 startingScale = 1.0f;
		constexpr dx3d::f32 endingScale = 0.25f;

		const dx3d::f32 currentScale =
			startingScale +
			(endingScale - startingScale) * t;

		auto& cubeTransform =
			m_animatedCube->getTransform();

		cubeTransform.setPosition(
			currentPosition
		);

		cubeTransform.setScale(
			{
				currentScale,
				currentScale,
				currentScale
			}
		);
	}*/

	// Warp the uniformly scaled cube into a horizontal plane.
/*if (m_warpCube && !m_warpFinished)
{
	m_warpElapsedTime += deltaTime;


	constexpr dx3d::f32 holdDuration =
		1.0f;

	constexpr dx3d::f32 warpDuration =
		3.0f;

	dx3d::f32 warpProgress =
		(m_warpElapsedTime - holdDuration) /
		warpDuration;

	if (warpProgress < 0.0f)
	{
		warpProgress = 0.0f;
	}

	if (warpProgress >= 1.0f)
	{
		warpProgress = 1.0f;
		m_warpFinished = true;
	}

	// a more natural warping appearance.
	const dx3d::f32 smoothProgress =
		warpProgress *
		warpProgress *
		(3.0f - 2.0f * warpProgress);

	const dx3d::Vec3 cubeScale
	{
		1.0f,
		1.0f,
		1.0f
	};

	// X and Z become larger while Y becomes very thin.
	const dx3d::Vec3 planeScale
	{
		4.0f,
		0.05f,
		3.0f
	};

	const dx3d::Vec3 currentScale
	{
		cubeScale.x +
			(planeScale.x - cubeScale.x) *
			smoothProgress,

		cubeScale.y +
			(planeScale.y - cubeScale.y) *
			smoothProgress,

		cubeScale.z +
			(planeScale.z - cubeScale.z) *
			smoothProgress
	};

	m_warpCube->getTransform().setScale(
		currentScale
	);
}*/

	auto* editorCamera = getEditorCamera();
	if (!editorCamera)
		return;

	bool imguiWantsTextInput = false;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		const ImGuiIO& io = ImGui::GetIO();
		imguiWantsTextInput = io.WantTextInput;
	}

	const bool rightMousePressed =
		input.isKeyPressed(dx3d::KeyCode::MouseRight);

	const bool rightMouseReleased =
		input.isKeyReleased(dx3d::KeyCode::MouseRight);

	const bool rightMouseDown =
		input.isKeyDown(dx3d::KeyCode::MouseRight);
	const bool leftMousePressed =
		input.isKeyPressed(dx3d::KeyCode::MouseLeft);
	const bool leftMouseReleased =
		input.isKeyReleased(dx3d::KeyCode::MouseLeft);
	const bool leftMouseDown =
		input.isKeyDown(dx3d::KeyCode::MouseLeft);
	const bool altDown = ImGui::GetCurrentContext() && ImGui::GetIO().KeyAlt;


	const bool sceneViewportHovered = isSceneViewportHovered();
	const bool sceneViewportFocused = isSceneViewportFocused();
	if (!sceneViewportFocused &&
		(m_isCameraControlActive || m_isOrbitActive || m_isDollyActive))
	{
		m_isCameraControlActive = false;
		m_isOrbitActive = false;
		m_isDollyActive = false;
		input.setCursorLocked(false);
		input.setCursorVisible(true);
	}
	if (rightMousePressed && sceneViewportHovered &&
		!m_isCameraControlActive && !m_isDollyActive)
	{
		m_isDollyActive = altDown;
		m_isCameraControlActive = !altDown;

		input.setCursorVisible(false);
		input.setCursorLocked(true);
	}
	if (leftMousePressed && altDown && sceneViewportHovered && !m_isOrbitActive)
	{
		m_isOrbitActive = true;
		auto& cameraTransform = editorCamera->getTransform();
		if (auto* selected = getPrimarySelectedObject())
		{
			const auto worldRow = selected->getTransform().getAffineWorldMatrix().row(3);
			m_orbitPivot = { worldRow.x, worldRow.y, worldRow.z };
			const dx3d::Vec3 cameraPosition = cameraTransform.getPosition();
			const float deltaX = cameraPosition.x - m_orbitPivot.x;
			const float deltaY = cameraPosition.y - m_orbitPivot.y;
			const float deltaZ = cameraPosition.z - m_orbitPivot.z;
			m_orbitDistance = std::max(0.25f,
				std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ));
			m_orbitPivotValid = true;
		}
		else if (!m_orbitPivotValid)
		{
			const dx3d::Vec3 position = cameraTransform.getPosition();
			const dx3d::Vec3 forward = cameraTransform.forward();
			m_orbitPivot = position + forward * m_orbitDistance;
			m_orbitPivotValid = true;
		}
		input.setCursorVisible(false);
		input.setCursorLocked(true);
	}


	if ((m_isCameraControlActive || m_isDollyActive) &&
		(rightMouseReleased || !rightMouseDown))
	{
		m_isCameraControlActive = false;
		m_isDollyActive = false;

		if (!m_isOrbitActive)
		{
			input.setCursorLocked(false);
			input.setCursorVisible(true);
		}
	}
	if (m_isOrbitActive && (leftMouseReleased || !leftMouseDown || !altDown))
	{
		m_isOrbitActive = false;
		if (!m_isCameraControlActive && !m_isDollyActive)
		{
			input.setCursorLocked(false);
			input.setCursorVisible(true);
		}
	}


	auto& transform = editorCamera->getTransform();

	if ((m_isCameraControlActive || m_isOrbitActive) &&
		!rightMousePressed && !leftMousePressed)
	{
		const auto mouseDelta = input.getMouseDelta();

		m_cameraYaw +=
			mouseDelta.x * m_cameraLookSpeed;

		m_cameraPitch +=
			mouseDelta.y * m_cameraLookSpeed;


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

		if (m_isOrbitActive && m_orbitPivotValid)
		{
			const dx3d::Vec3 forward = transform.forward();
			transform.setPosition({
				m_orbitPivot.x - forward.x * m_orbitDistance,
				m_orbitPivot.y - forward.y * m_orbitDistance,
				m_orbitPivot.z - forward.z * m_orbitDistance });
		}
	}

	const bool middleMouseDown =
		sceneViewportHovered && input.isKeyDown(dx3d::KeyCode::MouseMiddle);
	const float wheelDelta = sceneViewportHovered && ImGui::GetCurrentContext()
		? ImGui::GetIO().MouseWheel : 0.0f;
	if (sceneViewportFocused && !imguiWantsTextInput &&
		input.isKeyPressed(dx3d::KeyCode::F))
	{
		if (auto* selected = getPrimarySelectedObject())
		{
			const auto worldRow = selected->getTransform().getAffineWorldMatrix().row(3);
			m_orbitPivot = { worldRow.x, worldRow.y, worldRow.z };
			const dx3d::Vec3 scale = selected->getTransform().getScale();
			const float extent = std::max({ std::fabs(scale.x), std::fabs(scale.y),
				std::fabs(scale.z), 0.5f });
			m_orbitDistance = std::max(2.0f, extent * 3.0f);
			const dx3d::Vec3 forward = transform.forward();
			transform.setPosition({
				m_orbitPivot.x - forward.x * m_orbitDistance,
				m_orbitPivot.y - forward.y * m_orbitDistance,
				m_orbitPivot.z - forward.z * m_orbitDistance });
			m_orbitPivotValid = true;
		}
	}

	const bool arrowNavigation = sceneViewportFocused && !imguiWantsTextInput &&
		(input.isKeyDown(dx3d::KeyCode::Up) || input.isKeyDown(dx3d::KeyCode::Down) ||
		 input.isKeyDown(dx3d::KeyCode::Left) || input.isKeyDown(dx3d::KeyCode::Right));
	if (!m_isCameraControlActive && !m_isOrbitActive && !m_isDollyActive &&
		!middleMouseDown && wheelDelta == 0.0f && !arrowNavigation)
		return;
	if (imguiWantsTextInput && m_isCameraControlActive)
		return;

	const dx3d::f32 currentSpeed =
		m_sceneInputActions.isDown("Boost")
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


	if (m_isCameraControlActive)
	{
		const dx3d::f32 forwardInput = m_sceneInputActions.getValue("MoveForward");
		const dx3d::f32 rightInput = m_sceneInputActions.getValue("MoveRight");
		const dx3d::f32 upInput = m_sceneInputActions.getValue("MoveUp");
		movement = movement + forward * forwardInput;
		movement = movement + right * rightInput;
		movement.y += upInput;
	}
	else if (arrowNavigation)
	{
		if (input.isKeyDown(dx3d::KeyCode::Up)) movement = movement + forward;
		if (input.isKeyDown(dx3d::KeyCode::Down)) movement = movement + forward * -1.0f;
		if (input.isKeyDown(dx3d::KeyCode::Right)) movement = movement + right;
		if (input.isKeyDown(dx3d::KeyCode::Left)) movement = movement + right * -1.0f;
	}

	const dx3d::f32 movementLength =
		std::sqrt(
			movement.x * movement.x +
			movement.y * movement.y +
			movement.z * movement.z
		);

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
		m_orbitPivotValid = false;
	}

	if (middleMouseDown || wheelDelta != 0.0f || m_isDollyActive)
	{
		const auto mouseDelta = input.getMouseDelta();
		auto position = transform.getPosition();
		const dx3d::Vec3 up = transform.up();
		const float frameScale = deltaTime * 60.0f;
		if (middleMouseDown)
		{
			const float panScale = std::max(m_cameraPanSpeed,
				m_orbitDistance * 0.0015f) * frameScale;
			const dx3d::Vec3 pan = right * (-mouseDelta.x * panScale) +
				up * (mouseDelta.y * panScale);
			position = position + pan;
			if (m_orbitPivotValid) m_orbitPivot = m_orbitPivot + pan;
		}
		if (wheelDelta != 0.0f)
		{
			const float zoomAmount = wheelDelta *
				std::max(0.5f, m_orbitDistance * 0.15f) * frameScale;
			position = position + forward * zoomAmount;
			if (m_orbitPivotValid)
				m_orbitDistance = std::max(0.25f, m_orbitDistance - zoomAmount);
		}
		if (m_isDollyActive)
		{
			const float dollyAmount = -mouseDelta.y *
				std::max(0.01f, m_orbitDistance * 0.01f);
			position = position + forward * dollyAmount;
			if (m_orbitPivotValid)
				m_orbitDistance = std::max(0.25f, m_orbitDistance - dollyAmount);
		}
		transform.setPosition(position);
	}
}
