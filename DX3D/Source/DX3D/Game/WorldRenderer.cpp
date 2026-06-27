#include <DX3D/Game/WorldRenderer.h>
#include <DX3D/Graphics/GraphicsDevice.h>
#include <DX3D/Graphics/DeviceContext.h>
#include <DX3D/Graphics/SwapChain.h>
#include <DX3D/Graphics/VertexBuffer.h>
#include <DX3D/Graphics/IndexBuffer.h>
<<<<<<< Updated upstream
=======
#include <DX3D/Graphics/MeshData.h>
#include <DX3D/Graphics/PrimitiveMeshData.h>
#include <DX3D/Graphics/ShadowMap.h>
>>>>>>> Stashed changes

#include <DX3D/Game/World.h>
#include <DX3D/Game/Component.h>
#include <DX3D/Game/GameObject.h>

#include <DX3D/Component/TransformComponent.h>
#include <DX3D/Component/CubeComponent.h>
#include <DX3D/Component/CircleComponent.h>
#include <DX3D/Component/CameraComponent.h>
<<<<<<< Updated upstream
=======
#include <DX3D/Component/CombinedMeshComponent.h>
#include <DX3D/Component/DirectionalLightComponent.h>
>>>>>>> Stashed changes

#include <DX3D/Math/Vec3.h>
#include <fstream>
#include <ranges>
#include <vector>
#include <cmath>

dx3d::WorldRenderer::WorldRenderer(const WorldRendererDesc& desc)
	: Base(desc.base),
	m_graphicsDevice(desc.engine)
{
	auto& device = m_graphicsDevice;
	m_deviceContext = device.createDeviceContext();

	m_shadowMap =
		device.createShadowMap(
			{
				2048,
				2048
			}
		);

	constexpr char shaderFilePath[] = "DX3D/Assets/Shaders/Basic.hlsl";

	std::ifstream shaderStream(shaderFilePath);

	if (!shaderStream)
		DX3DLogThrowError("Failed to open shader file.");

	std::string shaderFileData{
		std::istreambuf_iterator<char>(shaderStream),
		std::istreambuf_iterator<char>()
	};

	auto shaderSourceCode = shaderFileData.c_str();
	auto shaderSourceCodeSize = shaderFileData.length();

	auto vs = device.compileShader(
		{
			shaderFilePath,
			shaderSourceCode,
			shaderSourceCodeSize,
			"VSMain",
			ShaderType::VertexShader
		}
	);

	auto ps = device.compileShader(
		{
			shaderFilePath,
			shaderSourceCode,
			shaderSourceCodeSize,
			"PSMain",
			ShaderType::PixelShader
		}
	);

	auto vsSig = device.createVertexShaderSignature({ vs });

	m_pipeline = device.createGraphicsPipelineState({ *vsSig, *ps });

	const Vertex vertexList[] =
	{
		{{-0.5f,-0.5f,-0.5f}, {1,0,0,1}},
		{{-0.5f,0.5f,-0.5f}, {0,1,0,1}},
		{{0.5f,0.5f,-0.5f},  {0,0,1,1}},
		{{0.5f,-0.5f,-0.5f}, {1,0,1,1}},

		{{0.5f,-0.5f,0.5f}, {1,0,1,1}},
		{{0.5f,0.5f,0.5f}, {0,0,1,1}},
		{{-0.5f,0.5f,0.5f}, {0,1,0,1}},
		{{-0.5f,-0.5f,0.5f}, {1,0,0,1}}
	};

	const ui32 indexList[] =
	{
		0,1,2,
		2,3,0,

		4,5,6,
		6,7,4,

		1,6,5,
		5,2,1,

		7,0,3,
		3,4,7,

		3,2,5,
		5,4,3,

		7,6,1,
		1,0,7
	};

	m_cubeVertexBuffer = device.createVertexBuffer(
		{ vertexList, std::size(vertexList), sizeof(Vertex) }
	);

	m_cb = device.createConstantBuffer(
		{ {}, sizeof(ConstantData) }
	);

	m_cubeIndexBuffer = device.createIndexBuffer(
		{ indexList, std::size(indexList) }
	);

	constexpr dx3d::ui32 circleSegments = 64;

	std::vector<Vertex> circleVertices;
	std::vector<dx3d::ui32> circleIndices;


	circleVertices.push_back(
		{
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f, 1.0f }
		}
	);


	for (dx3d::ui32 i = 0; i <= circleSegments; ++i)
	{
		const dx3d::f32 angle =
			(static_cast<dx3d::f32>(i) /
				static_cast<dx3d::f32>(circleSegments))
			* 2.0f
			* dx3d::MathUtils::PI;

		const dx3d::f32 x = std::cos(angle) * 0.5f;
		const dx3d::f32 y = std::sin(angle) * 0.5f;

		circleVertices.push_back(
			{
				{ x, y, 0.0f },
				{ 1.0f, 1.0f, 1.0f, 1.0f }
			}
		);
	}


	for (dx3d::ui32 i = 1; i <= circleSegments; ++i)
	{
		circleIndices.push_back(0);
		circleIndices.push_back(i + 1);
		circleIndices.push_back(i);
	}

	m_circleVertexBuffer = device.createVertexBuffer(
		{
			circleVertices.data(),
			static_cast<dx3d::ui32>(circleVertices.size()),
			sizeof(Vertex)
		}
	);

	m_circleIndexBuffer = device.createIndexBuffer(
		{
			circleIndices.data(),
			static_cast<dx3d::ui32>(circleIndices.size())
		}
	);
}

