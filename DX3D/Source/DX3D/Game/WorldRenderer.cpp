#include <DX3D/Game/WorldRenderer.h>

#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/DeviceContext.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/VertexBuffer.h>
#include <DX3D/Graphics/IndexBuffer.h>
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Graphics/ShadowMap.h>

#include <DX3D/Game/World.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/PlaneComponent.h>
#include <DX3D/Component/CircleComponent.h>
#include <DX3D/Component/SphereComponent.h>
#include <DX3D/Component/CylinderComponent.h>
#include <DX3D/Component/CapsuleComponent.h>
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>
#include <DX3D/Component/TextureComponent.h>

#include <DX3D/Math/MathUtils.h>

#include <fstream>
#include <ranges>
#include <vector>
#include <cmath>
#include <unordered_set>
#include <filesystem>
#include <limits>
#include <wincodec.h>

dx3d::WorldRenderer::TextureRenderResource*
dx3d::WorldRenderer::getTextureResource(const std::string& path)
{
	if (path.empty()) return nullptr;
	if (auto found = m_textureResources.find(path); found != m_textureResources.end())
		return &found->second;
	if (m_failedTexturePaths.contains(path)) return nullptr;

	const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
	{
		m_failedTexturePaths.insert(path);
		return nullptr;
	}
	Microsoft::WRL::ComPtr<IWICImagingFactory> factory{};
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder{};
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame{};
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter{};
	UINT width = 0, height = 0;
	const std::wstring widePath = std::filesystem::path(path).wstring();
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory))) ||
		FAILED(factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnDemand, &decoder)) ||
		FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) ||
		width == 0 || height == 0 || FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		m_failedTexturePaths.insert(path);
		DX3DLogWarning("Could not load texture: {}", path);
		return nullptr;
	}
	const size_t byteCount = static_cast<size_t>(width) * height * 4u;
	if (byteCount > std::numeric_limits<UINT>::max()) return nullptr;
	std::vector<unsigned char> pixels(byteCount);
	if (FAILED(converter->CopyPixels(nullptr, width * 4u,
		static_cast<UINT>(byteCount), pixels.data()))) return nullptr;

	TextureRenderResource resource{};
	D3D11_TEXTURE2D_DESC description{};
	description.Width = width;
	description.Height = height;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initial{ pixels.data(), width * 4u, 0 };
	auto* device = m_graphicsDevice.getD3DDevice();
	if (!device || FAILED(device->CreateTexture2D(&description, &initial, &resource.texture)) ||
		FAILED(device->CreateShaderResourceView(resource.texture.Get(), nullptr, &resource.view)))
		return nullptr;
	D3D11_SAMPLER_DESC sampler{};
	sampler.Filter = D3D11_FILTER_ANISOTROPIC;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler.MaxAnisotropy = 8;
	sampler.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(device->CreateSamplerState(&sampler, &resource.sampler))) return nullptr;
	auto [inserted, unused] = m_textureResources.emplace(path, std::move(resource));
	DX3DLogInfo("Loaded texture: {}", path);
	return &inserted->second;
}

