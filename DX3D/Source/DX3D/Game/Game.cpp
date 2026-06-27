#include <DX3D/Game/Game.h>
#include <DX3D/Window/Window.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Core/Logger.h>
#include <DX3D/Input/InputSystem.h>
#include <DX3D/Game/Display.h>
#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/WorldRenderer.h>
<<<<<<< Updated upstream
=======
#include <DX3D/Editor/ViewportPicker.h>

#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>

#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iterator>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
>>>>>>> Stashed changes



dx3d::Game::Game(const GameDesc& desc)
{
	m_logger = std::make_unique<Logger>(desc.logLevel);

	DX3DLogInfo("GDENG03 | DirectX Game Engine");
	DX3DLogInfo("--------------------------------------");

	m_inputSystem = std::make_unique<InputSystem>(InputSystemDesc{ *m_logger });
	m_graphicsDevice = std::make_shared<GraphicsDevice>(GraphicsDeviceDesc{ *m_logger });
	m_display = std::make_unique<Display>(DisplayDesc{ {*m_logger,desc.windowSize},*m_graphicsDevice });
	m_world = std::make_unique<World>(WorldDesc{ BaseDesc{*m_logger}, GameContext{*m_inputSystem} });
	m_worldRenderer = std::make_unique<WorldRenderer>(WorldRendererDesc{ {*m_logger},*m_graphicsDevice });

	m_inputSystem->setCursorLockArea(m_display->getClientAreaInScreenSpace());

	DX3DLogInfo("Game initialized.");
}

dx3d::Game::~Game()
{
	DX3DLogInfo("Game is shutting down...");
}

dx3d::World& dx3d::Game::getWorld() noexcept
{
	return *m_world;
}

dx3d::Logger& dx3d::Game::getLogger() noexcept
{
	return *m_logger;
}

dx3d::InputSystem& dx3d::Game::getInputSystem() noexcept
{
	return *m_inputSystem;
}

void dx3d::Game::requestExit() noexcept
{
	m_isRunning = false;
}

