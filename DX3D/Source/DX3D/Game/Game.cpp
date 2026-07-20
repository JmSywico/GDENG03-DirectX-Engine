#include <DX3D/Game/Game.h>
#include <DX3D/Window/Window.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Graphics/MeshMerger.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Core/Logger.h>
#include <DX3D/Input/InputSystem.h>
#include <DX3D/Game/Display.h>
#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>
#include <DX3D/Game/WorldRenderer.h>
#include <DX3D/Game/SceneSerializer.h>
#include <DX3D/Editor/ViewportPicker.h>

#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>

#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iterator>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>



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

	// Initialize Dear ImGui.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(
		m_display->getNativeHandle()
	))
	{
		DX3DLogThrowError(
			"ImGui Win32 initialization failed."
		);
	}

	if (!ImGui_ImplDX11_Init(
		m_graphicsDevice->getD3DDevice(),
		m_graphicsDevice->getImmediateContext()
	))
	{
		DX3DLogThrowError(
			"ImGui DirectX 11 initialization failed."
		);
	}

	loadCreditsLogo();

	m_inputSystem->setCursorLockArea(m_display->getClientAreaInScreenSpace());

	DX3DLogInfo("Game initialized.");
}

dx3d::Game::~Game()
{
	DX3DLogInfo("Game is shutting down...");

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
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

bool dx3d::Game::isObjectSelected(
	const GameObject* object
) const noexcept
{
	if (!object)
		return false;

	return std::find(
		m_selectedObjects.begin(),
		m_selectedObjects.end(),
		object
	) != m_selectedObjects.end();
}

void dx3d::Game::selectOnly(
	GameObject* object
)
{
	m_selectedObjects.clear();
	m_selectedObject = object;

	if (object)
	{
		m_selectedObjects.push_back(object);
	}
}

void dx3d::Game::toggleObjectSelection(
	GameObject* object
)
{
	if (!object)
		return;

	const auto selectedIt =
		std::find(
			m_selectedObjects.begin(),
			m_selectedObjects.end(),
			object
		);

	if (selectedIt == m_selectedObjects.end())
	{
		m_selectedObjects.push_back(object);
		m_selectedObject = object;
		return;
	}

	m_selectedObjects.erase(selectedIt);

	if (m_selectedObject == object)
	{
		if (m_selectedObjects.empty())
		{
			m_selectedObject = nullptr;
		}
		else
		{
			m_selectedObject =
				m_selectedObjects.back();
		}
	}
}

void dx3d::Game::removeObjectFromSelection(
	GameObject* object
)
{
	if (!object)
		return;

	m_selectedObjects.erase(
		std::remove(
			m_selectedObjects.begin(),
			m_selectedObjects.end(),
			object
		),
		m_selectedObjects.end()
	);

	if (m_selectedObject == object)
	{
		if (m_selectedObjects.empty())
		{
			m_selectedObject = nullptr;
		}
		else
		{
			m_selectedObject =
				m_selectedObjects.back();
		}
	}
}

void dx3d::Game::clearSelection() noexcept
{
	m_selectedObjects.clear();
	m_selectedObject = nullptr;
}

const dx3d::MeshData*
dx3d::Game::getObjectMeshData(
	GameObject* object
) const noexcept
{
	if (!object)
		return nullptr;

	// Check custom combined meshes first.
	if (auto* combinedComponent =
		object->getComponent<CombinedMeshComponent>())
	{
		if (combinedComponent->hasMeshData())
		{
			return &combinedComponent->getMeshData();
		}

		return nullptr;
	}

	if (object->getComponent<CubeComponent>())
	{
		return &getCubeMeshData();
	}

	if (object->getComponent<PlaneComponent>())
	{
		return &getPlaneMeshData();
	}

	return nullptr;
}

bool dx3d::Game::canMergeSelectedObjects() const noexcept
{
	if (m_selectedObjects.size() < 2)
		return false;

	for (auto* object : m_selectedObjects)
	{
		if (!object)
			return false;

		// The editor camera and unsupported object types
		// cannot participate in a mesh merge.
		if (object->getComponent<CameraComponent>())
			return false;

		if (!getObjectMeshData(object))
			return false;
	}

	return true;
}

void dx3d::Game::mergeSelectedObjects()
{
	if (!canMergeSelectedObjects())
		return;

	std::vector<MeshMergeSource> sources{};
	sources.reserve(m_selectedObjects.size());

	for (auto* object : m_selectedObjects)
	{
		const MeshData* meshData =
			getObjectMeshData(object);

		if (!meshData)
			return;

		sources.push_back(
			{
				meshData,
				object->getTransform()
					.getAffineWorldMatrix()
			}
		);
	}

	MeshData combinedMesh =
		mergeMeshes(sources);

	if (combinedMesh.empty())
		return;

	// Calculate a bounding-box center for the new pivot.
	Vec3 minimum =
		combinedMesh.vertices.front().position;

	Vec3 maximum =
		combinedMesh.vertices.front().position;

	for (const auto& vertex : combinedMesh.vertices)
	{
		const Vec3& position =
			vertex.position;

		minimum.x =
			std::min(minimum.x, position.x);

		minimum.y =
			std::min(minimum.y, position.y);

		minimum.z =
			std::min(minimum.z, position.z);

		maximum.x =
			std::max(maximum.x, position.x);

		maximum.y =
			std::max(maximum.y, position.y);

		maximum.z =
			std::max(maximum.z, position.z);
	}

	const Vec3 mergedPivot
	{
		(minimum.x + maximum.x) * 0.5f,
		(minimum.y + maximum.y) * 0.5f,
		(minimum.z + maximum.z) * 0.5f
	};

	// The vertices are currently in world space.
	// Move them relative to the new object's centered pivot.
	for (auto& vertex : combinedMesh.vertices)
	{
		vertex.position.x -= mergedPivot.x;
		vertex.position.y -= mergedPivot.y;
		vertex.position.z -= mergedPivot.z;
	}

	// Keep a copy because selection will be replaced after
	// creating the merged object.
	const std::vector<GameObject*> originalObjects =
		m_selectedObjects;

	auto* mergedObject =
		m_world->createGameObject<GameObject>();

	mergedObject->setName(
		"Merged Object"
	);

	auto* combinedComponent =
		mergedObject->createOrGetComponent<
		CombinedMeshComponent>();

	combinedComponent->setMeshData(
		combinedMesh
	);

	mergedObject->getTransform().setPosition(
		mergedPivot
	);

	mergedObject->getTransform().setRotation(
		{ 0.0f, 0.0f, 0.0f }
	);

	mergedObject->getTransform().setScale(
		{ 1.0f, 1.0f, 1.0f }
	);

	// Queue the original objects for removal only after
	// the new merged object was created successfully.
	for (auto* originalObject : originalObjects)
	{
		m_world->destroyGameObject(
			originalObject
		);
	}

	// Remove every old pointer from editor selection and
	// make the new combined object active.
	selectOnly(mergedObject);
}

bool dx3d::Game::canCopySelectedObject() const noexcept
{
	if (!m_selectedObject)
		return false;

	// Do not allow the editor camera to be copied.
	if (m_selectedObject->getComponent<
		CameraComponent>())
	{
		return false;
	}

	if (auto* combinedComponent =
		m_selectedObject->getComponent<
		CombinedMeshComponent>())
	{
		return combinedComponent->hasMeshData();
	}

	if (m_selectedObject->getComponent<
		CubeComponent>())
	{
		return true;
	}

	if (m_selectedObject->getComponent<
		PlaneComponent>())
	{
		return true;
	}

	return false;
}

void dx3d::Game::copySelectedObject()
{
	if (!canCopySelectedObject())
		return;

	ObjectCopyData copiedData{};

	copiedData.isValid = true;
	copiedData.sourceName =
		m_selectedObject->getName();

	auto& transform =
		m_selectedObject->getTransform();

	copiedData.position =
		transform.getPosition();

	copiedData.rotation =
		transform.getRotation();

	copiedData.scale =
		transform.getScale();

	// Combined meshes must copy their complete custom
	// vertex and index data.
	if (auto* combinedComponent =
		m_selectedObject->getComponent<
		CombinedMeshComponent>())
	{
		copiedData.type =
			CopiedObjectType::CombinedMesh;

		copiedData.meshData =
			combinedComponent->getMeshData();
	}
	else if (
		m_selectedObject->getComponent<
		CubeComponent>())
	{
		copiedData.type =
			CopiedObjectType::Cube;
	}
	else if (
		m_selectedObject->getComponent<
		PlaneComponent>())
	{
		copiedData.type =
			CopiedObjectType::Plane;
	}
	else
	{
		return;
	}

	// A new copy operation restarts the pasted-object count.
	copiedData.pasteCount = 0;

	m_objectClipboard =
		copiedData;
}

void dx3d::Game::pasteCopiedObject()
{
	if (!m_objectClipboard.isValid)
		return;

	auto* pastedObject =
		m_world->createGameObject<GameObject>();

	switch (m_objectClipboard.type)
	{
	case CopiedObjectType::Cube:
		pastedObject->createOrGetComponent<
			CubeComponent>();
		break;

	case CopiedObjectType::Plane:
		pastedObject->createOrGetComponent<
			PlaneComponent>();
		break;

	case CopiedObjectType::CombinedMesh:
	{
		auto* combinedComponent =
			pastedObject->createOrGetComponent<
			CombinedMeshComponent>();

		combinedComponent->setMeshData(
			m_objectClipboard.meshData
		);

		break;
	}

	case CopiedObjectType::None:
	default:
		m_world->destroyGameObject(
			pastedObject
		);
		return;
	}

	++m_objectClipboard.pasteCount;

	std::string pastedName =
		m_objectClipboard.sourceName +
		" Copy";

	if (m_objectClipboard.pasteCount > 1)
	{
		pastedName +=
			" (" +
			std::to_string(
				m_objectClipboard.pasteCount
			) +
			")";
	}

	pastedObject->setName(
		pastedName
	);

	// Preserve the exact transform of the copied object.
	auto& pastedTransform =
		pastedObject->getTransform();

	pastedTransform.setPosition(
		m_objectClipboard.position
	);

	pastedTransform.setRotation(
		m_objectClipboard.rotation
	);

	pastedTransform.setScale(
		m_objectClipboard.scale
	);

	// Automatically select the newly pasted object.
	selectOnly(
		pastedObject
	);
}

void dx3d::Game::handleViewportPicking(
	CameraComponent* camera,
	const TransformGizmo::ViewportArea& viewportArea
)
{
	if (!camera)
		return;

	const bool leftMousePressed =
		m_inputSystem->isKeyPressed(
			KeyCode::MouseLeft
		);

	if (!leftMousePressed)
		return;

	const bool rightMouseDown =
		m_inputSystem->isKeyDown(
			KeyCode::MouseRight
		);

	if (rightMouseDown)
		return;

	// A gizmo click must not trigger object picking.
	if (m_transformGizmo.isUsing() ||
		m_transformGizmo.getHoveredAxis() !=
		TransformGizmo::Axis::None)
	{
		return;
	}

	// Do not pick through editor windows or menus.
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	if (ImGui::IsPopupOpen(
		nullptr,
		ImGuiPopupFlags_AnyPopupId
	))
	{
		return;
	}

	const Vec2 mousePosition =
		m_inputSystem->getMousePosition();

	const PickingViewportArea pickingViewport
	{
		viewportArea.x,
		viewportArea.y,
		viewportArea.width,
		viewportArea.height
	};

	// Finds the closest hit from one ray.
	// planeObjectsOnly determines whether this pass searches
	// planes or all other mesh objects.
	auto findClosestHit =
		[&](
			const PickingRay& ray,
			bool planeObjectsOnly,
			GameObject*& outputObject,
			f32& outputDistance
			)
		{
			outputObject = nullptr;

			outputDistance =
				std::numeric_limits<f32>::max();

			const auto objects =
				m_world->getGameObjects();

			for (auto* object : objects)
			{
				if (!object)
					continue;

				if (object->getComponent<
					CameraComponent>())
				{
					continue;
				}

				const bool isPlaneObject =
					object->getComponent<
					PlaneComponent>() != nullptr;

				if (planeObjectsOnly !=
					isPlaneObject)
				{
					continue;
				}

				const MeshData* meshData =
					getObjectMeshData(object);

				if (!meshData)
					continue;

				f32 hitDistance = 0.0f;

				const bool wasHit =
					intersectRayWithMesh(
						ray,
						*meshData,
						object->getTransform()
						.getAffineWorldMatrix(),
						hitDistance
					);

				if (!wasHit)
					continue;

				if (hitDistance <
					outputDistance)
				{
					outputDistance =
						hitDistance;

					outputObject =
						object;
				}
			}
		};

	struct MouseOffset
	{
		f32 x{};
		f32 y{};
	};

	// The first sample is the exact cursor position.
	// The other samples provide a small selection tolerance.
	constexpr f32 pickingRadius = 6.0f;
	constexpr f32 diagonalRadius = 4.25f;

	const MouseOffset mouseOffsets[]
	{
		{ 0.0f, 0.0f },

		{ -pickingRadius, 0.0f },
		{ pickingRadius, 0.0f },
		{ 0.0f, -pickingRadius },
		{ 0.0f, pickingRadius },

		{ -diagonalRadius, -diagonalRadius },
		{ diagonalRadius, -diagonalRadius },
		{ -diagonalRadius, diagonalRadius },
		{ diagonalRadius, diagonalRadius }
	};

	GameObject* bestMeshObject = nullptr;

	f32 bestOffsetDistanceSquared =
		std::numeric_limits<f32>::max();

	f32 bestMeshDistance =
		std::numeric_limits<f32>::max();

	// Planes are only selected using the exact center ray.
	GameObject* centerPlaneObject = nullptr;

	f32 centerPlaneDistance =
		std::numeric_limits<f32>::max();

	for (size_t offsetIndex = 0;
		offsetIndex < std::size(mouseOffsets);
		++offsetIndex)
	{
		const MouseOffset& offset =
			mouseOffsets[offsetIndex];

		PickingRay pickingRay{};

		if (!createPickingRay(
			mousePosition.x + offset.x,
			mousePosition.y + offset.y,
			pickingViewport,
			*camera,
			pickingRay
		))
		{
			continue;
		}

		GameObject* meshObject = nullptr;
		GameObject* planeObject = nullptr;

		f32 meshDistance =
			std::numeric_limits<f32>::max();

		f32 planeDistance =
			std::numeric_limits<f32>::max();

		// Search cubes, merged objects, and other
		// non-plane mesh objects.
		findClosestHit(
			pickingRay,
			false,
			meshObject,
			meshDistance
		);

		// Search normal PlaneComponent objects separately.
		findClosestHit(
			pickingRay,
			true,
			planeObject,
			planeDistance
		);

		// Remember the plane directly beneath the cursor.
		// Nearby offset rays should not make the plane
		// selection itself oversized.
		if (offsetIndex == 0)
		{
			centerPlaneObject =
				planeObject;

			centerPlaneDistance =
				planeDistance;
		}

		if (!meshObject)
			continue;

		// Do not select a mesh that is hidden behind a plane
		// along the same sample ray.
		const bool meshIsVisible =
			!planeObject ||
			meshDistance <= planeDistance + 0.0001f;

		if (!meshIsVisible)
			continue;

		const f32 offsetDistanceSquared =
			offset.x * offset.x +
			offset.y * offset.y;

		const bool hasBetterMouseOffset =
			offsetDistanceSquared <
			bestOffsetDistanceSquared;

		const bool sameMouseOffset =
			std::fabs(
				offsetDistanceSquared -
				bestOffsetDistanceSquared
			) <= 0.001f;

		const bool hasBetterDepth =
			meshDistance <
			bestMeshDistance;

		if (hasBetterMouseOffset ||
			(sameMouseOffset && hasBetterDepth))
		{
			bestMeshObject =
				meshObject;

			bestOffsetDistanceSquared =
				offsetDistanceSquared;

			bestMeshDistance =
				meshDistance;
		}
	}

	GameObject* closestObject = nullptr;

	if (bestMeshObject)
	{
		// Prefer a nearby visible cube or merged mesh.
		closestObject =
			bestMeshObject;
	}
	else if (centerPlaneObject)
	{
		// The plane remains the fallback when no smaller
		// mesh was found near the cursor.
		closestObject =
			centerPlaneObject;
	}

	const bool controlHeld =
		ImGui::GetIO().KeyCtrl;

	if (closestObject)
	{
		if (controlHeld)
		{
			toggleObjectSelection(
				closestObject
			);
		}
		else
		{
			selectOnly(
				closestObject
			);
		}

		return;
	}

	if (!controlHeld)
	{
		clearSelection();
	}
}

bool dx3d::Game::loadCreditsLogo()
{
	struct ComScope
	{
		bool shouldUninitialize{ false };

		~ComScope()
		{
			if (shouldUninitialize)
			{
				CoUninitialize();
			}
		}
	};

	ComScope comScope{};

	const HRESULT comResult =
		CoInitializeEx(
			nullptr,
			COINIT_MULTITHREADED
		);

	if (SUCCEEDED(comResult))
	{
		comScope.shouldUninitialize = true;
	}
	else if (comResult != RPC_E_CHANGED_MODE)
	{
		DX3DLogError(
			"Failed to initialize COM for the credits logo."
		);

		return false;
	}

	Microsoft::WRL::ComPtr<
		IWICImagingFactory
	> imagingFactory{};

	HRESULT result =
		CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(
				imagingFactory.GetAddressOf()
			)
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to create the WIC imaging factory."
		);

		return false;
	}

	Microsoft::WRL::ComPtr<
		IWICBitmapDecoder
	> decoder{};

	const wchar_t* logoPaths[]
	{
		L"Assets\\Images\\Dlsu.png",
		L"DX3D\\Assets\\Images\\Dlsu.png"
	};

	for (const wchar_t* logoPath : logoPaths)
	{
		decoder.Reset();

		result =
			imagingFactory->CreateDecoderFromFilename(
				logoPath,
				nullptr,
				GENERIC_READ,
				WICDecodeMetadataCacheOnLoad,
				decoder.GetAddressOf()
			);

		if (SUCCEEDED(result))
		{
			break;
		}
	}

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to find or open the credits logo."
		);

		return false;
	}

	Microsoft::WRL::ComPtr<
		IWICBitmapFrameDecode
	> frame{};

	result =
		decoder->GetFrame(
			0,
			frame.GetAddressOf()
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to read the credits logo frame."
		);

		return false;
	}

	UINT width{};
	UINT height{};

	result =
		frame->GetSize(
			&width,
			&height
		);

	if (FAILED(result) ||
		width == 0 ||
		height == 0)
	{
		DX3DLogError(
			"Failed to read the credits logo size."
		);

		return false;
	}

	Microsoft::WRL::ComPtr<
		IWICFormatConverter
	> converter{};

	result =
		imagingFactory->CreateFormatConverter(
			converter.GetAddressOf()
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to create the logo format converter."
		);

		return false;
	}

	result =
		converter->Initialize(
			frame.Get(),
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to convert the credits logo."
		);

		return false;
	}

	const UINT bytesPerPixel = 4;
	const UINT rowPitch =
		width * bytesPerPixel;

	std::vector<unsigned char> pixels(
		static_cast<size_t>(rowPitch) *
		static_cast<size_t>(height)
	);

	result =
		converter->CopyPixels(
			nullptr,
			rowPitch,
			static_cast<UINT>(
				pixels.size()
				),
			pixels.data()
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to copy the credits logo pixels."
		);

		return false;
	}

	D3D11_TEXTURE2D_DESC textureDescription{};

	textureDescription.Width = width;
	textureDescription.Height = height;
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;

	textureDescription.Format =
		DXGI_FORMAT_R8G8B8A8_UNORM;

	textureDescription.SampleDesc.Count = 1;

	textureDescription.Usage =
		D3D11_USAGE_DEFAULT;

	textureDescription.BindFlags =
		D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA textureData{};

	textureData.pSysMem =
		pixels.data();

	textureData.SysMemPitch =
		rowPitch;

	Microsoft::WRL::ComPtr<
		ID3D11Texture2D
	> texture{};

	ID3D11Device* device =
		m_graphicsDevice->getD3DDevice();

	if (!device)
	{
		DX3DLogError(
			"DirectX device is unavailable while loading the logo."
		);

		return false;
	}

	result =
		device->CreateTexture2D(
			&textureDescription,
			&textureData,
			texture.GetAddressOf()
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to create the credits logo texture."
		);

		return false;
	}

	result =
		device->CreateShaderResourceView(
			texture.Get(),
			nullptr,
			m_creditsLogo.ReleaseAndGetAddressOf()
		);

	if (FAILED(result))
	{
		DX3DLogError(
			"Failed to create the credits logo resource view."
		);

		return false;
	}

	m_creditsLogoWidth =
		static_cast<f32>(width);

	m_creditsLogoHeight =
		static_cast<f32>(height);

	DX3DLogInfo(
		"Credits logo loaded."
	);

	return true;
}