dx3d::WorldRenderer::WorldRenderer(
	const WorldRendererDesc& desc
)
	: Base(desc.base),
	m_graphicsDevice(desc.engine)
{
	auto& device =
		m_graphicsDevice;

	m_deviceContext =
		device.createDeviceContext();

	m_shadowMap =
		device.createShadowMap(
			{
				2048,
				2048
			}
		);
	m_pointShadowMap =
		device.createShadowMap(
			{
				1024,
				1024,
				true
			}
		);

	constexpr char shaderFilePath[] =
		"DX3D/Assets/Shaders/Basic.hlsl";

	std::ifstream shaderStream(
		shaderFilePath
	);

	if (!shaderStream)
	{
		DX3DLogThrowError(
			"Failed to open shader file."
		);
	}

	std::string shaderFileData
	{
		std::istreambuf_iterator<char>(
			shaderStream
		),
		std::istreambuf_iterator<char>()
	};

	const char* shaderSourceCode =
		shaderFileData.c_str();

	const size_t shaderSourceCodeSize =
		shaderFileData.length();

	auto vertexShader =
		device.compileShader(
			{
				shaderFilePath,
				shaderSourceCode,
				shaderSourceCodeSize,
				"VSMain",
				ShaderType::VertexShader
			}
		);

	auto pixelShader =
		device.compileShader(
			{
				shaderFilePath,
				shaderSourceCode,
				shaderSourceCodeSize,
				"PSMain",
				ShaderType::PixelShader
			}
		);

	auto vertexSignature =
		device.createVertexShaderSignature(
			{ vertexShader }
		);

	m_pipeline =
		device.createGraphicsPipelineState(
			{
				*vertexSignature,
				*pixelShader
			}
		);

	const auto& cubeMesh =
		getCubeMeshData();

	m_cubeVertexBuffer =
		device.createVertexBuffer(
			{
				cubeMesh.vertices.data(),
				static_cast<ui32>(
					cubeMesh.vertices.size()
				),
				sizeof(MeshVertex)
			}
		);

	m_cubeIndexBuffer =
		device.createIndexBuffer(
			{
				cubeMesh.indices.data(),
				static_cast<ui32>(
					cubeMesh.indices.size()
				)
			}
		);

	const auto& planeMesh =
		getPlaneMeshData();

	m_planeVertexBuffer =
		device.createVertexBuffer(
			{
				planeMesh.vertices.data(),
				static_cast<ui32>(
					planeMesh.vertices.size()
				),
				sizeof(MeshVertex)
			}
		);

	m_planeIndexBuffer =
		device.createIndexBuffer(
			{
				planeMesh.indices.data(),
				static_cast<ui32>(
					planeMesh.indices.size()
				)
			}
		);

	const auto& sphereMesh = getSphereMeshData();
	m_sphereVertexBuffer = device.createVertexBuffer({ sphereMesh.vertices.data(),
		static_cast<ui32>(sphereMesh.vertices.size()), sizeof(MeshVertex) });
	m_sphereIndexBuffer = device.createIndexBuffer({ sphereMesh.indices.data(),
		static_cast<ui32>(sphereMesh.indices.size()) });

	const auto& cylinderMesh = getCylinderMeshData();
	m_cylinderVertexBuffer = device.createVertexBuffer({ cylinderMesh.vertices.data(),
		static_cast<ui32>(cylinderMesh.vertices.size()), sizeof(MeshVertex) });
	m_cylinderIndexBuffer = device.createIndexBuffer({ cylinderMesh.indices.data(),
		static_cast<ui32>(cylinderMesh.indices.size()) });

	const auto& capsuleMesh = getCapsuleMeshData();
	m_capsuleVertexBuffer = device.createVertexBuffer({ capsuleMesh.vertices.data(),
		static_cast<ui32>(capsuleMesh.vertices.size()), sizeof(MeshVertex) });
	m_capsuleIndexBuffer = device.createIndexBuffer({ capsuleMesh.indices.data(),
		static_cast<ui32>(capsuleMesh.indices.size()) });

	constexpr ui32 circleSegments = 64;

	std::vector<MeshVertex>
		circleVertices{};

	std::vector<ui32>
		circleIndices{};

	circleVertices.push_back(
		{
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			{ 0.0f, 0.0f, -1.0f }
		}
	);

	for (
		ui32 i = 0;
		i <= circleSegments;
		++i
		)
	{
		const f32 angle =
			(
				static_cast<f32>(i) /
				static_cast<f32>(
					circleSegments
					)
				) *
			2.0f *
			MathUtils::PI;

		const f32 x =
			std::cos(angle) *
			0.5f;

		const f32 y =
			std::sin(angle) *
			0.5f;

		circleVertices.push_back(
			{
				{ x, y, 0.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				{ 0.0f, 0.0f, -1.0f }
			}
		);
	}

	for (
		ui32 i = 1;
		i <= circleSegments;
		++i
		)
	{
		circleIndices.push_back(0);
		circleIndices.push_back(i + 1);
		circleIndices.push_back(i);
	}

	m_circleVertexBuffer =
		device.createVertexBuffer(
			{
				circleVertices.data(),
				static_cast<ui32>(
					circleVertices.size()
				),
				sizeof(MeshVertex)
			}
		);

	m_circleIndexBuffer =
		device.createIndexBuffer(
			{
				circleIndices.data(),
				static_cast<ui32>(
					circleIndices.size()
				)
			}
		);

	m_cb =
		device.createConstantBuffer(
			{
				{},
				sizeof(ConstantData)
			}
		);
}

dx3d::WorldRenderer::~WorldRenderer()
{}

void dx3d::WorldRenderer::render(
	const World& world,
	SwapChain& swapChain,
	f32 deltaTime,
	bool useSceneCamera
)
{
	const Rect size =
		swapChain.getSize();

	auto& context =
		*m_deviceContext;

	ui32 numComponents = 0;

	ConstantData data{};

	Vec3 directionToLight =
		Vec3::normalize(
			{
				-0.5f,
				1.0f,
				-0.3f
			}
		);

	f32 ambientStrength = 0.03f;
	f32 shadowArea = 30.0f;
	f32 shadowRange = 50.0f;
	f32 shadowSpotAngle = 45.0f;
	f32 shadowNearPlane = 0.05f;
	LightType shadowLightType = LightType::Directional;
	Vec3 shadowLightPosition{};
	Vec3 shadowLightForward{ 0.0f, 0.0f, 1.0f };
	bool castShadows = false;
	int shadowLightIndex = -1;
	ui32 lightCount = 0;

	{
		auto components =
			world.getComponents<
			DirectionalLightComponent
			>(
				numComponents
			);

		for (
			auto i :
			std::views::iota(
				0u,
				numComponents
			)
			)
		{
			auto* lightComponent =
				components[i];

			if (!lightComponent ||
				!lightComponent->getGameObject().isActiveInHierarchy())
				continue;

			if (lightCount >= MaxLights)
				break;

			auto& lightTransform =
				lightComponent->
				getGameObject().
				getTransform();

			const Vec3 lightForward =
				lightTransform.forward();

			const Vec3 lightColor = lightComponent->getColor();
			const Vec3 lightPosition = lightTransform.getPosition();
			const f32 spotCosine = std::cos(
				lightComponent->getSpotAngle() * 0.5f * 3.1415926535f / 180.0f);
			data.lightDirections[lightCount] = {
				lightForward.x, lightForward.y, lightForward.z,
				lightComponent->getIntensity() };
			data.lightColors[lightCount] = {
				lightColor.x, lightColor.y, lightColor.z, 0.0f };
			data.lightPositions[lightCount] = {
				lightPosition.x, lightPosition.y, lightPosition.z,
				lightComponent->getRange() };
			const f32 pointNearPlane = std::clamp(
				lightComponent->getRange() * 0.01f, 0.01f, 0.10f);
			data.lightParameters[lightCount] = {
				static_cast<f32>(lightComponent->getLightType()), spotCosine,
				pointNearPlane, 0.0f };
			ambientStrength = std::max(
				ambientStrength, lightComponent->getAmbientStrength());

			if (shadowLightIndex < 0 && lightComponent->getCastShadows())
			{
				shadowLightType = lightComponent->getLightType();
				shadowLightPosition = lightPosition;
				shadowLightForward = Vec3::normalize(lightForward);
				shadowArea = lightComponent->getShadowArea();
				shadowRange = std::max(lightComponent->getRange(), pointNearPlane + 0.01f);
				shadowSpotAngle = lightComponent->getSpotAngle();
				shadowNearPlane = pointNearPlane;
				if (shadowLightType == LightType::Directional)
				{
					directionToLight = Vec3::normalize({
						-lightForward.x, -lightForward.y, -lightForward.z });
				}
				castShadows = true;
				shadowLightIndex = static_cast<int>(lightCount);
			}
			++lightCount;
		}
	}

	if (lightCount == 0)
	{
		data.lightDirections[0] = {
			-directionToLight.x, -directionToLight.y, -directionToLight.z, 1.0f };
		data.lightColors[0] = { 1.0f, 1.0f, 1.0f, 0.0f };
		data.lightPositions[0] = { 0.0f, 0.0f, 0.0f, 100.0f };
		data.lightParameters[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
		lightCount = 1;
		ambientStrength = 0.20f;
		castShadows = true;
		shadowLightIndex = 0;
	}
	data.lightMeta = {
		static_cast<f32>(lightCount), ambientStrength,
		static_cast<f32>(shadowLightIndex), castShadows ? 1.0f : 0.0f };

	if (shadowLightType == LightType::Directional)
	{
		const Vec3 lightTarget{};
		const f32 lightDistance = 15.0f;
		shadowLightPosition = lightTarget + directionToLight * lightDistance;
		const Vec3 lightUp = std::fabs(directionToLight.y) > 0.99f
			? Vec3{ 0.0f, 0.0f, 1.0f }
			: Vec3{ 0.0f, 1.0f, 0.0f };
		data.lightView = Mat4x4::lookAtLH(
			shadowLightPosition, lightTarget, lightUp);
		data.lightProj = Mat4x4::orthoLH(
			shadowArea, shadowArea, 0.1f, 50.0f);
	}
	else
	{
		const Vec3 lightTarget = shadowLightPosition + shadowLightForward;
		const Vec3 lightUp = std::fabs(shadowLightForward.y) > 0.99f
			? Vec3{ 0.0f, 0.0f, 1.0f }
			: Vec3{ 0.0f, 1.0f, 0.0f };
		data.lightView = Mat4x4::lookAtLH(
			shadowLightPosition, lightTarget, lightUp);
		const f32 shadowFov = shadowLightType == LightType::Spot
			? std::clamp(shadowSpotAngle * MathUtils::PI / 180.0f,
				0.02f, MathUtils::PI - 0.02f)
			: MathUtils::PI * 0.5f;
		data.lightProj = Mat4x4::perspectiveFovLH(
			shadowFov, 1.0f, shadowNearPlane, shadowRange);
	}

	Mat4x4 cameraView =
		Mat4x4::identity();

	Mat4x4 cameraProjection =
		Mat4x4::identity();

	{
		auto components =
			world.getComponents<
			CameraComponent
			>(
				numComponents
				);

		CameraComponent* chosenCamera = nullptr;

		for (
			auto i :
			std::views::iota(
				0u,
				numComponents
			)
			)
		{
			auto* component =
				components[i];

			if (!component ||
				!component->getGameObject().isActiveInHierarchy())
				continue;

			const bool editorCamera = component->getGameObject().getName() == "Editor Camera";
			if ((!useSceneCamera && editorCamera) ||
				(useSceneCamera && !editorCamera && component->isPrimary()))
			{
				chosenCamera = component;
				break;
			}
			if (!chosenCamera && (useSceneCamera ? !editorCamera : editorCamera))
				chosenCamera = component;
		}
		if (chosenCamera)
		{
			chosenCamera->setViewportSize(size);
			cameraView = chosenCamera->getViewMatrix();
			cameraProjection = chosenCamera->getProjectionMatrix();
		}
	}

	std::unordered_set<
		const CombinedMeshComponent*
	> activeCombinedMeshes{};

	{
		auto components =
			world.getComponents<
			CombinedMeshComponent
			>(
				numComponents
			);

		activeCombinedMeshes.reserve(
			numComponents
		);

		for (
			auto i :
			std::views::iota(
				0u,
				numComponents
			)
			)
		{
			auto* component =
				components[i];

			if (!component)
				continue;

			activeCombinedMeshes.insert(
				component
			);

			if (!component->hasMeshData())
			{
				m_combinedMeshResources.erase(
					component
				);

				continue;
			}

			const auto& meshData =
				component->getMeshData();

			auto& renderResources =
				m_combinedMeshResources[
					component
				];

			const bool needsBufferRebuild =
				!renderResources.vertexBuffer ||
				!renderResources.indexBuffer ||
				renderResources.meshRevision !=
				component->getMeshRevision();

			if (!needsBufferRebuild)
				continue;

			renderResources.vertexBuffer =
				m_graphicsDevice.
				createVertexBuffer(
					{
						meshData.vertices.data(),
						static_cast<ui32>(
							meshData.vertices.size()
						),
						sizeof(MeshVertex)
					}
				);

			renderResources.indexBuffer =
				m_graphicsDevice.
				createIndexBuffer(
					{
						meshData.indices.data(),
						static_cast<ui32>(
							meshData.indices.size()
						)
					}
				);

			renderResources.meshRevision =
				component->getMeshRevision();
		}
	}

	for (
		auto it =
		m_combinedMeshResources.begin();

		it !=
		m_combinedMeshResources.end();
		)
	{
		if (
			activeCombinedMeshes.find(
				it->first
			) ==
			activeCombinedMeshes.end()
			)
		{
			it =
				m_combinedMeshResources.erase(
					it
				);
		}
		else
		{
			++it;
		}
	}

	auto drawObject =
		[&](
			GameObject& object,
			VertexBuffer& vertexBuffer,
			IndexBuffer& indexBuffer
		)
		{
			if (!object.isActiveInHierarchy())
				return;

			auto& transform = object.getTransform();
			data.materialAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			data.materialEmissiveAndStrength = {};
			data.materialParameters = {};
			context.setAlbedoTexture(nullptr, nullptr);

			if (auto* material = object.getComponent<MaterialComponent>())
			{
				data.materialAlbedo = material->getAlbedo();
				const Vec3 emissive = material->getEmissive();
				data.materialEmissiveAndStrength =
				{
					emissive.x, emissive.y, emissive.z,
					material->getEmissionStrength()
				};
				data.materialParameters.x = static_cast<f32>(material->getMode());
			}
			if (auto* texture = object.getComponent<TextureComponent>();
				texture && texture->isEnabled() && !texture->getAssetPath().empty())
			{
				if (auto* resource = getTextureResource(texture->getAssetPath()))
				{
					data.materialParameters.y = 1.0f;
					context.setAlbedoTexture(resource->view.Get(), resource->sampler.Get());
				}
			}

			data.world =
				transform.getAffineWorldMatrix();

			data.inverseWorld =
				Mat4x4::inverse(
					data.world
				);

			auto& constantBuffer =
				*m_cb;

			context.updateConstantBuffer(
				constantBuffer,
				&data
			);

			context.setVertexBuffer(
				vertexBuffer
			);

			context.setIndexBuffer(
				indexBuffer
			);

			context.setConstantBuffer(
				constantBuffer
			);

			context.drawIndexedTriangleList(
				indexBuffer.getIndexListSize(),
				0u,
				0u
			);
		};

	auto drawSceneGeometry =
		[&]()
		{
			{
				auto components =
					world.getComponents<
					CubeComponent
					>(
						numComponents
					);

				for (
					auto i :
					std::views::iota(
						0u,
						numComponents
					)
					)
				{
					auto* component =
						components[i];

					if (!component)
						continue;

					drawObject(
						component->getGameObject(),
						*m_cubeVertexBuffer,
						*m_cubeIndexBuffer
					);
				}
			}
			{
				auto components = world.getComponents<SphereComponent>(numComponents);
				for (auto i : std::views::iota(0u, numComponents))
				{
					auto* component = components[i];
					if (component) drawObject(component->getGameObject(),
						*m_sphereVertexBuffer, *m_sphereIndexBuffer);
				}
			}

			{
				auto components = world.getComponents<CylinderComponent>(numComponents);
				for (auto i : std::views::iota(0u, numComponents))
					if (auto* component = components[i]) drawObject(component->getGameObject(),
						*m_cylinderVertexBuffer, *m_cylinderIndexBuffer);
			}

			{
				auto components = world.getComponents<CapsuleComponent>(numComponents);
				for (auto i : std::views::iota(0u, numComponents))
					if (auto* component = components[i]) drawObject(component->getGameObject(),
						*m_capsuleVertexBuffer, *m_capsuleIndexBuffer);
			}

			{
				auto components =
					world.getComponents<
					PlaneComponent
					>(
						numComponents
					);

				for (
					auto i :
					std::views::iota(
						0u,
						numComponents
					)
					)
				{
					auto* component =
						components[i];

					if (!component)
						continue;

					drawObject(
						component->getGameObject(),
						*m_planeVertexBuffer,
						*m_planeIndexBuffer
					);
				}
			}

			{
				auto components =
					world.getComponents<
					CircleComponent
					>(
						numComponents
					);

				for (
					auto i :
					std::views::iota(
						0u,
						numComponents
					)
					)
				{
					auto* component =
						components[i];

					if (!component)
						continue;

					drawObject(
						component->getGameObject(),
						*m_circleVertexBuffer,
						*m_circleIndexBuffer
					);
				}
			}

			{
				auto components =
					world.getComponents<
					CombinedMeshComponent
					>(
						numComponents
					);

				for (
					auto i :
					std::views::iota(
						0u,
						numComponents
					)
					)
				{
					auto* component =
						components[i];

					if (
						!component ||
						!component->hasMeshData()
						)
					{
						continue;
					}

					auto resourceIterator =
						m_combinedMeshResources.find(
							component
						);

					if (
						resourceIterator ==
						m_combinedMeshResources.end()
						)
					{
						continue;
					}

					auto& renderResources =
						resourceIterator->second;

					if (
						!renderResources.vertexBuffer ||
						!renderResources.indexBuffer
						)
					{
						continue;
					}

					drawObject(
						component->getGameObject(),
						*renderResources.vertexBuffer,
						*renderResources.indexBuffer
					);
				}
			}
		};

	if (castShadows && !useSceneCamera)
	{
		context.setGraphicsPipelineState(
			*m_pipeline
		);

		if (shadowLightType == LightType::Point)
		{
			const Vec3 faceDirections[6]
			{
				{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }
			};
			const Vec3 faceUp[6]
			{
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
				{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
			};

			for (ui32 face = 0; face < 6; ++face)
			{
				context.beginShadowPass(*m_pointShadowMap, face);
				data.lightView = Mat4x4::lookAtLH(
					shadowLightPosition,
					shadowLightPosition + faceDirections[face],
					faceUp[face]);
				data.view = data.lightView;
				data.proj = data.lightProj;
				drawSceneGeometry();
			}
		}
		else
		{
			context.beginShadowPass(*m_shadowMap);
			data.view = data.lightView;
			data.proj = data.lightProj;
			drawSceneGeometry();
		}
	}

	context.clearAndSetBackBuffer(
		swapChain,
		useSceneCamera
			? Vec4{ 0.094f, 0.129f, 0.169f, 1.0f }
			: Vec4{ 0.20f, 0.298f, 0.40f, 1.0f }
	);

	context.setGraphicsPipelineState(
		*m_pipeline
	);

	context.setViewportSize(
		size
	);

	context.setShadowMap(
		*m_shadowMap
	);
	context.setPointShadowMap(
		*m_pointShadowMap
	);

	data.view =
		cameraView;

	data.proj =
		cameraProjection;

	drawSceneGeometry();

	m_graphicsDevice.executeCommandList(
		context
	);
}