void dx3d::Game::onInternalUpdate()
{
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<f32> delta = currentTime - m_previousTime;
	m_previousTime = currentTime;
	auto deltaTime = delta.count();

	m_inputSystem->update();

<<<<<<< Updated upstream
=======
	if (!m_display->update())
	{
		return;
	}

	m_inputSystem->setCursorLockArea(
		m_display->getClientAreaInScreenSpace()
	);

	// Begin the ImGui frame.
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

>>>>>>> Stashed changes
	onUpdate(deltaTime);

	if (!m_isRunning)
		return;

	m_world->update(deltaTime);

	m_worldRenderer->render(
		*m_world,
		m_display->getSwapChain(),
		deltaTime
	);
<<<<<<< Updated upstream
=======

	// --------------------
// Main editor menu
// --------------------

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem(
				"New Scene",
				nullptr,
				false,
				false
			);

			ImGui::MenuItem(
				"Save Scene",
				nullptr,
				false,
				false
			);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Game Object"))
		{
			if (ImGui::MenuItem("Create Cube"))
			{
				++m_cubeCounter;

				auto* cube =
					m_world->createGameObject<GameObject>();

				if (m_cubeCounter == 1)
				{
					cube->setName("Cube");
				}
				else
				{
					cube->setName(
						"Cube (" +
						std::to_string(m_cubeCounter) +
						")"
					);
				}

				cube->createOrGetComponent<
					CubeComponent>();

				cube->getTransform().setPosition(
					{
						static_cast<f32>(
							m_cubeCounter - 1
						) * 1.5f,
						0.5f,
						0.0f
					}
				);

				cube->getTransform().setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(cube);
			}

			if (ImGui::MenuItem("Create Plane"))
			{
				++m_planeCounter;

				auto* plane =
					m_world->createGameObject<GameObject>();

				if (m_planeCounter == 1)
				{
					plane->setName("Plane");
				}
				else
				{
					plane->setName(
						"Plane (" +
						std::to_string(m_planeCounter) +
						")"
					);
				}

				plane->createOrGetComponent<
					PlaneComponent>();

				plane->getTransform().setPosition(
					{
						static_cast<f32>(
							m_planeCounter - 1
						) * 5.0f,
						0.0f,
						0.0f
					}
				);

				plane->getTransform().setScale(
					{ 4.0f, 1.0f, 4.0f }
				);

				selectOnly(plane);
			}

			ui32 directionalLightCount = 0;

			m_world->getComponents<
				DirectionalLightComponent
			>(
				directionalLightCount
			);

			const bool canCreateDirectionalLight =
				directionalLightCount == 0;

			if (ImGui::MenuItem(
				"Create Directional Light",
				nullptr,
				false,
				canCreateDirectionalLight
			))
			{
				auto* lightObject =
					m_world->createGameObject<GameObject>();

				lightObject->setName(
					"Directional Light"
				);

				auto* lightComponent =
					lightObject->createOrGetComponent<
					DirectionalLightComponent
					>();

				lightComponent->setColor(
					{ 1.0f, 1.0f, 1.0f }
				);

				lightComponent->setIntensity(
					1.0f
				);

				lightComponent->setAmbientStrength(
					0.20f
				);

				lightComponent->setShadowArea(
					30.0f
				);

				lightComponent->setCastShadows(
					true
				);

				auto& lightTransform =
					lightObject->getTransform();

				lightTransform.setPosition(
					{ 0.0f, 3.0f, 0.0f }
				);

				lightTransform.setRotation(
					{ 1.04f, 1.03f, 0.0f }
				);

				lightTransform.setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(
					lightObject
				);
			}

			ImGui::Separator();

			const bool canMergeSelected =
				canMergeSelectedObjects();

			if (ImGui::MenuItem(
				"Merge Selected",
				nullptr,
				false,
				canMergeSelected
			))
			{
				mergeSelectedObjects();
			}

			const bool canDeleteSelected =
				m_selectedObject != nullptr &&
				m_selectedObject->getComponent<
				CameraComponent>() == nullptr;

			if (ImGui::MenuItem(
				"Delete Selected",
				"Delete",
				false,
				canDeleteSelected
			))
			{
				GameObject* objectToDelete =
					m_selectedObject;

				m_world->destroyGameObject(
					objectToDelete
				);

				removeObjectFromSelection(
					objectToDelete
				);
			}

			ImGui::EndMenu();
		}



		if (ImGui::BeginMenu("Gizmo"))
		{
			if (ImGui::MenuItem(
				"Translate",
				"W",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Translate
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Translate
				);
			}

			if (ImGui::MenuItem(
				"Rotate",
				"E",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Rotate
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Rotate
				);
			}

			if (ImGui::MenuItem(
				"Scale",
				"R",
				m_transformGizmo.getOperation() ==
				TransformGizmo::Operation::Scale
			))
			{
				m_transformGizmo.setOperation(
					TransformGizmo::Operation::Scale
				);
			}



			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	const ImGuiViewport* viewport =
		ImGui::GetMainViewport();

	const ImVec2 workPosition =
		viewport->WorkPos;

	const ImVec2 workSize =
		viewport->WorkSize;

	constexpr float panelWidth = 280.0f;
	constexpr float outlinerHeight = 220.0f;

	CameraComponent* editorCameraComponent = nullptr;

	const auto sceneObjects =
		m_world->getGameObjects();

	for (auto* object : sceneObjects)
	{
		if (!object)
			continue;

		auto* cameraComponent =
			object->getComponent<CameraComponent>();

		if (cameraComponent)
		{
			editorCameraComponent =
				cameraComponent;

			break;
		}
	}

	const TransformGizmo::ViewportArea gizmoViewport
	{
		viewport->Pos.x,
		viewport->Pos.y,
		viewport->Size.x,
		viewport->Size.y
	};

	m_transformGizmo.draw(
		m_selectedObject,
		editorCameraComponent,
		gizmoViewport
	);

	// Scene Outliner begins here.
	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x - panelWidth,
			workPosition.y
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			outlinerHeight
		},
		ImGuiCond_Always
	);

	ImGui::Begin("Scene Outliner");

	const auto objects = m_world->getGameObjects();

	for (auto* object : objects)
	{
		if (!object)
			continue;

		const bool isSelected =
			isObjectSelected(object);

		// Allows objects to have duplicate visible names
		// while still having unique ImGui identifiers.
		ImGui::PushID(object);

		if (ImGui::Selectable(
			object->getName().c_str(),
			isSelected
		))
		{
			const bool controlHeld =
				ImGui::GetIO().KeyCtrl;

			if (controlHeld)
			{
				toggleObjectSelection(object);
			}
			else
			{
				selectOnly(object);
			}
		}

		ImGui::PopID();
	}

	ImGui::End();

	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x - panelWidth,
			workPosition.y + outlinerHeight
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			panelWidth,
			workSize.y - outlinerHeight
		},
		ImGuiCond_Always
	);

	ImGui::Begin("Inspector Window");

	if (!m_selectedObject)
	{
		ImGui::TextDisabled(
			"No object selected. Select an object in the Scene Outliner."
		);
	}
	else
	{
		ImGui::Text("Selected Object");
		ImGui::Separator();

		ImGui::Text(
			"%s",
			m_selectedObject->getName().c_str()
		);

		const char* gizmoModeName = "Translate";

		switch (m_transformGizmo.getOperation())
		{
		case TransformGizmo::Operation::Translate:
			gizmoModeName = "Translate";
			break;

		case TransformGizmo::Operation::Rotate:
			gizmoModeName = "Rotate";
			break;

		case TransformGizmo::Operation::Scale:
			gizmoModeName = "Scale";
			break;
		}

		ImGui::TextDisabled(
			"Gizmo Mode: %s",
			gizmoModeName
		);

		auto& transform =
			m_selectedObject->getTransform();

		auto position = transform.getPosition();
		auto rotation = transform.getRotation();
		auto scale = transform.getScale();

		float positionValues[3] =
		{
			position.x,
			position.y,
			position.z
		};

		float rotationValues[3] =
		{
			rotation.x,
			rotation.y,
			rotation.z
		};

		float scaleValues[3] =
		{
			scale.x,
			scale.y,
			scale.z
		};

		ImGui::Spacing();

		if (ImGui::DragFloat3(
			"Position",
			positionValues,
			0.05f
		))
		{
			transform.setPosition(
				{
					positionValues[0],
					positionValues[1],
					positionValues[2]
				}
			);
		}



		if (ImGui::DragFloat3(
			"Rotation",
			rotationValues,
			0.01f
		))
		{
			transform.setRotation(
				{
					rotationValues[0],
					rotationValues[1],
					rotationValues[2]
				}
			);
		}

		if (ImGui::DragFloat3(
			"Scale",
			scaleValues,
			0.05f
		))
		{
			transform.setScale(
				{
					scaleValues[0],
					scaleValues[1],
					scaleValues[2]
				}
			);
		}

		if (auto* directionalLight =
			m_selectedObject->getComponent<
			DirectionalLightComponent
			>())
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Directional Light");
			ImGui::Spacing();

			Vec3 lightColor =
				directionalLight->getColor();

			float colorValues[3]
			{
				lightColor.x,
				lightColor.y,
				lightColor.z
			};

			if (ImGui::ColorEdit3(
				"Color",
				colorValues
			))
			{
				directionalLight->setColor(
					{
						colorValues[0],
						colorValues[1],
						colorValues[2]
					}
				);
			}

			float intensity =
				directionalLight->getIntensity();

			if (ImGui::DragFloat(
				"Intensity",
				&intensity,
				0.05f,
				0.0f,
				10.0f
			))
			{
				directionalLight->setIntensity(
					intensity
				);
			}

			float ambientStrength =
				directionalLight->
				getAmbientStrength();

			if (ImGui::SliderFloat(
				"Ambient Strength",
				&ambientStrength,
				0.0f,
				1.0f
			))
			{
				directionalLight->
					setAmbientStrength(
						ambientStrength
					);
			}

			float shadowArea =
				directionalLight->getShadowArea();

			if (ImGui::DragFloat(
				"Shadow Area",
				&shadowArea,
				0.5f,
				1.0f,
				200.0f
			))
			{
				directionalLight->setShadowArea(
					shadowArea
				);
			}

			bool castShadows =
				directionalLight->
				getCastShadows();

			if (ImGui::Checkbox(
				"Cast Shadows",
				&castShadows
			))
			{
				directionalLight->
					setCastShadows(
						castShadows
					);
			}
		}
	}

	ImGui::End();

	handleViewportPicking(
		editorCameraComponent,
		gizmoViewport
	);

	// --------------------------------------------------