void dx3d::Game::createNewScene()
{
	m_world->stopPhysics();

	SceneSerializer::clear(
		*m_world
	);

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter = 0;
	m_planeCounter = 0;

	m_sceneStatusMessage =
		"New scene created";
}

void dx3d::Game::saveScene()
{
	const bool saved =
		SceneSerializer::save(
			*m_world,
			"Scene.dx3dscene"
		);

	if (saved)
	{
		m_sceneStatusMessage =
			"Saved: Scene.dx3dscene";

		DX3DLogInfo(
			"Scene saved."
		);
	}
	else
	{
		m_sceneStatusMessage =
			"Save failed";

		DX3DLogError(
			"Scene save failed."
		);
	}
}

void dx3d::Game::loadScene()
{
	m_world->stopPhysics();

	const SceneLoadResult result =
		SceneSerializer::load(
			*m_world,
			"Scene.dx3dscene"
		);

	if (!result.success)
	{
		m_sceneStatusMessage =
			"Load failed or file not found";

		DX3DLogError(
			"Scene load failed."
		);

		return;
	}

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter =
		result.cubeCount;

	m_planeCounter =
		result.planeCount;

	m_sceneStatusMessage =
		"Loaded: Scene.dx3dscene";

	DX3DLogInfo(
		"Scene loaded."
	);
}