dx3d::WorldRenderer::~WorldRenderer()
{}

void dx3d::WorldRenderer::render(
	const World& world,
	SwapChain& swapChain,
	f32 deltaTime
)
{
	auto size = swapChain.getSize();

	auto& context = *m_deviceContext;

<<<<<<< Updated upstream
	context.clearAndSetBackBuffer(
		swapChain,
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	);

	context.setGraphicsPipelineState(*m_pipeline);
	context.setViewportSize(size);

=======
>>>>>>> Stashed changes
	auto numComponents = 0u;

	ConstantData data{};

<<<<<<< Updated upstream
=======
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

>>>>>>> Stashed changes
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
				lightComponent->getShadowArea();

			castShadows =
				lightComponent->getCastShadows();

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

	const f32 lightDistance = 15.0f;

	const Vec3 lightPosition
	{
		lightTarget.x +
			directionToLight.x * lightDistance,

		lightTarget.y +
			directionToLight.y * lightDistance,

		lightTarget.z +
			directionToLight.z * lightDistance
	};

	data.lightView =
		Mat4x4::lookAtLH(
			lightPosition,
			lightTarget,
			{
				0.0f,
				1.0f,
				0.0f
			}
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
<<<<<<< Updated upstream
			world.getComponents<CircleComponent>(numComponents);

		for (auto i : std::views::iota(0u, numComponents))
		{
			auto component = components[i];

			auto& transform =
				component->getGameObject().getTransform();

			data.world = transform.getAffineWorldMatrix();

			auto& cb = *m_cb;

			context.updateConstantBuffer(cb, &data);

			auto& vb = *m_circleVertexBuffer;
			auto& ib = *m_circleIndexBuffer;

			context.setVertexBuffer(vb);
			context.setConstantBuffer(cb);
			context.setIndexBuffer(ib);

			context.drawIndexedTriangleList(
				ib.getIndexListSize(),
				0u,
				0u
			);
		}
	}

	m_graphicsDevice.executeCommandList(context);
	swapChain.present();
=======
			world.getComponents<CameraComponent>(
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
			auto component = components[i];

			component->setViewportSize(size);

			cameraView =
				component->getViewMatrix();

			cameraProjection =
				component->getProjectionMatrix();

			break;
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
				m_graphicsDevice.createVertexBuffer(
					{
						meshData.vertices.data(),
						static_cast<ui32>(
							meshData.vertices.size()
						),
						sizeof(MeshVertex)
					}
				);

			renderResources.indexBuffer =
				m_graphicsDevice.createIndexBuffer(
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
					auto component =
						components[i];

					auto& transform =
						component->
						getGameObject().
						getTransform();

					data.world =
						transform.
						getAffineWorldMatrix();

					data.inverseWorld =
						Mat4x4::inverse(
							data.world
						);

					auto& cb = *m_cb;
					auto& vb =
						*m_cubeVertexBuffer;
					auto& ib =
						*m_cubeIndexBuffer;

					context.updateConstantBuffer(
						cb,
						&data
					);

					context.setVertexBuffer(vb);
					context.setIndexBuffer(ib);
					context.setConstantBuffer(cb);

					context.drawIndexedTriangleList(
						ib.getIndexListSize(),
						0u,
						0u
					);
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
					auto component =
						components[i];

					auto& transform =
						component->
						getGameObject().
						getTransform();

					data.world =
						transform.
						getAffineWorldMatrix();

					data.inverseWorld =
						Mat4x4::inverse(
							data.world
						);

					auto& cb = *m_cb;
					auto& vb =
						*m_planeVertexBuffer;
					auto& ib =
						*m_planeIndexBuffer;

					context.updateConstantBuffer(
						cb,
						&data
					);

					context.setVertexBuffer(vb);
					context.setIndexBuffer(ib);
					context.setConstantBuffer(cb);

					context.drawIndexedTriangleList(
						ib.getIndexListSize(),
						0u,
						0u
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
					auto component =
						components[i];

					auto& transform =
						component->
						getGameObject().
						getTransform();

					data.world =
						transform.
						getAffineWorldMatrix();

					data.inverseWorld =
						Mat4x4::inverse(
							data.world
						);

					auto& cb = *m_cb;
					auto& vb =
						*m_circleVertexBuffer;
					auto& ib =
						*m_circleIndexBuffer;

					context.updateConstantBuffer(
						cb,
						&data
					);

					context.setVertexBuffer(vb);
					context.setIndexBuffer(ib);
					context.setConstantBuffer(cb);

					context.drawIndexedTriangleList(
						ib.getIndexListSize(),
						0u,
						0u
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

					auto resourcesIterator =
						m_combinedMeshResources.find(
							component
						);

					if (
						resourcesIterator ==
						m_combinedMeshResources.end()
						)
					{
						continue;
					}

					auto& renderResources =
						resourcesIterator->second;

					if (
						!renderResources.vertexBuffer ||
						!renderResources.indexBuffer
						)
					{
						continue;
					}

					auto& transform =
						component->
						getGameObject().
						getTransform();

					data.world =
						transform.
						getAffineWorldMatrix();

					data.inverseWorld =
						Mat4x4::inverse(
							data.world
						);

					auto& cb = *m_cb;

					auto& vertexBuffer =
						*renderResources.vertexBuffer;

					auto& indexBuffer =
						*renderResources.indexBuffer;

					context.updateConstantBuffer(
						cb,
						&data
					);

					context.setVertexBuffer(
						vertexBuffer
					);

					context.setIndexBuffer(
						indexBuffer
					);

					context.setConstantBuffer(cb);

					context.drawIndexedTriangleList(
						indexBuffer.
						getIndexListSize(),
						0u,
						0u
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

			data.view = data.lightView;
			data.proj = data.lightProj;

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

	context.setViewportSize(size);

	context.setShadowMap(
		*m_shadowMap
	);

	data.view = cameraView;
	data.proj = cameraProjection;

	drawSceneGeometry();

	m_graphicsDevice.executeCommandList(
		context
	);
>>>>>>> Stashed changes
}