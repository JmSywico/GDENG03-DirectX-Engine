#include <DX3D/Game/Game.h>
#include <DX3D/Window/Window.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Graphics/MeshMerger.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Graphics/ObjLoader.h>

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
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/RigidBodyComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/ModelComponent.h>

#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iterator>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <Windows.h>
#include <commdlg.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "comdlg32.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>


namespace
{
	constexpr dx3d::f32 DebugMinimumHalfExtent = 0.001f;
	constexpr dx3d::f32 DebugPlaneHalfThickness = 0.05f;
	constexpr dx3d::f32 SnapshotFloatEpsilon = 0.0001f;
	constexpr size_t MaximumUndoHistory = 64;
	constexpr dx3d::f32 EditorSplitterThickness = 6.0f;
	constexpr dx3d::f32 MinimumRightPanelWidth = 220.0f;
	constexpr dx3d::f32 MinimumViewportWidth = 320.0f;
	constexpr dx3d::f32 MinimumOutlinerHeight = 120.0f;
	constexpr dx3d::f32 MinimumInspectorHeight = 180.0f;
	constexpr dx3d::f32 MinimumProjectPanelHeight = 140.0f;
	constexpr dx3d::f32 MinimumViewportHeight = 220.0f;

	struct DebugScreenPoint
	{
		dx3d::f32 x{};
		dx3d::f32 y{};
	};

	bool isPointInsideViewport(
		const dx3d::Vec2& point,
		const dx3d::TransformGizmo::ViewportArea& viewportArea
	) noexcept
	{
		return
			viewportArea.width > 0.0f &&
			viewportArea.height > 0.0f &&
			point.x >= viewportArea.x &&
			point.x <= viewportArea.x +
			viewportArea.width &&
			point.y >= viewportArea.y &&
			point.y <= viewportArea.y +
			viewportArea.height;
	}

	bool projectDebugPoint(
		const dx3d::Vec3& worldPosition,
		const dx3d::Mat4x4& viewProjectionMatrix,
		const dx3d::TransformGizmo::ViewportArea& viewportArea,
		DebugScreenPoint& screenPosition
	) noexcept
	{
		const dx3d::Vec4 worldPoint
		{
			worldPosition.x,
			worldPosition.y,
			worldPosition.z,
			1.0f
		};

		const dx3d::Vec4 clipPosition =
			viewProjectionMatrix.transform(
				worldPoint
			);

		if (clipPosition.w <= 0.001f)
			return false;

		const dx3d::f32 inverseW =
			1.0f / clipPosition.w;

		const dx3d::f32 normalizedX =
			clipPosition.x * inverseW;

		const dx3d::f32 normalizedY =
			clipPosition.y * inverseW;

		const dx3d::f32 normalizedZ =
			clipPosition.z * inverseW;

		if (normalizedZ < 0.0f ||
			normalizedZ > 1.0f)
		{
			return false;
		}

		screenPosition.x =
			viewportArea.x +
			(normalizedX * 0.5f + 0.5f) *
			viewportArea.width;

		screenPosition.y =
			viewportArea.y +
			(-normalizedY * 0.5f + 0.5f) *
			viewportArea.height;

		return true;
	}

	dx3d::Vec3 transformDebugPoint(
		const dx3d::Mat4x4& matrix,
		const dx3d::Vec3& point
	) noexcept
	{
		const dx3d::Vec4 transformed =
			matrix.transform(
				{
					point.x,
					point.y,
					point.z,
					1.0f
				}
			);

		return
		{
			transformed.x,
			transformed.y,
			transformed.z
		};
	}

	void drawProjectedBox(
		ImDrawList& drawList,
		const dx3d::Mat4x4& rigidWorldMatrix,
		const dx3d::Vec3& localCenter,
		const dx3d::Vec3& halfExtents,
		const dx3d::Mat4x4& viewProjectionMatrix,
		const dx3d::TransformGizmo::ViewportArea& viewportArea,
		ImU32 color,
		float thickness
	)
	{
		const dx3d::Vec3 localCorners[8]
		{
			{
				localCenter.x - halfExtents.x,
				localCenter.y - halfExtents.y,
				localCenter.z - halfExtents.z
			},
			{
				localCenter.x + halfExtents.x,
				localCenter.y - halfExtents.y,
				localCenter.z - halfExtents.z
			},
			{
				localCenter.x + halfExtents.x,
				localCenter.y + halfExtents.y,
				localCenter.z - halfExtents.z
			},
			{
				localCenter.x - halfExtents.x,
				localCenter.y + halfExtents.y,
				localCenter.z - halfExtents.z
			},
			{
				localCenter.x - halfExtents.x,
				localCenter.y - halfExtents.y,
				localCenter.z + halfExtents.z
			},
			{
				localCenter.x + halfExtents.x,
				localCenter.y - halfExtents.y,
				localCenter.z + halfExtents.z
			},
			{
				localCenter.x + halfExtents.x,
				localCenter.y + halfExtents.y,
				localCenter.z + halfExtents.z
			},
			{
				localCenter.x - halfExtents.x,
				localCenter.y + halfExtents.y,
				localCenter.z + halfExtents.z
			}
		};

		DebugScreenPoint screenCorners[8]{};
		bool visibleCorners[8]{};

		for (dx3d::ui32 index = 0;
			index < 8;
			++index)
		{
			const dx3d::Vec3 worldCorner =
				transformDebugPoint(
					rigidWorldMatrix,
					localCorners[index]
				);

			visibleCorners[index] =
				projectDebugPoint(
					worldCorner,
					viewProjectionMatrix,
					viewportArea,
					screenCorners[index]
				);
		}

		const dx3d::ui32 edges[12][2]
		{
			{ 0, 1 },
			{ 1, 2 },
			{ 2, 3 },
			{ 3, 0 },
			{ 4, 5 },
			{ 5, 6 },
			{ 6, 7 },
			{ 7, 4 },
			{ 0, 4 },
			{ 1, 5 },
			{ 2, 6 },
			{ 3, 7 }
		};

		for (const auto& edge : edges)
		{
			const dx3d::ui32 startIndex =
				edge[0];

			const dx3d::ui32 endIndex =
				edge[1];

			if (!visibleCorners[startIndex] ||
				!visibleCorners[endIndex])
			{
				continue;
			}

			drawList.AddLine(
				{
					screenCorners[startIndex].x,
					screenCorners[startIndex].y
				},
				{
					screenCorners[endIndex].x,
					screenCorners[endIndex].y
				},
				IM_COL32(0, 0, 0, 180),
				thickness + 2.0f
			);

			drawList.AddLine(
				{
					screenCorners[startIndex].x,
					screenCorners[startIndex].y
				},
				{
					screenCorners[endIndex].x,
					screenCorners[endIndex].y
				},
				color,
				thickness
			);
		}
	}

	bool areFloatsEqual(
		dx3d::f32 lhs,
		dx3d::f32 rhs
	) noexcept
	{
		return std::fabs(lhs - rhs) <=
			SnapshotFloatEpsilon;
	}

	bool areVec2Equal(
		const dx3d::Vec2& lhs,
		const dx3d::Vec2& rhs
	) noexcept
	{
		return
			areFloatsEqual(lhs.x, rhs.x) &&
			areFloatsEqual(lhs.y, rhs.y);
	}

	bool areVec3Equal(
		const dx3d::Vec3& lhs,
		const dx3d::Vec3& rhs
	) noexcept
	{
		return
			areFloatsEqual(lhs.x, rhs.x) &&
			areFloatsEqual(lhs.y, rhs.y) &&
			areFloatsEqual(lhs.z, rhs.z);
	}

	bool areVec4Equal(
		const dx3d::Vec4& lhs,
		const dx3d::Vec4& rhs
	) noexcept
	{
		return
			areFloatsEqual(lhs.x, rhs.x) &&
			areFloatsEqual(lhs.y, rhs.y) &&
			areFloatsEqual(lhs.z, rhs.z) &&
			areFloatsEqual(lhs.w, rhs.w);
	}

	bool areMeshDataEqual(
		const dx3d::MeshData& lhs,
		const dx3d::MeshData& rhs
	) noexcept
	{
		if (lhs.indices != rhs.indices ||
			lhs.vertices.size() !=
			rhs.vertices.size())
		{
			return false;
		}

		for (size_t index = 0;
			index < lhs.vertices.size();
			++index)
		{
			const auto& lhsVertex =
				lhs.vertices[index];

			const auto& rhsVertex =
				rhs.vertices[index];

			if (
				!areVec3Equal(
					lhsVertex.position,
					rhsVertex.position
				) ||
				!areVec4Equal(
					lhsVertex.color,
					rhsVertex.color
				) ||
				!areVec3Equal(
					lhsVertex.normal,
					rhsVertex.normal
				) ||
				!areVec2Equal(
					lhsVertex.texCoord,
					rhsVertex.texCoord
				)
				)
			{
				return false;
			}
		}

		return true;
	}

	std::string toLowerAscii(
		std::string value
	)
	{
		for (auto& character : value)
		{
			character =
				static_cast<char>(
					std::tolower(
						static_cast<unsigned char>(
							character
						)
					)
				);
		}

		return value;
	}

	bool containsCaseInsensitive(
		const std::string& text,
		const char* filter
	)
	{
		if (!filter || filter[0] == '\0')
			return true;

		return toLowerAscii(text).find(
			toLowerAscii(filter)
		) != std::string::npos;
	}

	std::string truncateText(
		const std::string& text,
		size_t maximumLength
	)
	{
		if (text.size() <= maximumLength)
			return text;

		if (maximumLength <= 3)
			return text.substr(0, maximumLength);

		return text.substr(
			0,
			maximumLength - 3
		) + "...";
	}

	std::string formatFileSize(
		size_t bytes
	)
	{
		constexpr double kiloBytes = 1024.0;
		constexpr double megaBytes =
			kiloBytes * 1024.0;

		char buffer[64]{};

		if (bytes >=
			static_cast<size_t>(megaBytes))
		{
			std::snprintf(
				buffer,
				sizeof(buffer),
				"%.2f MB",
				static_cast<double>(bytes) /
				megaBytes
			);
		}
		else if (bytes >=
			static_cast<size_t>(kiloBytes))
		{
			std::snprintf(
				buffer,
				sizeof(buffer),
				"%.1f KB",
				static_cast<double>(bytes) /
				kiloBytes
			);
		}
		else
		{
			std::snprintf(
				buffer,
				sizeof(buffer),
				"%zu B",
				bytes
			);
		}

		return buffer;
	}

	std::string wideToUtf8(
		const std::wstring& text
	)
	{
		if (text.empty())
			return {};

		const int length =
			WideCharToMultiByte(
				CP_UTF8,
				0,
				text.c_str(),
				static_cast<int>(text.size()),
				nullptr,
				0,
				nullptr,
				nullptr
			);

		if (length <= 0)
			return {};

		std::string result(
			static_cast<size_t>(length),
			'\0'
		);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			result.data(),
			length,
			nullptr,
			nullptr
		);

		return result;
	}

	std::string openLevelFileDialog(
		HWND ownerWindow
	)
	{
		std::array<wchar_t, 4096> filePath{};

		OPENFILENAMEW openFileName{};
		openFileName.lStructSize =
			sizeof(openFileName);
		openFileName.hwndOwner =
			ownerWindow;
		openFileName.lpstrFilter =
			L"DX3D Level Files (*.level)\0*.level\0"
			L"JSON Files (*.json)\0*.json\0"
			L"All Files (*.*)\0*.*\0";
		openFileName.lpstrFile =
			filePath.data();
		openFileName.nMaxFile =
			static_cast<DWORD>(
				filePath.size()
			);
		openFileName.lpstrTitle =
			L"Import DX3D .level";
		openFileName.lpstrDefExt =
			L"level";
		openFileName.Flags =
			OFN_FILEMUSTEXIST |
			OFN_PATHMUSTEXIST |
			OFN_NOCHANGEDIR |
			OFN_EXPLORER;

		if (!GetOpenFileNameW(
			&openFileName
		))
		{
			return {};
		}

		return wideToUtf8(
			filePath.data()
		);
	}
}