// Transform gizmo mode shortcuts
// --------------------------------------------------

	const bool rightMouseDown =
		m_inputSystem->isKeyDown(
			KeyCode::MouseRight
		);

	const bool editingImGuiValue =
		ImGui::IsAnyItemActive();

	const bool typingInImGui =
		ImGui::GetIO().WantTextInput;

	const bool popupIsOpen =
		ImGui::IsPopupOpen(
			nullptr,
			ImGuiPopupFlags_AnyPopupId
		);

	const bool allowGizmoShortcuts =
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen;

	if (allowGizmoShortcuts)
	{
		if (m_inputSystem->isKeyPressed(KeyCode::W))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Translate
			);
		}

		if (m_inputSystem->isKeyPressed(KeyCode::E))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Rotate
			);
		}

		if (m_inputSystem->isKeyPressed(KeyCode::R))
		{
			m_transformGizmo.setOperation(
				TransformGizmo::Operation::Scale
			);
		}
	}

	// --------------------------------------------------
// Object clipboard shortcuts
// Ctrl + C = copy active object
// Ctrl + V = paste copied object
// --------------------------------------------------

	const bool controlHeld =
		ImGui::GetIO().KeyCtrl;

	const bool allowClipboardShortcuts =
		controlHeld &&
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen;

	if (allowClipboardShortcuts)
	{
		if (m_inputSystem->isKeyPressed(
			KeyCode::C
		))
		{
			copySelectedObject();
		}
		else if (m_inputSystem->isKeyPressed(
			KeyCode::V
		))
		{
			pasteCopiedObject();
		}
	}

	// Delete the selected scene object using the keyboard.
	const bool deletePressed =
		m_inputSystem->isKeyPressed(KeyCode::Delete);

	const bool editingInspectorValue =
		ImGui::IsAnyItemActive();

	const bool typingText =
		ImGui::GetIO().WantTextInput;

	if (deletePressed &&
		!m_transformGizmo.isUsing() &&
		!editingInspectorValue &&
		!typingText &&
		m_selectedObject &&
		m_selectedObject->getComponent<CameraComponent>() == nullptr)
	{
		GameObject* objectToDelete =
			m_selectedObject;

		m_world->destroyGameObject(
			objectToDelete
		);

		removeObjectFromSelection(
			objectToDelete
		);
	}

	// Render the UI over the 3D scene.
	ImGui::Render();

	m_graphicsDevice->bindBackBuffer(
		m_display->getSwapChain()
	);

	ImGui_ImplDX11_RenderDrawData(
		ImGui::GetDrawData()
	);

	// Present only after both the scene and UI are rendered.
	m_display->getSwapChain().present();
>>>>>>> Stashed changes
}
