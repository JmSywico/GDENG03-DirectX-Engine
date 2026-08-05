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
#include <DX3D/Component/CameraComponent.h>
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
#include <DX3D/Component/MaterialComponent.h>

#include <DX3D/Math/MathUtils.h>

#include <fstream>
#include <ranges>
#include <vector>
#include <cmath>
#include <unordered_set>

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

	Vec3 lightColor
	{
		1.0f,
		1.0f,
		1.0f
	};

	f32 lightIntensity = 1.0f;
	f32 ambientStrength = 0.20f;
	f32 shadowArea = 30.0f;

	bool castShadows = true;

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

			if (!lightComponent)
				continue;

			auto& lightTransform =
				lightComponent->
				getGameObject().
				getTransform();

			const Vec3 lightForward =
				lightTransform.forward();

			directionToLight =
				Vec3::normalize(
					{
						-lightForward.x,
						-lightForward.y,
						-lightForward.z
					}
				);

			lightColor =
				lightComponent->getColor();

			lightIntensity =
				lightComponent->getIntensity();

			ambientStrength =
				lightComponent->
				getAmbientStrength();

			shadowArea =
				lightComponent->
				getShadowArea();

			castShadows =
				lightComponent->
				getCastShadows();

			break;
		}
	}

	data.lightDirection =
	{
		directionToLight.x,
		directionToLight.y,
		directionToLight.z,
		castShadows ? 1.0f : 0.0f
	};

	data.lightColorAndAmbient =
	{
		lightColor.x * lightIntensity,
		lightColor.y * lightIntensity,
		lightColor.z * lightIntensity,
		ambientStrength
	};

	const Vec3 lightTarget
	{
		0.0f,
		0.0f,
		0.0f
	};

	const f32 lightDistance =
		15.0f;

	const Vec3 lightPosition
	{
		lightTarget.x +
			directionToLight.x *
				lightDistance,

		lightTarget.y +
			directionToLight.y *
				lightDistance,

		lightTarget.z +
			directionToLight.z *
				lightDistance
	};

	const Vec3 lightUp =
		std::fabs(
			directionToLight.y
		) > 0.99f
		?
		Vec3{ 0.0f, 0.0f, 1.0f }
		:
		Vec3{ 0.0f, 1.0f, 0.0f };

	data.lightView =
		Mat4x4::lookAtLH(
			lightPosition,
			lightTarget,
			lightUp
		);

	data.lightProj =
		Mat4x4::orthoLH(
			shadowArea,
			shadowArea,
			0.1f,
			50.0f
		);

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

			if (!component)
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
			auto& transform = object.getTransform();
			data.materialAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			data.materialEmissiveAndStrength = {};
			data.materialParameters = {};

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

	if (castShadows)
	{
		context.beginShadowPass(
			*m_shadowMap
		);

		context.setGraphicsPipelineState(
			*m_pipeline
		);

		data.view =
			data.lightView;

		data.proj =
			data.lightProj;

		drawSceneGeometry();
	}

	context.clearAndSetBackBuffer(
		swapChain,
		{
			0.08f,
			0.10f,
			0.14f,
			1.0f
		}
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

	data.view =
		cameraView;

	data.proj =
		cameraProjection;

	drawSceneGeometry();

	m_graphicsDevice.executeCommandList(
		context
	);
}