dx3d::Game::Game(const GameDesc& desc)
{
	m_logger = std::make_unique<Logger>(desc.logLevel);

	DX3DLogInfo("GDENG03 | DirectX Game Engine");
	DX3DLogInfo("--------------------------------------");

	m_inputSystem = std::make_unique<InputSystem>(InputSystemDesc{ *m_logger });
	m_graphicsDevice = std::make_shared<GraphicsDevice>(GraphicsDeviceDesc{ *m_logger });
	m_display = std::make_unique<Display>(DisplayDesc{ {*m_logger,desc.windowSize},*m_graphicsDevice });
	m_inputSystem->setTargetWindow(m_display->getNativeHandle());
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

	std::error_code assetRootError{};

	if (!std::filesystem::exists(
		m_projectRootPath,
		assetRootError
	))
	{
		if (std::filesystem::exists(
			"Assets",
			assetRootError
		))
		{
			m_projectRootPath = "Assets";
		}
	}

	m_projectCurrentPath =
		m_projectRootPath;

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

	if (object->getComponent<SphereComponent>())
	{
		return &getSphereMeshData();
	}

	if (object->getComponent<CapsuleComponent>())
	{
		return &getCapsuleMeshData();
	}

	if (auto* modelComponent =
		object->getComponent<ModelComponent>())
	{
		if (modelComponent->hasMeshData())
		{
			return &modelComponent->getMeshData();
		}

		return nullptr;
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

	pushUndoSnapshot(
		captureEditorSnapshot()
	);

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

	if (m_selectedObject->getComponent<
		SphereComponent>())
	{
		return true;
	}

	if (m_selectedObject->getComponent<
		CapsuleComponent>())
	{
		return true;
	}

	return false;
}

bool dx3d::Game::canObjectAcceptTexture(
	GameObject* object
) const noexcept
{
	if (!object ||
		object->getComponent<
		CameraComponent>())
	{
		return false;
	}

	if (object->getComponent<
		CubeComponent>())
	{
		return true;
	}

	if (object->getComponent<
		PlaneComponent>())
	{
		return true;
	}

	if (object->getComponent<
		SphereComponent>())
	{
		return true;
	}

	if (object->getComponent<
		CapsuleComponent>())
	{
		return true;
	}

	if (auto* combinedComponent =
		object->getComponent<
		CombinedMeshComponent>())
	{
		return combinedComponent->hasMeshData();
	}

	if (auto* modelComponent =
		object->getComponent<
		ModelComponent>())
	{
		return modelComponent->hasMeshData();
	}

	return false;
}

void dx3d::Game::assignTextureToObject(
	GameObject* object,
	const std::string& texturePath
)
{
	if (!canObjectAcceptTexture(object))
		return;

	auto* material =
		object->createOrGetComponent<
		MaterialComponent>();

	material->setTexturePath(
		texturePath
	);

	material->setUseTexture(
		true
	);
}

void dx3d::Game::processDroppedAssetFiles()
{
	if (!m_display)
		return;

	const auto droppedFiles =
		m_display->consumeDroppedFiles();

	if (droppedFiles.empty())
		return;

	importAssetFiles(
		droppedFiles
	);
}

void dx3d::Game::importAssetFiles(
	const std::vector<std::string>& filePaths
)
{
	namespace fs = std::filesystem;

	if (filePaths.empty())
		return;

	std::error_code error{};

	fs::create_directories(
		m_projectRootPath,
		error
	);

	error.clear();

	const auto rootPath =
		fs::weakly_canonical(
			m_projectRootPath,
			error
		);

	const std::string rootText =
		error
		? toLowerAscii(
			fs::absolute(
				m_projectRootPath
			).generic_string()
		)
		: toLowerAscii(
			rootPath.generic_string()
		);

	auto isAlreadyInProject =
		[&](
			const fs::path& path
			)
		{
			std::error_code pathError{};

			const auto canonicalPath =
				fs::weakly_canonical(
					path,
					pathError
				);

			if (pathError)
				return false;

			std::string fileText =
				toLowerAscii(
					canonicalPath.generic_string()
				);

			std::string normalizedRoot =
				rootText;

			if (!normalizedRoot.empty() &&
				normalizedRoot.back() != '/')
			{
				normalizedRoot.push_back('/');
			}

			return fileText.starts_with(
				normalizedRoot
			);
		};

	auto getImportDirectory =
		[&](
			ProjectAssetKind kind
			)
		{
			const char* folderName = "Imported";

			switch (kind)
			{
			case ProjectAssetKind::Image:
				folderName = "Textures";
				break;

			case ProjectAssetKind::Model:
				folderName = "Models";
				break;

			case ProjectAssetKind::Shader:
				folderName = "Shaders";
				break;

			case ProjectAssetKind::Scene:
			case ProjectAssetKind::Level:
				folderName = "Scenes";
				break;

			case ProjectAssetKind::Prefab:
				folderName = "Prefabs";
				break;

			case ProjectAssetKind::Script:
				folderName = "Scripts";
				break;

			case ProjectAssetKind::Folder:
			case ProjectAssetKind::Other:
				folderName = "Imported";
				break;
			}

			return fs::path(
				m_projectRootPath
			) / folderName;
		};

	auto makeUniqueDestination =
		[](
			const fs::path& directory,
			const fs::path& filename
			)
		{
			fs::path candidate =
				directory / filename;

			if (!fs::exists(candidate))
				return candidate;

			const std::string stem =
				filename.stem().string();

			const std::string extension =
				filename.extension().string();

			for (ui32 suffix = 1;
				suffix < 10000;
				++suffix)
			{
				candidate =
					directory /
					(
						stem +
						" (" +
						std::to_string(suffix) +
						")" +
						extension
						);

				if (!fs::exists(candidate))
					return candidate;
			}

			return directory / filename;
		};

	ui32 importedCount = 0;
	ui32 refreshedCount = 0;
	ui32 skippedCount = 0;
	std::string lastImportDirectory{};

	auto importFile =
		[&](
			const fs::path& sourcePath
			)
		{
			std::error_code fileError{};

			if (!fs::is_regular_file(
				sourcePath,
				fileError
			))
			{
				++skippedCount;
				return;
			}

			const ProjectAssetKind kind =
				getProjectAssetKind(
					sourcePath.generic_string(),
					false
				);

			if (isAlreadyInProject(
				sourcePath
			))
			{
				++refreshedCount;
				lastImportDirectory =
					sourcePath.parent_path().
					generic_string();
				return;
			}

			const fs::path destinationDirectory =
				getImportDirectory(
					kind
				);

			fs::create_directories(
				destinationDirectory,
				fileError
			);

			if (fileError)
			{
				++skippedCount;
				return;
			}

			const fs::path destinationPath =
				makeUniqueDestination(
					destinationDirectory,
					sourcePath.filename()
				);

			fs::copy_file(
				sourcePath,
				destinationPath,
				fs::copy_options::none,
				fileError
			);

			if (fileError)
			{
				++skippedCount;
				return;
			}

			++importedCount;
			lastImportDirectory =
				destinationDirectory.
				generic_string();
		};

	for (const auto& filePath :
		filePaths)
	{
		const fs::path sourcePath{
			filePath
		};

		std::error_code fileError{};

		if (fs::is_directory(
			sourcePath,
			fileError
		))
		{
			for (const auto& entry :
				fs::recursive_directory_iterator(
					sourcePath,
					fileError
				))
			{
				if (fileError)
					break;

				importFile(
					entry.path()
				);
			}

			continue;
		}

		importFile(
			sourcePath
		);
	}

	if (!lastImportDirectory.empty())
	{
		m_projectCurrentPath =
			lastImportDirectory;
	}

	m_projectAssetsDirty = true;
	m_projectThumbnailCache.clear();

	if (importedCount > 0)
	{
		m_sceneStatusMessage =
			"Imported " +
			std::to_string(importedCount) +
			" asset(s).";
	}
	else if (refreshedCount > 0)
	{
		m_sceneStatusMessage =
			"Refreshed " +
			std::to_string(refreshedCount) +
			" project asset(s).";
	}
	else
	{
		m_sceneStatusMessage =
			"No supported assets were imported.";
	}
}

void dx3d::Game::createModelObjectFromAsset(
	const std::string& modelPath
)
{
	const std::string extension =
		toLowerAscii(
			std::filesystem::path(
				modelPath
			).extension().string()
		);

	if (extension != ".obj")
	{
		m_sceneStatusMessage =
			"Only .obj model assets can be instantiated.";
		return;
	}

	MeshData modelMesh{};

	if (!loadObjMesh(
		modelPath,
		modelMesh
	))
	{
		m_sceneStatusMessage =
			"Failed to load model asset.";
		return;
	}

	pushUndoSnapshot(
		captureEditorSnapshot()
	);

	auto* modelObject =
		m_world->createGameObject<GameObject>();

	modelObject->setName(
		std::filesystem::path(
			modelPath
		).stem().string()
	);

	auto* modelComponent =
		modelObject->createOrGetComponent<
		ModelComponent>();

	modelComponent->setMeshData(
		modelMesh
	);

	modelComponent->setModelPath(
		modelPath
	);

	auto& transform =
		modelObject->getTransform();

	transform.setPosition(
		{ 0.0f, 0.0f, 0.0f }
	);

	transform.setRotation(
		{ 0.0f, 0.0f, 0.0f }
	);

	transform.setScale(
		{ 1.0f, 1.0f, 1.0f }
	);

	selectOnly(
		modelObject
	);

	m_sceneStatusMessage =
		"Created model object from asset.";
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
	else if (
		m_selectedObject->getComponent<
		SphereComponent>())
	{
		copiedData.type =
			CopiedObjectType::Sphere;
	}
	else if (
		m_selectedObject->getComponent<
		CapsuleComponent>())
	{
		copiedData.type =
			CopiedObjectType::Capsule;
	}
	else
	{
		return;
	}

	if (auto* rigidBody =
		m_selectedObject->getComponent<
		RigidBodyComponent>())
	{
		copiedData.hasRigidBody = true;
		copiedData.rigidBodyVelocity =
			rigidBody->getVelocity();
		copiedData.rigidBodyAngularVelocity =
			rigidBody->getAngularVelocity();
		copiedData.rigidBodyMass =
			rigidBody->getMass();
		copiedData.rigidBodyRestitution =
			rigidBody->getRestitution();
		copiedData.rigidBodyFriction =
			rigidBody->getFriction();
		copiedData.rigidBodyUseGravity =
			rigidBody->getUseGravity();
		copiedData.rigidBodyIsStatic =
			rigidBody->getStatic();
		copiedData.rigidBodyColliderSize =
			rigidBody->getColliderSize();
		copiedData.rigidBodyColliderOffset =
			rigidBody->getColliderOffset();
		copiedData.
			rigidBodyColliderUsesTransformScale =
			rigidBody->
			getColliderUsesTransformScale();
	}

	if (auto* material =
		m_selectedObject->getComponent<
		MaterialComponent>())
	{
		copiedData.material.isPresent = true;
		copiedData.material.texturePath =
			material->getTexturePath();
		copiedData.material.useTexture =
			material->getUseTexture();
		copiedData.material.uvTiling =
			material->getUvTiling();
		copiedData.material.uvOffset =
			material->getUvOffset();
		copiedData.material.color =
			material->getColor();
		copiedData.material.roughness =
			material->getRoughness();
		copiedData.material.metallic =
			material->getMetallic();
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

	pushUndoSnapshot(
		captureEditorSnapshot()
	);

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

	case CopiedObjectType::Sphere:
		pastedObject->createOrGetComponent<
			SphereComponent>();
		break;

	case CopiedObjectType::Capsule:
		pastedObject->createOrGetComponent<
			CapsuleComponent>();
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

	if (m_objectClipboard.hasRigidBody)
	{
		auto* rigidBody =
			pastedObject->createOrGetComponent<
			RigidBodyComponent>();

		rigidBody->setVelocity(
			m_objectClipboard.
			rigidBodyVelocity
		);

		rigidBody->setAngularVelocity(
			m_objectClipboard.
			rigidBodyAngularVelocity
		);

		rigidBody->setMass(
			m_objectClipboard.
			rigidBodyMass
		);

		rigidBody->setRestitution(
			m_objectClipboard.
			rigidBodyRestitution
		);

		rigidBody->setFriction(
			m_objectClipboard.
			rigidBodyFriction
		);

		rigidBody->setUseGravity(
			m_objectClipboard.
			rigidBodyUseGravity
		);

		rigidBody->setStatic(
			m_objectClipboard.
			rigidBodyIsStatic
		);

		rigidBody->setColliderSize(
			m_objectClipboard.
			rigidBodyColliderSize
		);

		rigidBody->setColliderOffset(
			m_objectClipboard.
			rigidBodyColliderOffset
		);

		rigidBody->
			setColliderUsesTransformScale(
				m_objectClipboard.
				rigidBodyColliderUsesTransformScale
			);
	}

	if (m_objectClipboard.material.isPresent)
	{
		auto* material =
			pastedObject->createOrGetComponent<
			MaterialComponent>();

		material->setTexturePath(
			m_objectClipboard.
			material.texturePath
		);

		material->setUseTexture(
			m_objectClipboard.
			material.useTexture
		);

		material->setUvTiling(
			m_objectClipboard.
			material.uvTiling
		);

		material->setUvOffset(
			m_objectClipboard.
			material.uvOffset
		);

		material->setColor(
			m_objectClipboard.
			material.color
		);

		material->setRoughness(
			m_objectClipboard.
			material.roughness
		);

		material->setMetallic(
			m_objectClipboard.
			material.metallic
		);
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

void dx3d::Game::drawPhysicsDebugOverlay(
	CameraComponent* camera,
	const TransformGizmo::ViewportArea& viewportArea
)
{
	if (!m_showPhysicsDebugOverlay ||
		!camera ||
		viewportArea.width <= 0.0f ||
		viewportArea.height <= 0.0f)
	{
		return;
	}

	const Mat4x4 viewProjectionMatrix =
		camera->getViewMatrix() *
		camera->getProjectionMatrix();

	ImDrawList* drawList =
		ImGui::GetBackgroundDrawList();

	if (!drawList)
		return;

	drawList->PushClipRect(
		{
			viewportArea.x,
			viewportArea.y
		},
		{
			viewportArea.x +
			viewportArea.width,
			viewportArea.y +
			viewportArea.height
		},
		true
	);

	ui32 planeCount = 0;

	PlaneComponent* const* planes =
		m_world->getComponents<
		PlaneComponent
		>(
			planeCount
		);

	for (ui32 index = 0;
		index < planeCount;
		++index)
	{
		auto* plane =
			planes[index];

		if (!plane)
			continue;

		auto& object =
			plane->getGameObject();

		const bool selected =
			isObjectSelected(
				&object
			);

		if (m_showOnlySelectedPhysicsDebug &&
			!selected)
		{
			continue;
		}

		auto& transform =
			object.getTransform();

		const Vec3 scale =
			transform.getScale();

		const Vec3 halfExtents
		{
			std::max(
				std::fabs(scale.x) * 0.5f,
				DebugMinimumHalfExtent
			),
			DebugPlaneHalfThickness,
			std::max(
				std::fabs(scale.z) * 0.5f,
				DebugMinimumHalfExtent
			)
		};

		drawProjectedBox(
			*drawList,
			transform.getRigidWorldMatrix(),
			{
				0.0f,
				-DebugPlaneHalfThickness,
				0.0f
			},
			halfExtents,
			viewProjectionMatrix,
			viewportArea,
			selected
			? IM_COL32(255, 220, 70, 255)
			: IM_COL32(80, 170, 255, 220),
			selected ? 2.5f : 1.25f
		);
	}

	ui32 rigidBodyCount = 0;

	RigidBodyComponent* const* rigidBodies =
		m_world->getComponents<
		RigidBodyComponent
		>(
			rigidBodyCount
		);

	for (ui32 index = 0;
		index < rigidBodyCount;
		++index)
	{
		auto* rigidBody =
			rigidBodies[index];

		if (!rigidBody)
			continue;

		auto& object =
			rigidBody->getGameObject();

		const bool selected =
			isObjectSelected(
				&object
			);

		if (m_showOnlySelectedPhysicsDebug &&
			!selected)
		{
			continue;
		}

		if (!object.getComponent<
			CubeComponent>())
		{
			continue;
		}

		auto& transform =
			object.getTransform();

		const Vec3 colliderSize =
			rigidBody->
			getEffectiveColliderSize();

		const Vec3 halfExtents
		{
			std::max(
				std::fabs(colliderSize.x) * 0.5f,
				DebugMinimumHalfExtent
			),
			std::max(
				std::fabs(colliderSize.y) * 0.5f,
				DebugMinimumHalfExtent
			),
			std::max(
				std::fabs(colliderSize.z) * 0.5f,
				DebugMinimumHalfExtent
			)
		};

		const ImU32 bodyColor =
			selected
			? IM_COL32(255, 220, 70, 255)
			: rigidBody->getStatic()
			? IM_COL32(80, 170, 255, 220)
			: IM_COL32(60, 220, 110, 220);

		drawProjectedBox(
			*drawList,
			transform.getRigidWorldMatrix(),
			rigidBody->getColliderOffset(),
			halfExtents,
			viewProjectionMatrix,
			viewportArea,
			bodyColor,
			selected ? 2.5f : 1.25f
		);
	}

	drawList->PopClipRect();
}

dx3d::Game::EditorSnapshot
dx3d::Game::captureEditorSnapshot() const
{
	EditorSnapshot snapshot{};

	snapshot.cubeCounter =
		m_cubeCounter;

	snapshot.planeCounter =
		m_planeCounter;

	snapshot.sphereCounter =
		m_sphereCounter;

	snapshot.capsuleCounter =
		m_capsuleCounter;

	snapshot.sceneStatusMessage =
		m_sceneStatusMessage;

	for (auto* selectedObject :
		m_selectedObjects)
	{
		if (!selectedObject)
			continue;

		snapshot.selectedNames.
			push_back(
				selectedObject->getName()
			);
	}

	std::sort(
		snapshot.selectedNames.begin(),
		snapshot.selectedNames.end()
	);

	for (auto* object :
		m_world->getGameObjects())
	{
		if (!object)
			continue;

		if (object->getComponent<
			CameraComponent>())
		{
			continue;
		}

		ObjectSnapshot objectSnapshot{};
		bool canSnapshotObject = false;

		if (auto* combinedMesh =
			object->getComponent<
			CombinedMeshComponent>())
		{
			if (!combinedMesh->hasMeshData())
				continue;

			objectSnapshot.type =
				SnapshotObjectType::
				CombinedMesh;

			objectSnapshot.meshData =
				combinedMesh->getMeshData();

			canSnapshotObject = true;
		}
		else if (auto* model =
			object->getComponent<
			ModelComponent>())
		{
			if (!model->hasMeshData())
				continue;

			objectSnapshot.type =
				SnapshotObjectType::Model;

			objectSnapshot.meshData =
				model->getMeshData();

			objectSnapshot.modelPath =
				model->getModelPath();

			objectSnapshot.texturePath =
				model->getTexturePath();

			canSnapshotObject = true;
		}
		else if (object->getComponent<
			CubeComponent>())
		{
			objectSnapshot.type =
				SnapshotObjectType::Cube;

			canSnapshotObject = true;
		}
		else if (object->getComponent<
			PlaneComponent>())
		{
			objectSnapshot.type =
				SnapshotObjectType::Plane;

			canSnapshotObject = true;
		}
		else if (object->getComponent<
			SphereComponent>())
		{
			objectSnapshot.type =
				SnapshotObjectType::Sphere;

			canSnapshotObject = true;
		}
		else if (object->getComponent<
			CapsuleComponent>())
		{
			objectSnapshot.type =
				SnapshotObjectType::Capsule;

			canSnapshotObject = true;
		}
		else if (auto* light =
			object->getComponent<
			DirectionalLightComponent>())
		{
			objectSnapshot.type =
				SnapshotObjectType::
				DirectionalLight;

			objectSnapshot.lightColor =
				light->getColor();

			objectSnapshot.lightIntensity =
				light->getIntensity();

			objectSnapshot.ambientStrength =
				light->getAmbientStrength();

			objectSnapshot.shadowArea =
				light->getShadowArea();

			objectSnapshot.castShadows =
				light->getCastShadows();

			canSnapshotObject = true;
		}

		if (!canSnapshotObject)
			continue;

		objectSnapshot.name =
			object->getName();

		auto& transform =
			object->getTransform();

		objectSnapshot.position =
			transform.getPosition();

		objectSnapshot.rotation =
			transform.getRotation();

		objectSnapshot.scale =
			transform.getScale();

		if (auto* material =
			object->getComponent<
			MaterialComponent>())
		{
			objectSnapshot.material.isPresent = true;
			objectSnapshot.material.texturePath =
				material->getTexturePath();
			objectSnapshot.material.useTexture =
				material->getUseTexture();
			objectSnapshot.material.uvTiling =
				material->getUvTiling();
			objectSnapshot.material.uvOffset =
				material->getUvOffset();
			objectSnapshot.material.color =
				material->getColor();
			objectSnapshot.material.roughness =
				material->getRoughness();
			objectSnapshot.material.metallic =
				material->getMetallic();
		}

		if (auto* rigidBody =
			object->getComponent<
			RigidBodyComponent>())
		{
			auto& rigidBodySnapshot =
				objectSnapshot.rigidBody;

			rigidBodySnapshot.isPresent = true;
			rigidBodySnapshot.velocity =
				rigidBody->getVelocity();
			rigidBodySnapshot.angularVelocity =
				rigidBody->getAngularVelocity();
			rigidBodySnapshot.mass =
				rigidBody->getMass();
			rigidBodySnapshot.restitution =
				rigidBody->getRestitution();
			rigidBodySnapshot.friction =
				rigidBody->getFriction();
			rigidBodySnapshot.useGravity =
				rigidBody->getUseGravity();
			rigidBodySnapshot.isStatic =
				rigidBody->getStatic();
			rigidBodySnapshot.colliderSize =
				rigidBody->getColliderSize();
			rigidBodySnapshot.colliderOffset =
				rigidBody->getColliderOffset();
			rigidBodySnapshot.
				colliderUsesTransformScale =
				rigidBody->
				getColliderUsesTransformScale();
		}

		snapshot.objects.push_back(
			objectSnapshot
		);
	}

	std::sort(
		snapshot.objects.begin(),
		snapshot.objects.end(),
		[](
			const ObjectSnapshot& lhs,
			const ObjectSnapshot& rhs
			)
		{
			if (lhs.name != rhs.name)
				return lhs.name < rhs.name;

			if (lhs.type != rhs.type)
			{
				return static_cast<int>(
					lhs.type
				) <
					static_cast<int>(
						rhs.type
					);
			}

			if (!areVec3Equal(
				lhs.position,
				rhs.position
			))
			{
				if (!areFloatsEqual(
					lhs.position.x,
					rhs.position.x
				))
				{
					return lhs.position.x <
						rhs.position.x;
				}

				if (!areFloatsEqual(
					lhs.position.y,
					rhs.position.y
				))
				{
					return lhs.position.y <
						rhs.position.y;
				}

				return lhs.position.z <
					rhs.position.z;
			}

			return false;
		}
	);

	return snapshot;
}

void dx3d::Game::restoreEditorSnapshot(
	const EditorSnapshot& snapshot
)
{
	m_isRestoringEditorSnapshot = true;
	m_hasPendingUndoSnapshot = false;
	m_skipUndoTrackingThisFrame = true;

	m_world->stopPhysics();

	clearSelection();

	SceneSerializer::clear(
		*m_world
	);

	m_world->flushGameObjectEvents();

	for (const auto& objectSnapshot :
		snapshot.objects)
	{
		auto* object =
			m_world->createGameObject<
			GameObject>();

		if (!object)
			continue;

		object->setName(
			objectSnapshot.name
		);

		switch (objectSnapshot.type)
		{
		case SnapshotObjectType::Cube:
			object->createOrGetComponent<
				CubeComponent>();
			break;

		case SnapshotObjectType::Plane:
			object->createOrGetComponent<
				PlaneComponent>();
			break;

		case SnapshotObjectType::Sphere:
			object->createOrGetComponent<
				SphereComponent>();
			break;

		case SnapshotObjectType::Capsule:
			object->createOrGetComponent<
				CapsuleComponent>();
			break;

		case SnapshotObjectType::CombinedMesh:
	{
			auto* combinedMesh =
				object->createOrGetComponent<
				CombinedMeshComponent>();

			combinedMesh->setMeshData(
				objectSnapshot.meshData
			);

			break;
		}

		case SnapshotObjectType::Model:
		{
			auto* model =
				object->createOrGetComponent<
				ModelComponent>();

			model->setMeshData(
				objectSnapshot.meshData
			);

			model->setModelPath(
				objectSnapshot.modelPath
			);

			model->setTexturePath(
				objectSnapshot.texturePath
			);

			break;
		}

		case SnapshotObjectType::DirectionalLight:
		{
			auto* light =
				object->createOrGetComponent<
				DirectionalLightComponent>();

			light->setColor(
				objectSnapshot.lightColor
			);

			light->setIntensity(
				objectSnapshot.lightIntensity
			);

			light->setAmbientStrength(
				objectSnapshot.
				ambientStrength
			);

			light->setShadowArea(
				objectSnapshot.shadowArea
			);

			light->setCastShadows(
				objectSnapshot.castShadows
			);

			break;
		}
		}

		if (objectSnapshot.
			rigidBody.isPresent)
		{
			auto* rigidBody =
				object->createOrGetComponent<
				RigidBodyComponent>();

			const auto& rigidBodySnapshot =
				objectSnapshot.rigidBody;

			rigidBody->setVelocity(
				rigidBodySnapshot.velocity
			);

			rigidBody->setAngularVelocity(
				rigidBodySnapshot.
				angularVelocity
			);

			rigidBody->setMass(
				rigidBodySnapshot.mass
			);

			rigidBody->setRestitution(
				rigidBodySnapshot.
				restitution
			);

			rigidBody->setFriction(
				rigidBodySnapshot.friction
			);

			rigidBody->setUseGravity(
				rigidBodySnapshot.
				useGravity
			);

			rigidBody->setStatic(
				rigidBodySnapshot.isStatic
			);

			rigidBody->setColliderSize(
				rigidBodySnapshot.
				colliderSize
			);

			rigidBody->setColliderOffset(
				rigidBodySnapshot.
				colliderOffset
			);

			rigidBody->
				setColliderUsesTransformScale(
					rigidBodySnapshot.
					colliderUsesTransformScale
				);
		}

		if (objectSnapshot.material.isPresent)
		{
			auto* material =
				object->createOrGetComponent<
				MaterialComponent>();

			material->setTexturePath(
				objectSnapshot.
				material.texturePath
			);

			material->setUseTexture(
				objectSnapshot.
				material.useTexture
			);

			material->setUvTiling(
				objectSnapshot.
				material.uvTiling
			);

			material->setUvOffset(
				objectSnapshot.
				material.uvOffset
			);

			material->setColor(
				objectSnapshot.
				material.color
			);

			material->setRoughness(
				objectSnapshot.
				material.roughness
			);

			material->setMetallic(
				objectSnapshot.
				material.metallic
			);
		}

		auto& transform =
			object->getTransform();

		transform.setPosition(
			objectSnapshot.position
		);

		transform.setRotation(
			objectSnapshot.rotation
		);

		transform.setScale(
			objectSnapshot.scale
		);
	}

	m_world->flushGameObjectEvents();

	m_cubeCounter =
		snapshot.cubeCounter;

	m_planeCounter =
		snapshot.planeCounter;

	m_sphereCounter =
		snapshot.sphereCounter;

	m_capsuleCounter =
		snapshot.capsuleCounter;

	m_sceneStatusMessage =
		snapshot.sceneStatusMessage;

	for (auto* object :
		m_world->getGameObjects())
	{
		if (!object)
			continue;

		if (
			std::find(
				snapshot.selectedNames.begin(),
				snapshot.selectedNames.end(),
				object->getName()
			) ==
			snapshot.selectedNames.end()
			)
		{
			continue;
		}

		if (!m_selectedObject)
		{
			selectOnly(object);
		}
		else
		{
			toggleObjectSelection(object);
		}
	}

	m_isRestoringEditorSnapshot = false;
}

bool dx3d::Game::areEditorSnapshotsEqual(
	const EditorSnapshot& lhs,
	const EditorSnapshot& rhs
) const noexcept
{
	if (
		lhs.cubeCounter != rhs.cubeCounter ||
		lhs.planeCounter != rhs.planeCounter ||
		lhs.sphereCounter != rhs.sphereCounter ||
		lhs.capsuleCounter != rhs.capsuleCounter ||
		lhs.objects.size() != rhs.objects.size()
		)
	{
		return false;
	}

	for (size_t index = 0;
		index < lhs.objects.size();
		++index)
	{
		const auto& lhsObject =
			lhs.objects[index];

		const auto& rhsObject =
			rhs.objects[index];

		if (
			lhsObject.type != rhsObject.type ||
			lhsObject.name != rhsObject.name ||
			!areVec3Equal(
				lhsObject.position,
				rhsObject.position
			) ||
			!areVec3Equal(
				lhsObject.rotation,
				rhsObject.rotation
			) ||
			!areVec3Equal(
				lhsObject.scale,
				rhsObject.scale
			) ||
			lhsObject.modelPath !=
			rhsObject.modelPath ||
			lhsObject.texturePath !=
			rhsObject.texturePath ||
			lhsObject.material.isPresent !=
			rhsObject.material.isPresent ||
			lhsObject.material.texturePath !=
			rhsObject.material.texturePath ||
			lhsObject.material.useTexture !=
			rhsObject.material.useTexture ||
			!areVec2Equal(
				lhsObject.material.uvTiling,
				rhsObject.material.uvTiling
			) ||
			!areVec2Equal(
				lhsObject.material.uvOffset,
				rhsObject.material.uvOffset
			) ||
			!areVec4Equal(
				lhsObject.material.color,
				rhsObject.material.color
			) ||
			!areFloatsEqual(
				lhsObject.material.roughness,
				rhsObject.material.roughness
			) ||
			!areFloatsEqual(
				lhsObject.material.metallic,
				rhsObject.material.metallic
			) ||
			!areMeshDataEqual(
				lhsObject.meshData,
				rhsObject.meshData
			) ||
			!areVec3Equal(
				lhsObject.lightColor,
				rhsObject.lightColor
			) ||
			!areFloatsEqual(
				lhsObject.lightIntensity,
				rhsObject.lightIntensity
			) ||
			!areFloatsEqual(
				lhsObject.ambientStrength,
				rhsObject.ambientStrength
			) ||
			!areFloatsEqual(
				lhsObject.shadowArea,
				rhsObject.shadowArea
			) ||
			lhsObject.castShadows !=
			rhsObject.castShadows
			)
		{
			return false;
		}

		const auto& lhsBody =
			lhsObject.rigidBody;

		const auto& rhsBody =
			rhsObject.rigidBody;

		if (
			lhsBody.isPresent !=
			rhsBody.isPresent
			)
		{
			return false;
		}

		if (!lhsBody.isPresent)
			continue;

		if (
			!areVec3Equal(
				lhsBody.velocity,
				rhsBody.velocity
			) ||
			!areVec3Equal(
				lhsBody.angularVelocity,
				rhsBody.angularVelocity
			) ||
			!areFloatsEqual(
				lhsBody.mass,
				rhsBody.mass
			) ||
			!areFloatsEqual(
				lhsBody.restitution,
				rhsBody.restitution
			) ||
			!areFloatsEqual(
				lhsBody.friction,
				rhsBody.friction
			) ||
			lhsBody.useGravity !=
			rhsBody.useGravity ||
			lhsBody.isStatic !=
			rhsBody.isStatic ||
			!areVec3Equal(
				lhsBody.colliderSize,
				rhsBody.colliderSize
			) ||
			!areVec3Equal(
				lhsBody.colliderOffset,
				rhsBody.colliderOffset
			) ||
			lhsBody.
			colliderUsesTransformScale !=
			rhsBody.
			colliderUsesTransformScale
			)
		{
			return false;
		}
	}

	return true;
}

void dx3d::Game::pushUndoSnapshot(
	const EditorSnapshot& snapshot
)
{
	if (m_isRestoringEditorSnapshot)
		return;

	if (!m_undoStack.empty() &&
		areEditorSnapshotsEqual(
			m_undoStack.back(),
			snapshot
		))
	{
		return;
	}

	m_undoStack.push_back(
		snapshot
	);

	if (m_undoStack.size() >
		MaximumUndoHistory)
	{
		m_undoStack.erase(
			m_undoStack.begin()
		);
	}

	m_redoStack.clear();
}

void dx3d::Game::beginPendingUndoSnapshot(
	const EditorSnapshot& snapshot
)
{
	if (m_isRestoringEditorSnapshot ||
		m_world->isPhysicsEnabled() ||
		m_hasPendingUndoSnapshot)
	{
		return;
	}

	m_pendingUndoSnapshot =
		snapshot;

	m_hasPendingUndoSnapshot = true;
}

void dx3d::Game::commitPendingUndoSnapshot(
	const EditorSnapshot& currentSnapshot,
	bool editorStillActive
)
{
	if (!m_hasPendingUndoSnapshot ||
		editorStillActive)
	{
		return;
	}

	if (!areEditorSnapshotsEqual(
		m_pendingUndoSnapshot,
		currentSnapshot
	))
	{
		pushUndoSnapshot(
			m_pendingUndoSnapshot
		);
	}

	m_hasPendingUndoSnapshot = false;
}

bool dx3d::Game::canUndo() const noexcept
{
	return !m_undoStack.empty();
}

bool dx3d::Game::canRedo() const noexcept
{
	return !m_redoStack.empty();
}

void dx3d::Game::undoEditorOperation()
{
	if (m_hasPendingUndoSnapshot)
	{
		commitPendingUndoSnapshot(
			captureEditorSnapshot(),
			false
		);
	}

	if (!canUndo())
		return;

	const EditorSnapshot currentSnapshot =
		captureEditorSnapshot();

	EditorSnapshot undoSnapshot =
		m_undoStack.back();

	m_undoStack.pop_back();

	if (!areEditorSnapshotsEqual(
		currentSnapshot,
		undoSnapshot
	))
	{
		m_redoStack.push_back(
			currentSnapshot
		);
	}

	restoreEditorSnapshot(
		undoSnapshot
	);
}

void dx3d::Game::redoEditorOperation()
{
	if (!canRedo())
		return;

	const EditorSnapshot currentSnapshot =
		captureEditorSnapshot();

	EditorSnapshot redoSnapshot =
		m_redoStack.back();

	m_redoStack.pop_back();

	if (!areEditorSnapshotsEqual(
		currentSnapshot,
		redoSnapshot
	))
	{
		m_undoStack.push_back(
			currentSnapshot
		);
	}

	restoreEditorSnapshot(
		redoSnapshot
	);
}

void dx3d::Game::handleViewportPicking(
	CameraComponent* camera,
	const TransformGizmo::ViewportArea& renderViewportArea,
	const TransformGizmo::ViewportArea& interactionViewportArea
)
{
	if (!camera)
		return;

	const bool leftMousePressed =
		ImGui::IsMouseClicked(
			ImGuiMouseButton_Left
		);

	if (!leftMousePressed)
		return;

	const ImGuiIO& io =
		ImGui::GetIO();

	const bool imguiWindowHovered =
		ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AnyWindow |
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
		);

	if (io.WantCaptureMouse ||
		imguiWindowHovered)
	{
		return;
	}

	const bool rightMouseDown =
		ImGui::IsMouseDown(
			ImGuiMouseButton_Right
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

	const ImVec2 imguiMousePosition =
		ImGui::GetMousePos();

	const Vec2 mousePosition
	{
		imguiMousePosition.x,
		imguiMousePosition.y
	};

	if (!isPointInsideViewport(
		mousePosition,
		interactionViewportArea
	))
	{
		return;
	}

	if (ImGui::IsPopupOpen(
		nullptr,
		ImGuiPopupFlags_AnyPopupId
	))
	{
		return;
	}

	if (ImGui::GetDragDropPayload())
		return;

	const PickingViewportArea pickingViewport
	{
		renderViewportArea.x,
		renderViewportArea.y,
		renderViewportArea.width,
		renderViewportArea.height
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

void dx3d::Game::refreshProjectAssets()
{
	m_projectAssets.clear();

	std::error_code error{};

	if (!std::filesystem::exists(
		m_projectCurrentPath,
		error
	))
	{
		m_projectCurrentPath =
			m_projectRootPath;
	}

	if (!std::filesystem::exists(
		m_projectCurrentPath,
		error
	))
	{
		m_projectAssetsDirty = false;
		return;
	}

	for (const auto& entry :
		std::filesystem::directory_iterator(
			m_projectCurrentPath,
			error
		))
	{
		if (error)
			break;

		ProjectAssetEntry asset{};

		asset.isDirectory =
			entry.is_directory(error);

		const auto path =
			entry.path();

		asset.name =
			path.filename().string();

		asset.path =
			path.generic_string();

		asset.kind =
			getProjectAssetKind(
				asset.path,
				asset.isDirectory
			);

		if (!asset.isDirectory)
		{
			const auto size =
				entry.file_size(error);

			asset.fileSize =
				error
				? 0u
				: static_cast<size_t>(
					size
				);

			error.clear();
		}

		m_projectAssets.push_back(
			std::move(asset)
		);
	}

	std::sort(
		m_projectAssets.begin(),
		m_projectAssets.end(),
		[](
			const ProjectAssetEntry& lhs,
			const ProjectAssetEntry& rhs
			)
		{
			if (lhs.isDirectory !=
				rhs.isDirectory)
			{
				return lhs.isDirectory >
					rhs.isDirectory;
			}

			return toLowerAscii(lhs.name) <
				toLowerAscii(rhs.name);
		}
	);

	m_projectAssetsDirty = false;
}

dx3d::Game::ProjectAssetKind
dx3d::Game::getProjectAssetKind(
	const std::string& path,
	bool isDirectory
) const
{
	if (isDirectory)
		return ProjectAssetKind::Folder;

	const std::string extension =
		toLowerAscii(
			std::filesystem::path(path).
			extension().
			string()
		);

	if (
		extension == ".png" ||
		extension == ".jpg" ||
		extension == ".jpeg" ||
		extension == ".bmp" ||
		extension == ".tif" ||
		extension == ".tiff" ||
		extension == ".gif"
		)
	{
		return ProjectAssetKind::Image;
	}

	if (
		extension == ".obj" ||
		extension == ".fbx"
		)
	{
		return ProjectAssetKind::Model;
	}

	if (
		extension == ".hlsl" ||
		extension == ".hlsli" ||
		extension == ".fx"
		)
	{
		return ProjectAssetKind::Shader;
	}

	if (extension == ".dx3dscene")
		return ProjectAssetKind::Scene;

	if (extension == ".level")
		return ProjectAssetKind::Level;

	if (extension == ".prefab")
		return ProjectAssetKind::Prefab;

	if (
		extension == ".cs" ||
		extension == ".py" ||
		extension == ".cpp" ||
		extension == ".h" ||
		extension == ".hpp"
		)
	{
		return ProjectAssetKind::Script;
	}

	return ProjectAssetKind::Other;
}

const char* dx3d::Game::getProjectAssetIcon(
	ProjectAssetKind kind
) const noexcept
{
	switch (kind)
	{
	case ProjectAssetKind::Folder:
		return "DIR";

	case ProjectAssetKind::Image:
		return "IMG";

	case ProjectAssetKind::Model:
		return "OBJ";

	case ProjectAssetKind::Shader:
		return "SHD";

	case ProjectAssetKind::Scene:
		return "SCN";

	case ProjectAssetKind::Level:
		return "LVL";

	case ProjectAssetKind::Prefab:
		return "PFB";

	case ProjectAssetKind::Script:
		return "SCR";

	case ProjectAssetKind::Other:
		return "FILE";
	}

	return "FILE";
}

dx3d::Game::ProjectThumbnail*
dx3d::Game::getProjectThumbnail(
	const std::string& path
)
{
	auto iterator =
		m_projectThumbnailCache.find(path);

	if (iterator !=
		m_projectThumbnailCache.end())
	{
		return &iterator->second;
	}

	ProjectThumbnail thumbnail{};

	if (!loadProjectThumbnail(
		path,
		thumbnail
	))
	{
		thumbnail.failed = true;
	}

	auto [insertedIterator, inserted] =
		m_projectThumbnailCache.emplace(
			path,
			std::move(thumbnail)
		);

	(void)inserted;

	return &insertedIterator->second;
}

bool dx3d::Game::loadProjectThumbnail(
	const std::string& path,
	ProjectThumbnail& thumbnail
)
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
		return false;

	Microsoft::WRL::ComPtr<
		IWICBitmapDecoder
	> decoder{};

	const std::wstring widePath =
		std::filesystem::path(path).
		wstring();

	result =
		imagingFactory->
		CreateDecoderFromFilename(
			widePath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			decoder.GetAddressOf()
		);

	if (FAILED(result))
		return false;

	Microsoft::WRL::ComPtr<
		IWICBitmapFrameDecode
	> frame{};

	result =
		decoder->GetFrame(
			0,
			frame.GetAddressOf()
		);

	if (FAILED(result))
		return false;

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
		return false;
	}

	Microsoft::WRL::ComPtr<
		IWICFormatConverter
	> converter{};

	result =
		imagingFactory->
		CreateFormatConverter(
			converter.GetAddressOf()
		);

	if (FAILED(result))
		return false;

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
		return false;

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
		return false;

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
		return false;

	result =
		device->CreateTexture2D(
			&textureDescription,
			&textureData,
			texture.GetAddressOf()
		);

	if (FAILED(result))
		return false;

	result =
		device->CreateShaderResourceView(
			texture.Get(),
			nullptr,
			thumbnail.resourceView.
			ReleaseAndGetAddressOf()
		);

	if (FAILED(result))
		return false;

	thumbnail.width =
		static_cast<f32>(width);

	thumbnail.height =
		static_cast<f32>(height);

	thumbnail.failed = false;

	return true;
}

void dx3d::Game::drawProjectWindow(
	f32 workX,
	f32 workY,
	f32 workWidth,
	f32 workHeight,
	f32 rightPanelWidth
)
{
	ImGui::SetNextWindowPos(
		{
			workX,
			workY + workHeight -
			m_projectPanelHeight
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			std::max(
				220.0f,
				workWidth - rightPanelWidth
			),
			m_projectPanelHeight
		},
		ImGuiCond_Always
	);

	if (!ImGui::Begin(
		"Project",
		&m_showProjectWindow
	))
	{
		ImGui::End();
		return;
	}

	if (m_projectAssetsDirty)
	{
		refreshProjectAssets();
	}

	auto findSelectedProjectAsset =
		[&]() -> const ProjectAssetEntry*
		{
			for (const auto& asset :
				m_projectAssets)
			{
				if (asset.path ==
					m_selectedProjectAssetPath)
				{
					return &asset;
				}
			}

			return nullptr;
		};

	auto openProjectAsset =
		[&](
			const ProjectAssetEntry& asset
			)
		{
			if (asset.isDirectory)
			{
				m_projectCurrentPath =
					asset.path;

				m_projectAssetsDirty = true;
				m_projectSearchBuffer[0] = '\0';
				return;
			}

			if (asset.kind ==
				ProjectAssetKind::Model)
			{
				createModelObjectFromAsset(
					asset.path
				);
				return;
			}

			if (asset.kind ==
				ProjectAssetKind::Scene)
			{
				loadSceneFromPath(
					asset.path
				);
				return;
			}

			if (asset.kind ==
				ProjectAssetKind::Level)
			{
				loadLevelFromPath(
					asset.path
				);
			}
		};

	const ProjectAssetEntry* selectedProjectAsset =
		findSelectedProjectAsset();

	std::error_code pathError{};

	const bool canGoUp =
		!std::filesystem::equivalent(
			m_projectCurrentPath,
			m_projectRootPath,
			pathError
		);

	if (ImGui::Button("Assets"))
	{
		m_projectCurrentPath =
			m_projectRootPath;

		m_projectAssetsDirty = true;
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(!canGoUp);

	if (ImGui::ArrowButton(
		"##ProjectUp",
		ImGuiDir_Up
	))
	{
		const auto parent =
			std::filesystem::path(
				m_projectCurrentPath
			).parent_path();

		if (!parent.empty())
		{
			m_projectCurrentPath =
				parent.generic_string();

			m_projectAssetsDirty = true;
		}
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Refresh"))
	{
		m_projectAssetsDirty = true;
		m_projectThumbnailCache.clear();
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(180.0f);

	ImGui::InputTextWithHint(
		"##ProjectSearch",
		"Search",
		m_projectSearchBuffer.data(),
		m_projectSearchBuffer.size()
	);

	ImGui::SameLine();

	std::filesystem::path relativePath{};

	const auto relative =
		std::filesystem::relative(
			m_projectCurrentPath,
			m_projectRootPath,
			pathError
		);

	if (!pathError &&
		!relative.empty())
	{
		relativePath = relative;
	}

	const std::string displayPath =
		relativePath.empty() ||
		relativePath == "."
		? "Assets"
		: "Assets/" +
		relativePath.generic_string();

	ImGui::TextDisabled(
		"%s",
		displayPath.c_str()
	);

	if (ImGui::Button("New Scene"))
	{
		createNewScene();
	}

	ImGui::SameLine();

	if (ImGui::Button("Save Scene"))
	{
		std::string defaultSceneName =
			"Scene.dx3dscene";

		if (selectedProjectAsset &&
			!selectedProjectAsset->isDirectory &&
			selectedProjectAsset->kind ==
			ProjectAssetKind::Scene)
		{
			defaultSceneName =
				selectedProjectAsset->name;
		}

		std::snprintf(
			m_projectSceneSaveNameBuffer.data(),
			m_projectSceneSaveNameBuffer.size(),
			"%s",
			defaultSceneName.c_str()
		);

		ImGui::OpenPopup(
			"Save Scene Asset"
		);
	}

	ImGui::SameLine();

	const bool canOpenSelectedAsset =
		selectedProjectAsset &&
		!selectedProjectAsset->isDirectory &&
		(
			selectedProjectAsset->kind ==
			ProjectAssetKind::Scene ||
			selectedProjectAsset->kind ==
			ProjectAssetKind::Level
			);

	ImGui::BeginDisabled(
		!canOpenSelectedAsset
	);

	if (ImGui::Button("Open"))
	{
		openProjectAsset(
			*selectedProjectAsset
		);
	}

	ImGui::EndDisabled();

	if (ImGui::BeginPopupModal(
		"Save Scene Asset",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	))
	{
		ImGui::SetNextItemWidth(
			260.0f
		);

		ImGui::InputText(
			"Name",
			m_projectSceneSaveNameBuffer.data(),
			m_projectSceneSaveNameBuffer.size()
		);

		std::string sceneFileName =
			m_projectSceneSaveNameBuffer.data();

		const bool canSaveSceneAsset =
			!sceneFileName.empty();

		ImGui::BeginDisabled(
			!canSaveSceneAsset
		);

		if (ImGui::Button("Save"))
		{
			std::filesystem::path scenePath =
				std::filesystem::path(
					m_projectCurrentPath
				) / sceneFileName;

			const std::string extension =
				toLowerAscii(
					scenePath.extension().
					string()
				);

			if (extension != ".dx3dscene")
			{
				scenePath.replace_extension(
					".dx3dscene"
				);
			}

			saveSceneToPath(
				scenePath.generic_string()
			);

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	const ImVec2 available =
		ImGui::GetContentRegionAvail();

	if (ImGui::BeginChild(
		"##ProjectAssets",
		available,
		true
	))
	{
		constexpr f32 tileWidth = 96.0f;
		constexpr f32 thumbnailSize = 54.0f;

		const int columnCount =
			std::max(
				1,
				static_cast<int>(
					ImGui::GetContentRegionAvail().x /
					tileWidth
				)
			);

		ImGui::Columns(
			columnCount,
			"##ProjectGrid",
			false
		);

		for (auto& asset :
			m_projectAssets)
		{
			if (!containsCaseInsensitive(
				asset.name,
				m_projectSearchBuffer.data()
			))
			{
				continue;
			}

			ImGui::PushID(
				asset.path.c_str()
			);

			const bool isSelected =
				asset.path ==
				m_selectedProjectAssetPath;

			ImGui::BeginGroup();

			bool wasClicked = false;

			const ImVec4 normalButtonColor =
				isSelected
				? ImVec4(
					0.20f,
					0.36f,
					0.58f,
					1.0f
				)
				: ImVec4(
					0.12f,
					0.14f,
					0.18f,
					1.0f
				);

			const ImVec4 hoverButtonColor =
				isSelected
				? ImVec4(
					0.26f,
					0.44f,
					0.68f,
					1.0f
				)
				: ImVec4(
					0.18f,
					0.20f,
					0.25f,
					1.0f
			);

			bool assetHovered = false;
			ProjectThumbnail* thumbnail = nullptr;

			if (asset.kind ==
				ProjectAssetKind::Image)
			{
				thumbnail =
					getProjectThumbnail(
						asset.path
					);

				if (thumbnail &&
					thumbnail->resourceView)
				{
					wasClicked =
						ImGui::ImageButton(
							"##AssetThumbnail",
							(ImTextureID)
							thumbnail->
							resourceView.Get(),
							{
								thumbnailSize,
								thumbnailSize
							},
							{ 0.0f, 0.0f },
							{ 1.0f, 1.0f },
							normalButtonColor
						);

					assetHovered =
						ImGui::IsItemHovered();
				}
			}

			if (!wasClicked &&
				(asset.kind !=
				ProjectAssetKind::Image ||
					!thumbnail ||
					!thumbnail->resourceView))
			{
				ImGui::PushStyleColor(
					ImGuiCol_Button,
					normalButtonColor
				);

				ImGui::PushStyleColor(
					ImGuiCol_ButtonHovered,
					hoverButtonColor
				);

				ImGui::PushStyleColor(
					ImGuiCol_ButtonActive,
					hoverButtonColor
				);

				wasClicked =
					ImGui::Button(
						getProjectAssetIcon(
							asset.kind
						),
						{
							thumbnailSize,
							thumbnailSize
						}
					);

				ImGui::PopStyleColor(3);

				assetHovered =
					assetHovered ||
					ImGui::IsItemHovered();
			}

			if (asset.kind ==
				ProjectAssetKind::Image &&
				ImGui::BeginDragDropSource())
			{
				m_selectedProjectAssetPath =
					asset.path;

				ImGui::SetDragDropPayload(
					"DX3D_TEXTURE_PATH",
					asset.path.c_str(),
					asset.path.size() + 1
				);

				ImGui::TextUnformatted(
					asset.name.c_str()
				);

				ImGui::EndDragDropSource();
			}

			if (wasClicked)
			{
				m_selectedProjectAssetPath =
					asset.path;
			}

			if (assetHovered &&
				ImGui::IsMouseDoubleClicked(
					ImGuiMouseButton_Left
				))
			{
				openProjectAsset(
					asset
				);
			}

			if (ImGui::BeginPopupContextItem(
				"##ProjectAssetContext"
			))
			{
				m_selectedProjectAssetPath =
					asset.path;

				if (ImGui::MenuItem(
					"Copy Path"
				))
				{
					ImGui::SetClipboardText(
						asset.path.c_str()
					);
				}

				if (asset.isDirectory &&
					ImGui::MenuItem("Open"))
				{
					m_projectCurrentPath =
						asset.path;

					m_projectAssetsDirty = true;
					m_projectSearchBuffer[0] = '\0';
				}

				if (!asset.isDirectory &&
					asset.kind ==
					ProjectAssetKind::Model &&
					ImGui::MenuItem(
						"Create Model Object"
					))
				{
					createModelObjectFromAsset(
						asset.path
					);
				}

				if (!asset.isDirectory &&
					asset.kind ==
					ProjectAssetKind::Scene)
				{
					if (ImGui::MenuItem(
						"Open Scene"
					))
					{
						loadSceneFromPath(
							asset.path
						);
					}

					if (ImGui::MenuItem(
						"Save Current Scene Here"
					))
					{
						saveSceneToPath(
							asset.path
						);
					}
				}

				if (!asset.isDirectory &&
					asset.kind ==
					ProjectAssetKind::Level)
				{
					if (ImGui::MenuItem(
						"Import Level"
					))
					{
						loadLevelFromPath(
							asset.path
						);
					}

					if (ImGui::MenuItem(
						"Export Current Level Here"
					))
					{
						saveLevelToPath(
							asset.path
						);
					}
				}

				ImGui::EndPopup();
			}

			const std::string label =
				truncateText(
					asset.name,
					20
				);

			ImGui::PushTextWrapPos(
				ImGui::GetCursorPosX() +
				tileWidth - 10.0f
			);

			ImGui::TextWrapped(
				"%s",
				label.c_str()
			);

			ImGui::PopTextWrapPos();

			assetHovered =
				assetHovered ||
				ImGui::IsItemHovered();

			if (assetHovered)
			{
				ImGui::BeginTooltip();

				ImGui::TextUnformatted(
					asset.name.c_str()
				);

				ImGui::Separator();

				ImGui::TextDisabled(
					"%s",
					asset.path.c_str()
				);

				if (!asset.isDirectory)
				{
					const std::string size =
						formatFileSize(
							asset.fileSize
						);

					ImGui::TextDisabled(
						"%s",
						size.c_str()
					);
				}

				ImGui::EndTooltip();
			}

			ImGui::EndGroup();
			ImGui::PopID();
			ImGui::NextColumn();
		}

		ImGui::Columns(1);
	}

	ImGui::EndChild();

	if (!m_selectedProjectAssetPath.empty())
	{
		ImGui::Separator();

		ImGui::TextDisabled(
			"%s",
			m_selectedProjectAssetPath.c_str()
		);
	}

	ImGui::End();
}

void dx3d::Game::createNewScene()
{
	const EditorSnapshot undoSnapshot =
		captureEditorSnapshot();

	m_world->stopPhysics();

	SceneSerializer::clear(
		*m_world
	);

	m_world->flushGameObjectEvents();

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter = 0;
	m_planeCounter = 0;
	m_sphereCounter = 0;
	m_capsuleCounter = 0;

	m_sceneStatusMessage =
		"New scene created";

	if (!areEditorSnapshotsEqual(
		undoSnapshot,
		captureEditorSnapshot()
	))
	{
		pushUndoSnapshot(
			undoSnapshot
		);
	}
}

void dx3d::Game::saveScene()
{
	saveSceneToPath(
		"Scene.dx3dscene"
	);
}

void dx3d::Game::saveSceneToPath(
	const std::string& filePath
)
{
	if (filePath.empty())
		return;

	const std::filesystem::path scenePath{
		filePath
	};

	std::error_code error{};

	const auto parentPath =
		scenePath.parent_path();

	if (!parentPath.empty())
	{
		std::filesystem::create_directories(
			parentPath,
			error
		);

		if (error)
		{
			m_sceneStatusMessage =
				"Scene save failed: " +
				scenePath.filename().string();

			DX3DLogError(
				"Scene save failed: {}",
				filePath
			);

			return;
		}
	}

	const bool saved =
		SceneSerializer::save(
			*m_world,
			scenePath.generic_string()
		);

	if (saved)
	{
		m_sceneStatusMessage =
			"Saved: " +
			scenePath.filename().string();

		DX3DLogInfo(
			"Scene saved: {}",
			scenePath.generic_string()
		);

		m_projectAssetsDirty = true;
		m_selectedProjectAssetPath =
			scenePath.generic_string();
	}
	else
	{
		m_sceneStatusMessage =
			"Scene save failed: " +
			scenePath.filename().string();

		DX3DLogError(
			"Scene save failed: {}",
			scenePath.generic_string()
		);
	}
}

void dx3d::Game::loadScene()
{
	loadSceneFromPath(
		"Scene.dx3dscene"
	);
}

void dx3d::Game::loadSceneFromPath(
	const std::string& filePath
)
{
	if (filePath.empty())
		return;

	const std::filesystem::path scenePath{
		filePath
	};

	const EditorSnapshot undoSnapshot =
		captureEditorSnapshot();

	m_world->stopPhysics();

	const SceneLoadResult result =
		SceneSerializer::load(
			*m_world,
			scenePath.generic_string()
		);

	if (!result.success)
	{
		m_sceneStatusMessage =
			"Scene load failed: " +
			scenePath.filename().string();

		DX3DLogError(
			"Scene load failed: {}",
			scenePath.generic_string()
		);

		return;
	}

	m_world->flushGameObjectEvents();

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter =
		result.cubeCount;

	m_planeCounter =
		result.planeCount;

	m_sphereCounter =
		result.sphereCount;

	m_capsuleCounter =
		result.capsuleCount;

	m_sceneStatusMessage =
		"Opened: " +
		scenePath.filename().string();

	DX3DLogInfo(
		"Scene loaded: {}",
		scenePath.generic_string()
	);

	m_selectedProjectAssetPath =
		scenePath.generic_string();

	if (!areEditorSnapshotsEqual(
		undoSnapshot,
		captureEditorSnapshot()
	))
	{
		pushUndoSnapshot(
			undoSnapshot
		);
	}
}

void dx3d::Game::saveLevel()
{
	saveLevelToPath(
		"Scene.level"
	);
}

void dx3d::Game::saveLevelToPath(
	const std::string& filePath
)
{
	if (filePath.empty())
		return;

	const std::filesystem::path levelPath{
		filePath
	};

	std::error_code error{};

	const auto parentPath =
		levelPath.parent_path();

	if (!parentPath.empty())
	{
		std::filesystem::create_directories(
			parentPath,
			error
		);

		if (error)
		{
			m_sceneStatusMessage =
				"Level export failed: " +
				levelPath.filename().string();

			DX3DLogError(
				"Level export failed: {}",
				filePath
			);

			return;
		}
	}

	const bool saved =
		SceneSerializer::saveLevel(
			*m_world,
			levelPath.generic_string()
		);

	if (saved)
	{
		m_sceneStatusMessage =
			"Exported: " +
			levelPath.filename().string();

		DX3DLogInfo(
			"Level exported: {}",
			levelPath.generic_string()
		);

		m_projectAssetsDirty = true;
		m_selectedProjectAssetPath =
			levelPath.generic_string();
	}
	else
	{
		m_sceneStatusMessage =
			"Level export failed: " +
			levelPath.filename().string();

		DX3DLogError(
			"Level export failed: {}",
			levelPath.generic_string()
		);
	}
}

void dx3d::Game::loadLevel()
{
	const std::string levelFilePath =
		openLevelFileDialog(
			m_display
			? static_cast<HWND>(
				m_display->getNativeHandle()
			)
			: nullptr
		);

	if (levelFilePath.empty())
	{
		m_sceneStatusMessage =
			"Level import canceled";

		return;
	}

	loadLevelFromPath(
		levelFilePath
	);
}

void dx3d::Game::loadLevelFromPath(
	const std::string& filePath
)
{
	if (filePath.empty())
		return;

	const std::filesystem::path levelPath{
		filePath
	};

	const EditorSnapshot undoSnapshot =
		captureEditorSnapshot();

	m_world->stopPhysics();

	const SceneLoadResult result =
		SceneSerializer::loadLevel(
			*m_world,
			levelPath.generic_string()
		);

	if (!result.success)
	{
		m_sceneStatusMessage =
			"Level import failed: " +
			levelPath.filename().string();

		DX3DLogError(
			"Level import failed: {}",
			levelPath.generic_string()
		);

		return;
	}

	m_world->flushGameObjectEvents();

	clearSelection();

	m_objectClipboard =
		ObjectCopyData{};

	m_cubeCounter =
		result.cubeCount;

	m_planeCounter =
		result.planeCount;

	m_sphereCounter =
		result.sphereCount;

	m_capsuleCounter =
		result.capsuleCount;

	m_sceneStatusMessage =
		"Imported: " +
		levelPath.filename().string();

	DX3DLogInfo(
		"Level imported: {}",
		levelPath.generic_string()
	);

	m_selectedProjectAssetPath =
		levelPath.generic_string();

	if (!areEditorSnapshotsEqual(
		undoSnapshot,
		captureEditorSnapshot()
	))
	{
		pushUndoSnapshot(
			undoSnapshot
		);
	}
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

	processDroppedAssetFiles();

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

	const EditorSnapshot frameStartSnapshot =
		captureEditorSnapshot();

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

			if (ImGui::MenuItem(
				"Export .level"
			))
			{
				saveLevel();
			}

			if (ImGui::MenuItem(
				"Import .level"
			))
			{
				loadLevel();
			}

			ImGui::Separator();

			ImGui::TextDisabled(
				"%s",
				m_sceneStatusMessage.c_str()
			);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::BeginDisabled(
				!canUndo() ||
				physicsIsPlaying
			);

			if (ImGui::MenuItem(
				"Undo",
				"Ctrl+Z"
			))
			{
				undoEditorOperation();
			}

			ImGui::EndDisabled();

			ImGui::BeginDisabled(
				!canRedo() ||
				physicsIsPlaying
			);

			if (ImGui::MenuItem(
				"Redo",
				"Ctrl+Y / Ctrl+Shift+Z"
			))
			{
				redoEditorOperation();
			}

			ImGui::EndDisabled();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Game Object"))
		{
			if (ImGui::MenuItem("Create Cube"))
			{
				pushUndoSnapshot(
					frameStartSnapshot
				);

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

			if (ImGui::MenuItem("Create Sphere"))
			{
				pushUndoSnapshot(
					frameStartSnapshot
				);

				++m_sphereCounter;

				auto* sphere =
					m_world->createGameObject<GameObject>();

				if (m_sphereCounter == 1)
				{
					sphere->setName("Sphere");
				}
				else
				{
					sphere->setName(
						"Sphere (" +
						std::to_string(
							m_sphereCounter
						) +
						")"
					);
				}

				sphere->createOrGetComponent<
					SphereComponent>();

				sphere->getTransform().setPosition(
					{
						static_cast<f32>(
							m_sphereCounter - 1
						) * 1.5f,
						0.5f,
						2.0f
					}
				);

				sphere->getTransform().setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(sphere);
			}

			if (ImGui::MenuItem("Create Capsule"))
			{
				pushUndoSnapshot(
					frameStartSnapshot
				);

				++m_capsuleCounter;

				auto* capsule =
					m_world->createGameObject<GameObject>();

				if (m_capsuleCounter == 1)
				{
					capsule->setName("Capsule");
				}
				else
				{
					capsule->setName(
						"Capsule (" +
						std::to_string(
							m_capsuleCounter
						) +
						")"
					);
				}

				capsule->createOrGetComponent<
					CapsuleComponent>();

				capsule->getTransform().setPosition(
					{
						static_cast<f32>(
							m_capsuleCounter - 1
						) * 1.5f,
						1.0f,
						-2.0f
					}
				);

				capsule->getTransform().setScale(
					{ 1.0f, 1.0f, 1.0f }
				);

				selectOnly(capsule);
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
				pushUndoSnapshot(
					frameStartSnapshot
				);

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
				pushUndoSnapshot(
					frameStartSnapshot
				);

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

			if (ImGui::MenuItem(
				"Create Stanford Bunny"
			))
			{
				MeshData bunnyMesh{};

				std::string bunnyPath{};

				const std::string bunnyPaths[]
				{
					"Assets/Models/bunny.obj",
					"DX3D/Assets/Models/bunny.obj"
				};

				for (const auto& path : bunnyPaths)
				{
					if (loadObjMesh(
						path,
						bunnyMesh
					))
					{
						bunnyPath = path;
						break;
					}
				}

				if (bunnyMesh.empty())
				{
					DX3DLogError(
						"Failed to load Stanford Bunny OBJ."
					);
				}
				else
				{
					pushUndoSnapshot(
						frameStartSnapshot
					);

					auto* bunny =
						m_world->createGameObject<
						GameObject>();

					bunny->setName(
						"Stanford Bunny"
					);

					auto* modelComponent =
						bunny->createOrGetComponent<
						ModelComponent>();

					modelComponent->setMeshData(
						bunnyMesh
					);

					modelComponent->setModelPath(
						bunnyPath
					);

					auto& bunnyTransform =
						bunny->getTransform();

					bunnyTransform.setPosition(
						{ 0.0f, 0.0f, 0.0f }
					);

					bunnyTransform.setRotation(
						{ 0.0f, 0.0f, 0.0f }
					);

					bunnyTransform.setScale(
						{ 1.0f, 1.0f, 1.0f }
					);

					selectOnly(
						bunny
					);

					DX3DLogInfo(
						"Stanford Bunny loaded."
					);
				}
			}

			if (ImGui::MenuItem(
				"Create Armadillo"
			))
			{
				MeshData armadilloMesh{};

				std::string armadilloPath{};

				const std::string armadilloPaths[]
				{
					"Assets/Models/armadillo.obj",
					"DX3D/Assets/Models/armadillo.obj"
				};

				for (const auto& path : armadilloPaths)
				{
					if (loadObjMesh(
						path,
						armadilloMesh
					))
					{
						armadilloPath = path;
						break;
					}
				}

				if (armadilloMesh.empty())
				{
					DX3DLogError(
						"Failed to load Armadillo OBJ."
					);
				}
				else
				{
					pushUndoSnapshot(
						frameStartSnapshot
					);

					auto* armadillo =
						m_world->createGameObject<
						GameObject>();

					armadillo->setName(
						"Armadillo"
					);

					auto* modelComponent =
						armadillo->createOrGetComponent<
						ModelComponent>();

					modelComponent->setMeshData(
						armadilloMesh
					);

					modelComponent->setModelPath(
						armadilloPath
					);

					auto& armadilloTransform =
						armadillo->getTransform();

					armadilloTransform.setPosition(
						{ 3.0f, 0.0f, 0.0f }
					);

					armadilloTransform.setRotation(
						{ 0.0f, 0.0f, 0.0f }
					);

					armadilloTransform.setScale(
						{ 1.0f, 1.0f, 1.0f }
					);

					selectOnly(
						armadillo
					);

					DX3DLogInfo(
						"Armadillo loaded."
					);
				}
			}

			if (ImGui::MenuItem(
				"Create Utah Teapot"
			))
			{
				MeshData teapotMesh{};

				std::string teapotPath{};

				const std::string teapotPaths[]
				{
					"Assets/Models/teapot.obj",
					"DX3D/Assets/Models/teapot.obj"
				};

				for (const auto& path : teapotPaths)
				{
					if (loadObjMesh(
						path,
						teapotMesh
					))
					{
						teapotPath = path;
						break;
					}
				}

				if (teapotMesh.empty())
				{
					DX3DLogError(
						"Failed to load Utah Teapot OBJ."
					);
				}
				else
				{
					pushUndoSnapshot(
						frameStartSnapshot
					);

					auto* teapot =
						m_world->createGameObject<
						GameObject>();

					teapot->setName(
						"Utah Teapot"
					);

					auto* modelComponent =
						teapot->createOrGetComponent<
						ModelComponent>();

					modelComponent->setMeshData(
						teapotMesh
					);

					modelComponent->setModelPath(
						teapotPath
					);

					modelComponent->setTexturePath(
						"DX3D/Assets/Textures/brick.png"
					);

					auto& teapotTransform =
						teapot->getTransform();

					teapotTransform.setPosition(
						{ -3.0f, 0.0f, 0.0f }
					);

					teapotTransform.setRotation(
						{ 0.0f, 0.0f, 0.0f }
					);

					teapotTransform.setScale(
						{ 1.0f, 1.0f, 1.0f }
					);

					selectOnly(
						teapot
					);

					DX3DLogInfo(
						"Utah Teapot loaded with brick texture."
					);
				}
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
				pushUndoSnapshot(
					frameStartSnapshot
				);

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
				pushUndoSnapshot(
					frameStartSnapshot
				);

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
			ImGui::MenuItem(
				"Project Window",
				nullptr,
				&m_showProjectWindow
			);

			ImGui::Separator();

			ImGui::MenuItem(
				"Physics Debug Overlay",
				nullptr,
				&m_showPhysicsDebugOverlay
			);

			ImGui::BeginDisabled(
				!m_showPhysicsDebugOverlay
			);

			ImGui::MenuItem(
				"Selected Physics Only",
				nullptr,
				&m_showOnlySelectedPhysicsDebug
			);

			ImGui::EndDisabled();

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

	const ImGuiViewport* viewport =
		ImGui::GetMainViewport();

	const ImVec2 workPosition =
		viewport->WorkPos;

	const ImVec2 workSize =
		viewport->WorkSize;

	m_rightPanelWidth =
		std::clamp(
			m_rightPanelWidth,
			MinimumRightPanelWidth,
			std::max(
				MinimumRightPanelWidth,
				workSize.x - MinimumViewportWidth
			)
		);

	m_outlinerPanelHeight =
		std::clamp(
			m_outlinerPanelHeight,
			MinimumOutlinerHeight,
			std::max(
				MinimumOutlinerHeight,
				workSize.y - MinimumInspectorHeight
			)
		);

	m_projectPanelHeight =
		std::clamp(
			m_projectPanelHeight,
			MinimumProjectPanelHeight,
			std::max(
				MinimumProjectPanelHeight,
				workSize.y - MinimumViewportHeight
			)
		);

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

	const f32 sceneViewportWidth =
		std::max(
			0.0f,
			workSize.x - m_rightPanelWidth -
			EditorSplitterThickness * 0.5f
		);

	const f32 sceneViewportHeight =
		std::max(
			0.0f,
			workSize.y -
			(m_showProjectWindow
				? m_projectPanelHeight +
				EditorSplitterThickness * 0.5f
				: 0.0f)
		);

	const TransformGizmo::ViewportArea sceneInteractionViewport
	{
		workPosition.x,
		workPosition.y,
		sceneViewportWidth,
		sceneViewportHeight
	};

	drawPhysicsDebugOverlay(
		editorCameraComponent,
		gizmoViewport
	);

	m_transformGizmo.draw(
		m_selectedObject,
		editorCameraComponent,
		gizmoViewport,
		sceneInteractionViewport
	);

	if (sceneInteractionViewport.width > 0.0f &&
		sceneInteractionViewport.height > 0.0f)
	{
		const ImGuiWindowFlags gizmoToolbarFlags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::SetNextWindowPos(
			{
				sceneInteractionViewport.x + 12.0f,
				sceneInteractionViewport.y + 12.0f
			},
			ImGuiCond_Always
		);

		ImGui::SetNextWindowBgAlpha(
			0.88f
		);

		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding,
			{ 6.0f, 6.0f }
		);

		if (ImGui::Begin(
			"##SceneGizmoToolbar",
			nullptr,
			gizmoToolbarFlags
		))
		{
			const bool compactToolbar =
				sceneInteractionViewport.width < 310.0f;

			const ImVec2 modeButtonSize =
				compactToolbar
				? ImVec2{ 34.0f, 28.0f }
				: ImVec2{ 82.0f, 28.0f };

			auto drawModeButton =
				[&](
					const char* label,
					const char* tooltip,
					TransformGizmo::Operation operation
					)
				{
					const bool isActive =
						m_transformGizmo.getOperation() ==
						operation;

					if (isActive)
					{
						ImGui::PushStyleColor(
							ImGuiCol_Button,
							{ 0.18f, 0.42f, 0.78f, 1.0f }
						);

						ImGui::PushStyleColor(
							ImGuiCol_ButtonHovered,
							{ 0.24f, 0.50f, 0.92f, 1.0f }
						);

						ImGui::PushStyleColor(
							ImGuiCol_ButtonActive,
							{ 0.14f, 0.34f, 0.64f, 1.0f }
						);
					}

					if (ImGui::Button(
						label,
						modeButtonSize
					))
					{
						m_transformGizmo.setOperation(
							operation
						);
					}

					if (isActive)
					{
						ImGui::PopStyleColor(
							3
						);
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip(
							"%s",
							tooltip
						);
					}
				};

			drawModeButton(
				compactToolbar ? "T" : "Translate",
				"Translate (W)",
				TransformGizmo::Operation::Translate
			);

			ImGui::SameLine(
				0.0f,
				6.0f
			);

			drawModeButton(
				compactToolbar ? "R" : "Rotate",
				"Rotate (E)",
				TransformGizmo::Operation::Rotate
			);

			ImGui::SameLine(
				0.0f,
				6.0f
			);

			drawModeButton(
				compactToolbar ? "S" : "Scale",
				"Scale (R)",
				TransformGizmo::Operation::Scale
			);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	if (m_showProjectWindow)
	{
		drawProjectWindow(
			workPosition.x,
			workPosition.y,
			workSize.x,
			workSize.y,
			m_rightPanelWidth
		);
	}

	// Scene Outliner begins here.
	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x -
			m_rightPanelWidth,
			workPosition.y
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			m_rightPanelWidth,
			m_outlinerPanelHeight
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

		if (canObjectAcceptTexture(object) &&
			ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload(
					"DX3D_TEXTURE_PATH"
				))
			{
				const char* payloadText =
					static_cast<const char*>(
						payload->Data
					);

				if (payloadText &&
					payload->DataSize > 0)
				{
					pushUndoSnapshot(
						frameStartSnapshot
					);

					assignTextureToObject(
						object,
						std::string(payloadText)
					);

					selectOnly(object);
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();
	}

	ImGui::End();

	ImGui::SetNextWindowPos(
		{
			workPosition.x + workSize.x -
			m_rightPanelWidth,
			workPosition.y +
			m_outlinerPanelHeight
		},
		ImGuiCond_Always
	);

	ImGui::SetNextWindowSize(
		{
			m_rightPanelWidth,
			workSize.y -
			m_outlinerPanelHeight
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

		if (canObjectAcceptTexture(
			m_selectedObject
		))
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Material");
			ImGui::Spacing();

			auto* material =
				m_selectedObject->getComponent<
				MaterialComponent>();

			auto* modelComponent =
				m_selectedObject->getComponent<
				ModelComponent>();

			const Vec4 materialColor =
				material
				? material->getColor()
				: Vec4{
					1.0f,
					1.0f,
					1.0f,
					1.0f
				};

			float materialColorValues[4]
			{
				materialColor.x,
				materialColor.y,
				materialColor.z,
				materialColor.w
			};

			if (ImGui::ColorEdit4(
				"Base Color",
				materialColorValues
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setColor(
					{
						materialColorValues[0],
						materialColorValues[1],
						materialColorValues[2],
						materialColorValues[3]
					}
				);
			}

			const std::string* texturePath =
				nullptr;

			const char* textureSource =
				"None";

			if (material &&
				material->hasTexture())
			{
				texturePath =
					&material->getTexturePath();

				textureSource =
					"Material";
			}
			else if (modelComponent &&
				modelComponent->hasTexture())
			{
				texturePath =
					&modelComponent->getTexturePath();

				textureSource =
					"Model";
			}

			const bool hasAssignedTexture =
				texturePath != nullptr;

			bool useTexture =
				material
				? material->getUseTexture()
				: true;

			ImGui::BeginDisabled(
				!hasAssignedTexture
			);

			if (ImGui::Checkbox(
				"Use Texture",
				&useTexture
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setUseTexture(
					useTexture
				);
			}

			ImGui::EndDisabled();

			ImGui::TextWrapped(
				"Texture: %s",
				texturePath
				? texturePath->c_str()
				: "None"
			);

			ImGui::TextDisabled(
				"Texture Source: %s",
				textureSource
			);

			ImGui::Button(
				"Drop Texture Here",
				{ -1.0f, 0.0f }
			);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload(
						"DX3D_TEXTURE_PATH"
					))
				{
					const char* payloadText =
						static_cast<const char*>(
							payload->Data
						);

					if (payloadText &&
						payload->DataSize > 0)
					{
						pushUndoSnapshot(
							frameStartSnapshot
						);

						assignTextureToObject(
							m_selectedObject,
							std::string(payloadText)
						);

						material =
							m_selectedObject->
							getComponent<
							MaterialComponent>();
					}
				}

				ImGui::EndDragDropTarget();
			}

			ImGui::BeginDisabled(
				!hasAssignedTexture
			);

			if (ImGui::Button(
				"Clear Texture"
			))
			{
				pushUndoSnapshot(
					frameStartSnapshot
				);

				if (material)
				{
					material->clearTexture();
				}

				if (modelComponent)
				{
					modelComponent->clearTexture();
				}
			}

			ImGui::EndDisabled();

			Vec2 uvTiling =
				material
				? material->getUvTiling()
				: Vec2{ 1.0f, 1.0f };

			Vec2 uvOffset =
				material
				? material->getUvOffset()
				: Vec2{};

			float uvTilingValues[2]
			{
				uvTiling.x,
				uvTiling.y
			};

			float uvOffsetValues[2]
			{
				uvOffset.x,
				uvOffset.y
			};

			if (ImGui::DragFloat2(
				"UV Tiling",
				uvTilingValues,
				0.02f,
				0.01f,
				100.0f
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setUvTiling(
					{
						uvTilingValues[0],
						uvTilingValues[1]
					}
				);
			}

			if (ImGui::DragFloat2(
				"UV Offset",
				uvOffsetValues,
				0.02f,
				-100.0f,
				100.0f
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setUvOffset(
					{
						uvOffsetValues[0],
						uvOffsetValues[1]
					}
				);
			}

			f32 roughness =
				material
				? material->getRoughness()
				: 0.50f;

			if (ImGui::SliderFloat(
				"Roughness",
				&roughness,
				0.0f,
				1.0f
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setRoughness(
					roughness
				);
			}

			f32 metallic =
				material
				? material->getMetallic()
				: 0.0f;

			if (ImGui::SliderFloat(
				"Metallic",
				&metallic,
				0.0f,
				1.0f
			))
			{
				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->setMetallic(
					metallic
				);
			}

			ImGui::Spacing();

			if (ImGui::Button(
				"Reset Material"
			))
			{
				pushUndoSnapshot(
					frameStartSnapshot
				);

				material =
					m_selectedObject->
					createOrGetComponent<
					MaterialComponent>();

				material->reset();

				if (modelComponent)
				{
					modelComponent->clearTexture();
				}
			}
		}

		auto* rigidBody =
			m_selectedObject->getComponent<
			RigidBodyComponent
			>();

		const bool canAddRigidBody =
			m_selectedObject->getComponent<
			CubeComponent
			>() != nullptr ||
			m_selectedObject->getComponent<
			SphereComponent
			>() != nullptr ||
			m_selectedObject->getComponent<
			CapsuleComponent
			>() != nullptr;

		if (rigidBody || canAddRigidBody)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Rigid Body");
			ImGui::Spacing();

			if (!rigidBody)
			{
				if (ImGui::Button(
					"Add RigidBody"
				))
				{
					rigidBody =
						m_selectedObject->
						createOrGetComponent<
						RigidBodyComponent
						>();
				}
			}

			if (rigidBody)
			{
				bool isStatic =
					rigidBody->getStatic();

				if (ImGui::Checkbox(
					"Static",
					&isStatic
				))
				{
					rigidBody->setStatic(
						isStatic
					);
				}

				bool useGravity =
					rigidBody->getUseGravity();

				if (ImGui::Checkbox(
					"Use Gravity",
					&useGravity
				))
				{
					rigidBody->setUseGravity(
						useGravity
					);
				}

				float mass =
					rigidBody->getMass();

				if (ImGui::DragFloat(
					"Mass",
					&mass,
					0.05f,
					0.001f,
					1000.0f
				))
				{
					if (mass < 0.001f)
					{
						mass = 0.001f;
					}

					rigidBody->setMass(
						mass
					);
				}

				float restitution =
					rigidBody->getRestitution();

				if (ImGui::SliderFloat(
					"Restitution",
					&restitution,
					0.0f,
					1.0f
				))
				{
					rigidBody->setRestitution(
						restitution
					);
				}

				float friction =
					rigidBody->getFriction();

				if (ImGui::SliderFloat(
					"Friction",
					&friction,
					0.0f,
					1.0f
				))
				{
					rigidBody->setFriction(
						friction
					);
				}

				bool matchRenderScale =
					rigidBody->
					getColliderUsesTransformScale();

				if (ImGui::Checkbox(
					"Match Render Scale",
					&matchRenderScale
				))
				{
					if (!matchRenderScale)
					{
						rigidBody->
							setColliderSize(
								rigidBody->
								getEffectiveColliderSize()
							);
					}

					rigidBody->
						setColliderUsesTransformScale(
							matchRenderScale
						);
				}

				Vec3 colliderSize =
					matchRenderScale
					? rigidBody->
					getEffectiveColliderSize()
					: rigidBody->getColliderSize();

				float colliderSizeValues[3]
				{
					colliderSize.x,
					colliderSize.y,
					colliderSize.z
				};

				ImGui::BeginDisabled(
					matchRenderScale
				);

				if (ImGui::DragFloat3(
					"Collider Size",
					colliderSizeValues,
					0.05f,
					0.002f,
					1000.0f
				))
				{
					rigidBody->setColliderSize(
						{
							colliderSizeValues[0],
							colliderSizeValues[1],
							colliderSizeValues[2]
						}
					);
				}

				ImGui::EndDisabled();

				Vec3 colliderOffset =
					rigidBody->getColliderOffset();

				float colliderOffsetValues[3]
				{
					colliderOffset.x,
					colliderOffset.y,
					colliderOffset.z
				};

				if (ImGui::DragFloat3(
					"Collider Offset",
					colliderOffsetValues,
					0.05f
				))
				{
					rigidBody->setColliderOffset(
						{
							colliderOffsetValues[0],
							colliderOffsetValues[1],
							colliderOffsetValues[2]
						}
					);
				}

				auto velocity =
					rigidBody->getVelocity();

				float velocityValues[3]
				{
					velocity.x,
					velocity.y,
					velocity.z
				};

				if (ImGui::DragFloat3(
					"Velocity",
					velocityValues,
					0.05f
				))
				{
					rigidBody->setVelocity(
						{
							velocityValues[0],
							velocityValues[1],
							velocityValues[2]
						}
					);
				}

				auto angularVelocity =
					rigidBody->
					getAngularVelocity();

				float angularVelocityValues[3]
				{
					angularVelocity.x,
					angularVelocity.y,
					angularVelocity.z
				};

				if (ImGui::DragFloat3(
					"Angular Velocity",
					angularVelocityValues,
					0.05f
				))
				{
					rigidBody->
						setAngularVelocity(
							{
								angularVelocityValues[0],
								angularVelocityValues[1],
								angularVelocityValues[2]
							}
						);
				}
			}
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

	const ImGuiWindowFlags splitterWindowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	auto drawSplitter =
		[&](
			const char* windowName,
			const ImVec2& position,
			const ImVec2& size,
			ImGuiMouseCursor cursor
			)
		{
			ImGui::SetNextWindowPos(
				position,
				ImGuiCond_Always
			);

			ImGui::SetNextWindowSize(
				size,
				ImGuiCond_Always
			);

			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				{ 0.0f, 0.0f }
			);

			ImGui::Begin(
				windowName,
				nullptr,
				splitterWindowFlags
			);

			ImGui::InvisibleButton(
				"##SplitterDrag",
				size
			);

			const bool hovered =
				ImGui::IsItemHovered();

			const bool active =
				ImGui::IsItemActive();

			if (hovered || active)
			{
				ImGui::SetMouseCursor(
					cursor
				);
			}

			const ImU32 color =
				ImGui::GetColorU32(
					active
					? ImGuiCol_ButtonActive
					: hovered
					? ImGuiCol_ButtonHovered
					: ImGuiCol_Separator
				);

			ImGui::GetWindowDrawList()->
				AddRectFilled(
					ImGui::GetItemRectMin(),
					ImGui::GetItemRectMax(),
					color
				);

			ImGui::End();
			ImGui::PopStyleVar();

			return active;
		};

	if (drawSplitter(
		"##RightPanelSplitter",
		{
			workPosition.x + workSize.x -
			m_rightPanelWidth -
			EditorSplitterThickness * 0.5f,
			workPosition.y
		},
		{
			EditorSplitterThickness,
			workSize.y
		},
		ImGuiMouseCursor_ResizeEW
	))
	{
		m_rightPanelWidth -=
			ImGui::GetIO().MouseDelta.x;
	}

	if (drawSplitter(
		"##OutlinerInspectorSplitter",
		{
			workPosition.x + workSize.x -
			m_rightPanelWidth,
			workPosition.y +
			m_outlinerPanelHeight -
			EditorSplitterThickness * 0.5f
		},
		{
			m_rightPanelWidth,
			EditorSplitterThickness
		},
		ImGuiMouseCursor_ResizeNS
	))
	{
		m_outlinerPanelHeight +=
			ImGui::GetIO().MouseDelta.y;
	}

	if (m_showProjectWindow)
	{
		if (drawSplitter(
			"##ProjectPanelSplitter",
			{
				workPosition.x,
				workPosition.y +
				workSize.y -
				m_projectPanelHeight -
				EditorSplitterThickness * 0.5f
			},
			{
				std::max(
					220.0f,
					workSize.x -
					m_rightPanelWidth
				),
				EditorSplitterThickness
			},
			ImGuiMouseCursor_ResizeNS
		))
		{
			m_projectPanelHeight -=
				ImGui::GetIO().MouseDelta.y;
		}
	}

	handleViewportPicking(
		editorCameraComponent,
		gizmoViewport,
		sceneInteractionViewport
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

	const bool shiftHeld =
		ImGui::GetIO().KeyShift;

	const bool allowHistoryShortcuts =
		controlHeld &&
		!m_transformGizmo.isUsing() &&
		!rightMouseDown &&
		!editingImGuiValue &&
		!typingInImGui &&
		!popupIsOpen &&
		!m_world->isPhysicsEnabled();

	if (allowHistoryShortcuts)
	{
		if (m_inputSystem->isKeyPressed(
			KeyCode::Z
		))
		{
			if (shiftHeld)
			{
				redoEditorOperation();
			}
			else
			{
				undoEditorOperation();
			}
		}
		else if (m_inputSystem->isKeyPressed(
			KeyCode::Y
		))
		{
			redoEditorOperation();
		}
	}

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

		pushUndoSnapshot(
			frameStartSnapshot
		);

		m_world->destroyGameObject(
			objectToDelete
		);

		removeObjectFromSelection(
			objectToDelete
		);
	}

	const EditorSnapshot frameEndSnapshot =
		captureEditorSnapshot();

	const bool editorStillActive =
		ImGui::IsAnyItemActive() ||
		m_transformGizmo.isUsing();

	if (!m_skipUndoTrackingThisFrame &&
		!m_world->isPhysicsEnabled())
	{
		if (!areEditorSnapshotsEqual(
			frameStartSnapshot,
			frameEndSnapshot
		))
		{
			beginPendingUndoSnapshot(
				frameStartSnapshot
			);
		}

		commitPendingUndoSnapshot(
			frameEndSnapshot,
			editorStillActive
		);
	}

	m_skipUndoTrackingThisFrame = false;

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