void dx3d::Game::onInternalUpdate()
{
	auto currentTime = std::chrono::steady_clock::now();

	std::chrono::duration<f32> delta =
		currentTime - m_previousTime;

	m_previousTime = currentTime;

	const auto deltaTime = delta.count();

	m_inputSystem->update();

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

	onUpdate(deltaTime);

	if (!m_isRunning)
		return;

	m_world->update(deltaTime);

	// Render the 3D scene first.
	m_worldRenderer->render(
		*m_world,
		m_display->getSwapChain(),
		deltaTime
	);

	// --------------------
// Main editor menu
// --------------------

	if (ImGui::BeginMainMenuBar())
	{
		const bool physicsIsPlaying =
			m_world->isPhysicsEnabled();

		const bool physicsHasStarted =
			m_world->hasPhysicsStarted();

		ImGui::BeginDisabled(
			physicsIsPlaying
		);

		if (ImGui::Button("Play"))
		{
			m_world->setPhysicsEnabled(
				true
			);
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		ImGui::BeginDisabled(
			!physicsIsPlaying
		);

		if (ImGui::Button("Pause"))
		{
			m_world->setPhysicsEnabled(
				false
			);
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		ImGui::BeginDisabled(
			!physicsHasStarted
		);

		if (ImGui::Button("Stop"))
		{
			m_world->stopPhysics();
		}

		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Separator();
		ImGui::SameLine();

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem(
				"New Scene"
			))
			{
				createNewScene();
			}

			if (ImGui::MenuItem(
				"Save Scene"
			))
			{
				saveScene();
			}

			if (ImGui::MenuItem(
				"Load Scene"
			))
			{
				loadScene();
			}

			ImGui::Separator();

			ImGui::TextDisabled(
				"%s",
				m_sceneStatusMessage.c_str()
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

			ImGui::Separator();

			static int physicsCubeSpawnCount = 20;

			const bool physicsSimulationStarted =
				m_world->hasPhysicsStarted();

			ImGui::BeginDisabled(
				physicsSimulationStarted
			);

			ImGui::SetNextItemWidth(120.0f);

			if (ImGui::InputInt(
				"Physics Cube Count",
				&physicsCubeSpawnCount
			))
			{
				physicsCubeSpawnCount =
					std::clamp(
						physicsCubeSpawnCount,
						1,
						100
					);
			}

			if (ImGui::MenuItem("Spawn Physics Cubes"))
			{
				physicsCubeSpawnCount =
					std::clamp(
						physicsCubeSpawnCount,
						1,
						100
					);

				GameObject* lastCreatedCube = nullptr;

				for (int cubeIndex = 0;
					cubeIndex < physicsCubeSpawnCount;
					++cubeIndex)
				{
					++m_cubeCounter;

					auto* cube =
						m_world->createGameObject<
						GameObject>();

					cube->setName(
						"Physics Cube (" +
						std::to_string(
							m_cubeCounter
						) +
						")"
					);

					cube->createOrGetComponent<
						CubeComponent>();

					auto* rigidBody =
						cube->createOrGetComponent<
						RigidBodyComponent>();

					const int clusterColumns = 3;
					const int clusterRows = 3;

					const int column =
						cubeIndex % clusterColumns;

					const int row =
						(cubeIndex / clusterColumns) % clusterRows;

					const int layer =
						cubeIndex / (clusterColumns * clusterRows);

					const f32 spacingX = 0.22f;
					const f32 spacingZ = 0.22f;
					const f32 layerHeight = 0.22f;

					const f32 spawnX =
						(
							static_cast<f32>(column) -
							static_cast<f32>(clusterColumns - 1) * 0.5f
							) * spacingX;

					const f32 spawnZ =
						(
							static_cast<f32>(row) -
							static_cast<f32>(clusterRows - 1) * 0.5f
							) * spacingZ;

					const f32 spawnY =
						5.8f +
						static_cast<f32>(layer) * layerHeight;

					const f32 horizontalVelocityX =
						(
							static_cast<f32>(column) -
							static_cast<f32>(clusterColumns - 1) * 0.5f
							) * 2.0f;

					const f32 horizontalVelocityZ =
						(
							static_cast<f32>(row) -
							static_cast<f32>(clusterRows - 1) * 0.5f
							) * 2.0f;

					rigidBody->setVelocity(
						{
							horizontalVelocityX,
							0.0f,
							horizontalVelocityZ
						}
					);

					rigidBody->setAngularVelocity(
						{
							0.8f +
								static_cast<f32>(cubeIndex % 4) * 0.2f,
							0.7f +
								static_cast<f32>(cubeIndex % 3) * 0.2f,
							0.6f +
								static_cast<f32>(cubeIndex % 5) * 0.15f
						}
					);

					rigidBody->setRestitution(
						0.65f
					);

					rigidBody->setFriction(
						0.05f
					);

					cube->getTransform().setPosition(
						{
							spawnX,
							spawnY,
							spawnZ
						}
					);

					cube->getTransform().setRotation(
						{
							0.15f * static_cast<f32>(cubeIndex % 5),
							0.22f * static_cast<f32>((cubeIndex + 2) % 6),
							0.18f * static_cast<f32>((cubeIndex + 1) % 4)
						}
					);

					cube->getTransform().setScale(
						{
							1.0f,
							1.0f,
							1.0f
						}
					);

					lastCreatedCube = cube;
				}

				if (lastCreatedCube)
				{
					selectOnly(
						lastCreatedCube
					);
				}
			}

			ImGui::EndDisabled();

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

		if (ImGui::BeginMenu("Tools"))
		{
			if (ImGui::MenuItem("Color Picker"))
			{
				m_showColorPickerWindow = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("About"))
		{
			if (ImGui::MenuItem("Credits"))
			{
				m_showCreditsWindow = true;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (m_showCreditsWindow)
	{
		ImGui::SetNextWindowSize(
			ImVec2(610.0f, 400.0f),
			ImGuiCond_FirstUseEver
		);

		if (ImGui::Begin(
			"Credits",
			&m_showCreditsWindow
		))
		{
			ImGui::TextUnformatted("About");
			ImGui::Spacing();

			if (m_creditsLogo &&
				m_creditsLogoWidth > 0.0f &&
				m_creditsLogoHeight > 0.0f)
			{
				constexpr f32 logoWidth = 180.0f;

				const f32 logoHeight =
					logoWidth *
					(m_creditsLogoHeight /
						m_creditsLogoWidth);

				const f32 availableWidth =
					ImGui::GetContentRegionAvail().x;

				const f32 logoPositionX =
					ImGui::GetCursorPosX() +
					std::max(
						0.0f,
						(availableWidth - logoWidth) *
						0.5f
					);

				ImGui::SetCursorPosX(
					logoPositionX
				);

				ImGui::Image(
					(ImTextureID)m_creditsLogo.Get(),
					ImVec2(
						logoWidth,
						logoHeight
					)
				);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
			}

			ImGui::TextUnformatted(
				"Direct X 11 Game Engine v1.0"
			);

			ImGui::TextUnformatted(
				"Developed by: Jm Sy-wico"
			);

			ImGui::TextUnformatted(
				"July 14, 2026"
			);

			ImGui::TextUnformatted(
				"GDENG03 X21"
			);
		}

		ImGui::End();
	}

	if (m_showColorPickerWindow)
	{
		ImGui::SetNextWindowSize(
			ImVec2(360.0f, 430.0f),
			ImGuiCond_FirstUseEver
		);

		if (ImGui::Begin(
			"Color Picker",
			&m_showColorPickerWindow
		))
		{
			static float placeholderColor[4]
			{
				1.0f,
				0.0f,
				0.0f,
				1.0f
			};

			ImGui::TextUnformatted(
				"Color Picker Placeholder"
			);

			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled();

			ImGui::ColorPicker4(
				"##PlaceholderColorPicker",
				placeholderColor,
				ImGuiColorEditFlags_AlphaBar |
				ImGuiColorEditFlags_DisplayRGB
			);

			ImGui::Spacing();

			ImGui::InputFloat4(
				"RGBA",
				placeholderColor,
				"%.2f"
			);

			ImGui::EndDisabled();
		}

		ImGui::End();
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
				directionalLight->
				getShadowArea();

			if (ImGui::DragFloat(
				"Shadow Area",
				&shadowArea,
				0.5f,
				1.0f,
				200.0f
			))
			{
				directionalLight->
					setShadowArea(
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
}
